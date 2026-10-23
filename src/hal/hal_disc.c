/**
 * @file hal_disc.c
 * @brief FiiO Snowsky DISC Hardware Backend (Ingenic X2000 MIPS32 Linux 4.4)
 *
 * Verified against reverse-engineered Snowsky DISC hardware specifications:
 *   1. Display (`/dev/fb0`, `ingenicfb`): 360x360 visible (360x1080 triple-buffered),
 *      32 bpp XRGB8888, physically mounted 180 degrees rotated in the chassis.
 *      Clears all pages on init, pans to page 0 (`FB_ACTIVATE_NOW`), and blits
 *      180-degree rotated XRGB8888 pixels into the active visible page.
 *   2. Capacitive Touch (`/dev/input/event1`, `cst816t`): Multitouch Type-B
 *      (`ABS_MT_TRACKING_ID`, `ABS_MT_POSITION_X/Y`) with 180-degree coordinate
 *      inversion (`359 - x`, `359 - y`).
 *   3. Physical Buttons (`/dev/input/event0`, `x2000_key`): GPIO keys (`KEY_POWER`,
 *      `KEY_PLAYPAUSE`, `KEY_VOLUMEUP`, `KEY_VOLUMEDOWN`).
 *   4. Stock Watchdog Survival (`fiio_init.sh`): Sets process comm name to `mq_ui`
 *      via `prctl(PR_SET_NAME, "mq_ui")` so `pgrep -x mq_ui` succeeds.
 *   5. Audio Daemon IPC (`/player` & `/ui` POSIX mqueues): Encodes commands into
 *      FiiO wire frames (`TAG(4) + TOTAL_LEN(4hex) + VALUE1(4hex)`).
 */
#define _POSIX_C_SOURCE 200809L
#include "dial_hal.h"
#include "dial_assets.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/fb.h>
#include <linux/input.h>
#include <mqueue.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#endif

static dial_fb_t g_disc_fb;
static int       g_fb_fd       = -1;
static uint8_t  *g_fb_mmap     = NULL;
static size_t    g_fb_bytes    = DIAL_SCREEN_WIDTH * DIAL_SCREEN_HEIGHT * sizeof(uint32_t);
static size_t    g_fb_vis_off  = 0;
static uint32_t  g_fb_bpp      = 32;
static uint32_t  g_fb_stride_px= DIAL_SCREEN_WIDTH;
static int       g_touch_fd    = -1;
static int       g_keys_fd     = -1;
static float     g_last_raw_tx = 180.0f;
static float     g_last_raw_ty = 180.0f;
static bool      g_touch_dn    = false;

static uint32_t       g_haptic_count   = 0;
static uint16_t       g_last_haptic_ms = 0;
static dial_ipc_msg_t g_ipc_ring[32];
static size_t         g_ipc_head  = 0;
static size_t         g_ipc_tail  = 0;
static size_t         g_ipc_count = 0;
static char           g_last_wire_frame[160];

#if defined(__linux__)
static mqd_t                    g_mq_player = (mqd_t)-1;
static mqd_t                    g_mq_ui     = (mqd_t)-1;
static struct fb_var_screeninfo g_fb_vinfo;
static struct fb_fix_screeninfo g_fb_finfo;
#endif

bool dial_hal_init(bool headless) {
    memset(&g_disc_fb, 0, sizeof(g_disc_fb));
    g_disc_fb.width  = DIAL_SCREEN_WIDTH;
    g_disc_fb.height = DIAL_SCREEN_HEIGHT;

#if defined(__linux__)
    /* Patch #3: Satisfy /usr/project/fiio_init.sh watchdog (`pgrep -x mq_ui`) */
    (void)prctl(PR_SET_NAME, "mq_ui", 0, 0, 0);
#endif

    /* Support local virtual Snowsky DISC `/dev/fb0` file when DIAL_VIRTUAL_FB0 is set */
    const char *vfb_path = getenv("DIAL_VIRTUAL_FB0");
    if (vfb_path && *vfb_path) {
        g_fb_fd = open(vfb_path, O_RDWR | O_CREAT, 0644);
        if (g_fb_fd >= 0) {
            g_fb_bytes     = (size_t)DIAL_SCREEN_WIDTH * (size_t)DIAL_SCREEN_HEIGHT * 3u * sizeof(uint32_t);
            g_fb_bpp       = 32;
            g_fb_stride_px = DIAL_SCREEN_WIDTH;
            g_fb_vis_off   = 0;
            (void)ftruncate(g_fb_fd, (off_t)g_fb_bytes);
            void *map = mmap(NULL, g_fb_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb_fd, 0);
            if (map != MAP_FAILED) {
                g_fb_mmap = (uint8_t *)map;
                memset(g_fb_mmap, 0, g_fb_bytes);
            }
        }
    }

    if (headless) {
        return true;
    }

#if defined(__linux__)
    if (g_fb_fd < 0) {
        g_fb_fd = open(DIAL_DEV_FB0, O_RDWR);
        if (g_fb_fd >= 0) {
            memset(&g_fb_vinfo, 0, sizeof(g_fb_vinfo));
            memset(&g_fb_finfo, 0, sizeof(g_fb_finfo));
            if (ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &g_fb_vinfo) == 0 &&
                ioctl(g_fb_fd, FBIOGET_FSCREENINFO, &g_fb_finfo) == 0) {
                g_fb_bpp       = g_fb_vinfo.bits_per_pixel ? g_fb_vinfo.bits_per_pixel : 32u;
                g_fb_bytes     = g_fb_finfo.smem_len ? (size_t)g_fb_finfo.smem_len
                                                     : (size_t)g_fb_vinfo.xres * (size_t)g_fb_vinfo.yres * (g_fb_bpp / 8u);
                g_fb_stride_px = (g_fb_finfo.line_length > 0 && g_fb_bpp >= 8)
                                     ? (g_fb_finfo.line_length / (g_fb_bpp / 8u))
                                     : g_fb_vinfo.xres;
            }
            void *map = mmap(NULL, g_fb_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb_fd, 0);
            if (map != MAP_FAILED) {
                g_fb_mmap = (uint8_t *)map;
                /* Clear all 3 framebuffer pages upfront so stale stock splash never bleeds through */
                memset(g_fb_mmap, 0, g_fb_bytes);

                /* Pan to page 0 (FB_ACTIVATE_NOW) and read back actual visible yoffset */
                g_fb_vinfo.xoffset  = 0;
                g_fb_vinfo.yoffset  = 0;
                g_fb_vinfo.activate = FB_ACTIVATE_NOW;
                (void)ioctl(g_fb_fd, FBIOPAN_DISPLAY, &g_fb_vinfo);
                (void)ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &g_fb_vinfo);
                g_fb_vis_off = (size_t)g_fb_vinfo.yoffset * (size_t)g_fb_finfo.line_length;
            }
        }
    }

    /* Patch #2: event1 = cst816t touch, event0 = x2000_key physical buttons */
    g_touch_fd = open(DIAL_DEV_TOUCH, O_RDONLY | O_NONBLOCK);
    g_keys_fd  = open(DIAL_DEV_KEYS,  O_RDONLY | O_NONBLOCK);

    g_mq_player = mq_open(DIAL_MQ_PLAYER, O_WRONLY | O_NONBLOCK);
    g_mq_ui     = mq_open(DIAL_MQ_UI,     O_RDONLY | O_NONBLOCK);

    /* Request initial state snapshot ("02020008") from mq_player on boot */
    dial_hal_send_ipc(DIAL_CMD_GET_STATE, "");
#endif
    return true;
}

void dial_hal_shutdown(void) {
#if defined(__linux__)
    if (g_mq_player != (mqd_t)-1) { mq_close(g_mq_player); g_mq_player = (mqd_t)-1; }
    if (g_mq_ui     != (mqd_t)-1) { mq_close(g_mq_ui);     g_mq_ui     = (mqd_t)-1; }
#endif
    if (g_fb_mmap) {
        munmap(g_fb_mmap, g_fb_bytes);
        g_fb_mmap = NULL;
    }
    if (g_fb_fd >= 0)    { close(g_fb_fd);    g_fb_fd    = -1; }
    if (g_touch_fd >= 0) { close(g_touch_fd); g_touch_fd = -1; }
    if (g_keys_fd >= 0)  { close(g_keys_fd);  g_keys_fd  = -1; }
}

dial_fb_t *dial_hal_get_framebuffer(void) {
    return &g_disc_fb;
}

void dial_hal_present(dial_fb_t *fb) {
    if (!fb) return;
    const size_t total = (size_t)fb->width * (size_t)fb->height;
    for (size_t i = 0; i < total; ++i) {
        fb->rgb565[i] = dial_rgb888_to_rgb565(fb->pixels[i]);
    }
    if (g_fb_mmap) {
        uint8_t *vis_base = g_fb_mmap + g_fb_vis_off;
        if (g_fb_bpp == 32) {
            /* Patch #1: 32 bpp XRGB8888 180-degree rotated blit for ingenicfb panel mounting */
            dial_disc_blit_rotated_xrgb8888((uint32_t *)vis_base,
                                            g_fb_stride_px,
                                            fb->pixels,
                                            fb->width,
                                            fb->height);
        } else {
            /* 16 bpp RGB565 180-degree rotated fallback */
            uint16_t *dst16 = (uint16_t *)vis_base;
            for (uint32_t y = 0; y < fb->height; ++y) {
                const uint16_t *srow = fb->rgb565 + (size_t)y * fb->width;
                uint16_t *drow = dst16 + (size_t)(fb->height - 1u - y) * g_fb_stride_px;
                for (uint32_t x = 0; x < fb->width; ++x) {
                    drow[fb->width - 1u - x] = srow[x];
                }
            }
        }
    }
}

int dial_hal_save_png(const dial_fb_t *fb, const char *filepath) {
    if (!fb || !filepath) return -1;
    FILE *fp = fopen(filepath, "wb");
    if (!fp) return -2;
    fwrite(fb->rgb565, sizeof(uint16_t), (size_t)fb->width * (size_t)fb->height, fp);
    fclose(fp);
    return 0;
}

uint32_t dial_hal_ticks_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL);
}

void dial_hal_delay_ms(uint32_t ms) {
    struct timespec req;
    req.tv_sec  = (time_t)(ms / 1000u);
    req.tv_nsec = (long)((ms % 1000u) * 1000000UL);
    nanosleep(&req, NULL);
}

bool dial_hal_poll_events(dial_touch_event_t *out_touch,
                          bool *has_touch,
                          dial_key_event_t *out_key,
                          bool *has_key) {
    if (has_touch) *has_touch = false;
    if (has_key)   *has_key   = false;
    (void)g_last_raw_tx;
    (void)g_last_raw_ty;
    (void)g_touch_dn;

#if defined(__linux__)
    struct input_event ev;
    /* Patch #2: cst816t on /dev/input/event1 (MT Type-B + 180-degree panel inversion) */
    if (g_touch_fd >= 0 && out_touch && has_touch) {
        while (read(g_touch_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
            if (ev.type == EV_ABS) {
                if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X) {
                    g_last_raw_tx = (float)ev.value;
                } else if (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y) {
                    g_last_raw_ty = (float)ev.value;
                } else if (ev.code == ABS_MT_TRACKING_ID) {
                    g_touch_dn = (ev.value >= 0);
                }
            } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
                g_touch_dn = (ev.value != 0);
            } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
                dial_disc_map_touch_180(g_last_raw_tx, g_last_raw_ty, &out_touch->x, &out_touch->y);
                out_touch->pressed      = g_touch_dn;
                out_touch->timestamp_ms = dial_hal_ticks_ms();
                *has_touch              = true;
                return true;
            }
        }
    }
    /* x2000_key physical buttons on /dev/input/event0 */
    if (g_keys_fd >= 0 && out_key && has_key) {
        while (read(g_keys_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
            if (ev.type == EV_KEY) {
                dial_key_id_t mapped = DIAL_KEY_NONE;
                if (ev.code == KEY_POWER)          mapped = DIAL_KEY_POWER;
                else if (ev.code == KEY_PLAYPAUSE) mapped = DIAL_KEY_PLAYPAUSE;
                else if (ev.code == KEY_VOLUMEUP)  mapped = DIAL_KEY_VOLUP;
                else if (ev.code == KEY_VOLUMEDOWN)mapped = DIAL_KEY_VOLDOWN;
                if (mapped != DIAL_KEY_NONE) {
                    out_key->key          = mapped;
                    out_key->pressed      = (ev.value != 0);
                    out_key->timestamp_ms = dial_hal_ticks_ms();
                    *has_key              = true;
                    return true;
                }
            }
        }
    }
#else
    (void)out_touch;
    (void)out_key;
#endif
    return true;
}

void dial_hal_haptic_tick(uint16_t duration_ms) {
    g_haptic_count++;
    g_last_haptic_ms = duration_ms;
    int fd = open("/sys/class/leds/vibrator/duration", O_WRONLY);
    if (fd >= 0) {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%u\n", (unsigned)duration_ms);
        (void)write(fd, buf, (size_t)len);
        close(fd);
    }
    int afd = open("/sys/class/leds/vibrator/activate", O_WRONLY);
    if (afd >= 0) {
        (void)write(afd, "1\n", 2);
        close(afd);
    }
}

uint32_t dial_hal_get_haptic_count(void)    { return g_haptic_count; }
uint16_t dial_hal_get_last_haptic_ms(void)  { return g_last_haptic_ms; }

void dial_hal_reset_stats(void) {
    g_haptic_count   = 0;
    g_last_haptic_ms = 0;
    g_ipc_head       = 0;
    g_ipc_tail       = 0;
    g_ipc_count      = 0;
    g_last_wire_frame[0] = '\0';
}

bool dial_hal_send_ipc(const char *opcode, const char *payload) {
    if (!opcode) return false;
    if (g_ipc_count >= 32) {
        g_ipc_tail = (g_ipc_tail + 1) % 32;
        g_ipc_count--;
    }
    dial_ipc_msg_t *slot = &g_ipc_ring[g_ipc_head];
    memset(slot, 0, sizeof(*slot));
    strncpy(slot->opcode, opcode, sizeof(slot->opcode) - 1);
    if (payload) {
        strncpy(slot->payload, payload, sizeof(slot->payload) - 1);
    }
    slot->timestamp_ms = dial_hal_ticks_ms();
    g_ipc_head = (g_ipc_head + 1) % 32;
    g_ipc_count++;

    /* Patch #4: Format exact FiiO mq_player wire frame (`TAG(4) + TOTAL_LEN(4hex) + VALUE1(4hex)`) */
    size_t frame_len = dial_ipc_format_frame(g_last_wire_frame, sizeof(g_last_wire_frame), opcode, payload);

    const char *vmq_log = getenv("DIAL_VIRTUAL_MQ_LOG");
    if (vmq_log && *vmq_log && frame_len > 0) {
        FILE *lfp = fopen(vmq_log, "a");
        if (lfp) {
            fprintf(lfp, "%s\n", g_last_wire_frame);
            fclose(lfp);
        }
    }

#if defined(__linux__)
    if (g_mq_player != (mqd_t)-1 && frame_len > 0) {
        (void)mq_send(g_mq_player, g_last_wire_frame, frame_len, 0);
    }
#endif
    return true;
}

bool dial_hal_pop_ipc(dial_ipc_msg_t *out_msg) {
    if (g_ipc_count == 0) return false;
    if (out_msg) *out_msg = g_ipc_ring[g_ipc_tail];
    g_ipc_tail = (g_ipc_tail + 1) % 32;
    g_ipc_count--;
    return true;
}

size_t dial_hal_get_ipc_count(void) { return g_ipc_count; }

size_t dial_hal_get_ram_usage_bytes(void) {
    return sizeof(g_disc_fb) + sizeof(g_ipc_ring) +
           sizeof(g_dial_font_atlas) + sizeof(g_dial_fonts) + sizeof(g_dial_vinyl_covers);
}

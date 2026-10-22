/**
 * @file hal_disc.c
 * @brief FiiO Snowsky DISC Hardware Backend (Ingenic X2000 MIPS32 Linux)
 *
 * Implements the DialOS Hardware Abstraction Layer (`include/dial_hal.h`) for
 * the target device:
 *   - Framebuffer: `/dev/fb0` (360x360 RGB565 mmap with FBIOPAN_DISPLAY)
 *   - Capacitive Touch: `/dev/input/event0` (120 Hz ABS_MT_POSITION_X/Y)
 *   - Physical Buttons: `/dev/input/event1` (GPIO_PA02..05 KEY_POWER/PLAY/VOL)
 *   - Haptic LRA: `/sys/class/leds/vibrator/duration` & `activate`
 *   - Audio Daemon IPC: POSIX message queues `/player` and `/ui`
 *
 * Also compiles cleanly in portable verification mode (`make disc-check`) on
 * macOS/Linux CI hosts to guarantee zero platform-boundary regressions.
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
#endif

static dial_fb_t g_disc_fb;
static int       g_fb_fd    = -1;
static uint16_t *g_fb_mmap  = NULL;
static size_t    g_fb_bytes = DIAL_SCREEN_WIDTH * DIAL_SCREEN_HEIGHT * sizeof(uint16_t);
static int       g_touch_fd = -1;
static int       g_keys_fd  = -1;
static float     g_last_tx  = 180.0f;
static float     g_last_ty  = 180.0f;
static bool      g_touch_dn = false;

static uint32_t       g_haptic_count   = 0;
static uint16_t       g_last_haptic_ms = 0;
static dial_ipc_msg_t g_ipc_ring[32];
static size_t         g_ipc_head  = 0;
static size_t         g_ipc_tail  = 0;
static size_t         g_ipc_count = 0;

#if defined(__linux__)
static mqd_t g_mq_player = (mqd_t)-1;
static mqd_t g_mq_ui     = (mqd_t)-1;
#endif

bool dial_hal_init(bool headless) {
    memset(&g_disc_fb, 0, sizeof(g_disc_fb));
    g_disc_fb.width  = DIAL_SCREEN_WIDTH;
    g_disc_fb.height = DIAL_SCREEN_HEIGHT;

    if (headless) {
        return true;
    }

#if defined(__linux__)
    g_fb_fd = open(DIAL_DEV_FB0, O_RDWR);
    if (g_fb_fd >= 0) {
        struct fb_var_screeninfo vinfo;
        if (ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &vinfo) == 0) {
            g_fb_bytes = (size_t)vinfo.xres * (size_t)vinfo.yres * (vinfo.bits_per_pixel / 8u);
        }
        void *map = mmap(NULL, g_fb_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb_fd, 0);
        if (map != MAP_FAILED) {
            g_fb_mmap = (uint16_t *)map;
        }
    }

    g_touch_fd = open(DIAL_DEV_TOUCH, O_RDONLY | O_NONBLOCK);
    g_keys_fd  = open(DIAL_DEV_KEYS,  O_RDONLY | O_NONBLOCK);

    g_mq_player = mq_open(DIAL_MQ_PLAYER, O_WRONLY | O_NONBLOCK);
    g_mq_ui     = mq_open(DIAL_MQ_UI,     O_RDONLY | O_NONBLOCK);
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
        memcpy(g_fb_mmap, fb->rgb565, total * sizeof(uint16_t));
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
    (void)g_last_tx;
    (void)g_last_ty;
    (void)g_touch_dn;

#if defined(__linux__)
    struct input_event ev;
    if (g_touch_fd >= 0 && out_touch && has_touch) {
        while (read(g_touch_fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
            if (ev.type == EV_ABS) {
                if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X) g_last_tx = (float)ev.value;
                if (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y) g_last_ty = (float)ev.value;
            } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
                g_touch_dn = (ev.value != 0);
            } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
                out_touch->x            = g_last_tx;
                out_touch->y            = g_last_ty;
                out_touch->pressed      = g_touch_dn;
                out_touch->timestamp_ms = dial_hal_ticks_ms();
                *has_touch              = true;
                return true;
            }
        }
    }
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

#if defined(__linux__)
    if (g_mq_player != (mqd_t)-1) {
        char msg[160];
        int n = snprintf(msg, sizeof(msg), "%s %s", opcode, payload ? payload : "");
        (void)mq_send(g_mq_player, msg, (size_t)n, 0);
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

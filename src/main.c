#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "dial_config.h"
#include "dial_theme.h"
#include "dial_hal.h"
#include "dial_polar.h"
#include "dial_gestures.h"
#include "dial_draw.h"
#include "dial_icons.h"
#include "dial_media.h"
#include "dial_app.h"

static dial_app_t g_app;

#if defined(DIAL_TARGET_DISC) && defined(__linux__)
static bool disc_is_sdcard_mounted(void) {
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) return false;
    char line[512];
    bool mounted = false;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, " /tmp/sdcard ") != NULL) {
            mounted = true;
            break;
        }
    }
    fclose(fp);
    return mounted;
}

static bool disc_try_mount_sdcard(void) {
    (void)mkdir("/tmp/sdcard", 0755);
    if (disc_is_sdcard_mounted()) {
        return true;
    }
    const char *dev = NULL;
    if (access("/dev/mmcblk0p1", F_OK) == 0) {
        dev = "/dev/mmcblk0p1";
    } else if (access("/dev/mmcblk0", F_OK) == 0) {
        dev = "/dev/mmcblk0";
    }
    if (!dev) {
        return false;
    }
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "mount -o iocharset=utf8 %s /tmp/sdcard 2>/dev/null || "
             "mount %s /tmp/sdcard 2>/dev/null || "
             "ntfs-3g -o utf8 %s /tmp/sdcard 2>/dev/null",
             dev, dev, dev);
    (void)system(cmd);
    return disc_is_sdcard_mounted();
}
#endif

int main(int argc, char **argv) {
#if defined(DIAL_TARGET_DISC) && defined(__linux__)
    /* Mount /tmp/sdcard (matching stock Snowsky DISC mq_ui) + SD Hot-Reload / Escape Hatch */
    if (!getenv("DIAL_SD_BOOTED")) {
        setenv("DIAL_SD_BOOTED", "1", 1);
        for (int wait_i = 0; wait_i < 10; ++wait_i) {
            if (disc_try_mount_sdcard()) break;
            struct timespec ts = {0, 150000000L};
            nanosleep(&ts, NULL);
        }
        if (access("/tmp/sdcard/dialos/use_stock", F_OK) == 0 ||
            access("/mnt/mmcblk0p1/dialos/use_stock", F_OK) == 0) {
            execv("/usr/bin/mq_ui.stock", argv);
        }
        if (access("/tmp/sdcard/dialos/mq_ui", F_OK) == 0) {
            (void)system("cp /tmp/sdcard/dialos/mq_ui /tmp/mq_ui.sd && chmod +x /tmp/mq_ui.sd");
            execv("/tmp/mq_ui.sd", argv);
        }
        if (access("/mnt/mmcblk0p1/dialos/mq_ui", F_OK) == 0) {
            (void)system("cp /mnt/mmcblk0p1/dialos/mq_ui /tmp/mq_ui.sd && chmod +x /tmp/mq_ui.sd");
            execv("/tmp/mq_ui.sd", argv);
        }
    } else {
        (void)disc_try_mount_sdcard();
    }
#endif
    bool headless = false;
    const char *out_path  = "/tmp/dialos_snapshot.png";
    const char *music_dir = NULL;
    dial_mode_t mode      = DIAL_MODE_NIGHT;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--headless") == 0) {
            headless = true;
        } else if (strcmp(argv[i], "--music") == 0 && i + 1 < argc) {
            music_dir = argv[++i];
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            const char *m = argv[++i];
            mode = (strcmp(m, "day") == 0 || strcmp(m, "DAY") == 0)
                       ? DIAL_MODE_DAY
                       : DIAL_MODE_NIGHT;
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        }
    }

    if (!dial_hal_init(headless)) {
        fprintf(stderr, "Failed to initialize DialOS HAL\n");
        return 1;
    }

#if defined(DIAL_TARGET_DISC)
    const char *target_music = music_dir ? music_dir : "/tmp/sdcard";
#else
    const char *home = getenv("HOME");
    char default_music[512];
    if (access("/Volumes/MUSIC", R_OK) == 0) {
        snprintf(default_music, sizeof(default_music), "/Volumes/MUSIC");
    } else {
        snprintf(default_music, sizeof(default_music), "%s/Music", home ? home : ".");
    }
    const char *target_music = music_dir ? music_dir : default_music;
#endif
    if (!headless || music_dir) {
        dial_media_scan_root(target_music);
    }

    dial_app_init(&g_app, mode);

    dial_fb_t *fb = dial_hal_get_framebuffer();
    dial_app_tick(&g_app, dial_hal_ticks_ms());
    dial_app_render(&g_app, fb);
    dial_hal_present(fb);

    if (headless) {
        int rc = dial_hal_save_png(fb, out_path);
        dial_hal_shutdown();
        return (rc == 0) ? 0 : 2;
    }

#if defined(DIAL_TARGET_DISC) && defined(__linux__)
    bool sd_was_mounted = disc_is_sdcard_mounted();
    uint32_t sd_poll_frames = 0;
#endif

    bool running = true;
    while (running) {
        /* Drain all pending input events for zero-latency touch/mouse/key response */
        while (true) {
            dial_touch_event_t touch;
            dial_key_event_t   key;
            bool has_touch = false, has_key = false;
            if (!dial_hal_poll_events(&touch, &has_touch, &key, &has_key)) {
                running = false;
                break;
            }
            if (!has_touch && !has_key) {
                break;
            }
            if (has_touch) {
                dial_app_handle_touch(&g_app, &touch);
            }
            if (has_key) {
                dial_app_handle_key(&g_app, &key);
            }
        }
        if (!running) break;

#if defined(DIAL_TARGET_DISC) && defined(__linux__)
        if (++sd_poll_frames >= 120u) {
            sd_poll_frames = 0;
            bool now_mounted = disc_try_mount_sdcard();
            if (now_mounted != sd_was_mounted ||
                (now_mounted && dial_media_get_fs_entry_count() == 0)) {
                sd_was_mounted = now_mounted;
                dial_media_scan_root(target_music);
            }
        }
#endif

        dial_app_tick(&g_app, dial_hal_ticks_ms());
        dial_app_render(&g_app, fb);
        dial_hal_present(fb);
        dial_hal_delay_ms(16);
    }

    dial_hal_shutdown();
    return 0;
}

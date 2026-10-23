#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dial_config.h"
#include "dial_theme.h"
#include "dial_hal.h"
#include "dial_polar.h"
#include "dial_gestures.h"
#include "dial_draw.h"
#include "dial_icons.h"
#include "dial_media.h"

int main(int argc, char **argv) {
#if defined(DIAL_TARGET_DISC) && defined(__linux__)
    /* SD-Card Hot-Reload & Stock Escape Hatch (zero 18-minute re-flash needed) */
    if (!getenv("DIAL_SD_BOOTED")) {
        setenv("DIAL_SD_BOOTED", "1", 1);
        if (access("/mnt/mmcblk0p1/dialos/use_stock", F_OK) == 0) {
            execv("/usr/bin/mq_ui.stock", argv);
        }
        if (access("/mnt/mmcblk0p1/dialos/mq_ui", F_OK) == 0) {
            (void)system("cp /mnt/mmcblk0p1/dialos/mq_ui /tmp/mq_ui.sd && chmod +x /tmp/mq_ui.sd");
            execv("/tmp/mq_ui.sd", argv);
        }
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

    if (!headless) {
#if defined(DIAL_TARGET_DISC)
        const char *target_music = music_dir ? music_dir : "/mnt/mmcblk0p1";
#else
        const char *home = getenv("HOME");
        char default_music[512];
        snprintf(default_music, sizeof(default_music), "%s/Music", home ? home : ".");
        const char *target_music = music_dir ? music_dir : default_music;
#endif
        dial_media_scan_root(target_music);
    } else if (music_dir) {
        dial_media_scan_root(music_dir);
    }

    dial_fb_t *fb = dial_hal_get_framebuffer();
    const dial_palette_t *pal = dial_get_palette(mode);
    dial_draw_clear(fb, pal->bg);
    dial_draw_circle_mask(fb);
    dial_hal_present(fb);

    if (headless) {
        int rc = dial_hal_save_png(fb, out_path);
        dial_hal_shutdown();
        return (rc == 0) ? 0 : 2;
    }

    while (true) {
        dial_touch_event_t touch;
        dial_key_event_t   key;
        bool has_touch = false, has_key = false;
        if (!dial_hal_poll_events(&touch, &has_touch, &key, &has_key)) {
            break;
        }
        (void)touch;
        (void)key;
        dial_hal_present(fb);
        dial_hal_delay_ms(16);
    }

    dial_hal_shutdown();
    return 0;
}

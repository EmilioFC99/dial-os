#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dial_config.h"
#include "dial_theme.h"
#include "dial_hal.h"
#include "dial_polar.h"
#include "dial_gestures.h"
#include "dial_draw.h"
#include "dial_icons.h"
#include "dial_screens.h"
#include "dial_app.h"
#include "dial_media.h"

int main(int argc, char **argv) {
    bool headless = false;
    const char *screen_id  = "NP-01";
    const char *out_path   = "/tmp/dialos_snapshot.png";
    const char *music_dir  = NULL;
    dial_mode_t mode       = DIAL_MODE_NIGHT;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--headless") == 0) {
            headless = true;
        } else if (strcmp(argv[i], "--screen") == 0 && i + 1 < argc) {
            screen_id = argv[++i];
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
        const char *target_music = music_dir ? music_dir : "/tmp/sdcard";
#else
        const char *target_music = music_dir ? music_dir : "/Users/emilio.flores/Music/Au5";
#endif
        dial_media_scan_root(target_music);
    } else if (music_dir) {
        dial_media_scan_root(music_dir);
    }

    dial_app_t *app = dial_app_get_state();
    dial_app_init(app, screen_id, mode);

    dial_fb_t *fb = dial_hal_get_framebuffer();
    dial_app_render(app, fb, 0);
    dial_hal_present(fb);

    if (headless) {
        int rc = dial_hal_save_png(fb, out_path);
        dial_hal_shutdown();
        if (rc != 0) {
            fprintf(stderr, "Failed to save PNG snapshot to %s (rc=%d)\n", out_path, rc);
            return 2;
        }
        printf("[DialOS] Rendered %s (%s) -> %s (RAM: %.2f KB)\n",
               screen_id,
               (mode == DIAL_MODE_DAY) ? "day" : "night",
               out_path,
               (double)dial_hal_get_ram_usage_bytes() / 1024.0);
        return 0;
    }

    printf("[DialOS] Live Interactive Simulator started on screen %s (%s mode)\n",
           app->screen_id, (app->mode == DIAL_MODE_DAY) ? "Day" : "Night");
    printf("  Mouse: Click/Drag r=38px vinyl ring, 1x tap Quick Actions, 2x tap Play/Pause, Rim scroll, Top pull-down CC-01\n");
    printf("  Keys : [Left/Right] Cycle Screens | [D] Night/Day | [Space] Play/Pause | [Up/Down] Vol | [S] Screenshot | [Q] Quit\n");

    /* Live 60 FPS interactive desktop loop (native macOS Cocoa or SDL2) */
    while (true) {
        dial_touch_event_t touch;
        dial_key_event_t   key;
        bool has_touch = false, has_key = false;
        if (!dial_hal_poll_events(&touch, &has_touch, &key, &has_key)) {
            break;
        }
        if (has_touch) {
            dial_app_on_touch(app, &touch);
        }
        if (has_key) {
            dial_app_on_key(app, &key, fb);
        }
        uint32_t now_ms = dial_hal_ticks_ms();
        dial_app_tick(app, now_ms);
        dial_app_render(app, fb, now_ms);
        dial_hal_present(fb);
        dial_hal_delay_ms(16);
    }

    dial_hal_shutdown();
    return 0;
}

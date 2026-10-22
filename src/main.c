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

int main(int argc, char **argv) {
    bool headless = false;
    const char *screen_id = "NP-01";
    const char *out_path  = "/tmp/dialos_snapshot.png";
    dial_mode_t mode      = DIAL_MODE_NIGHT;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--headless") == 0) {
            headless = true;
        } else if (strcmp(argv[i], "--screen") == 0 && i + 1 < argc) {
            screen_id = argv[++i];
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

    dial_fb_t *fb = dial_hal_get_framebuffer();
    dial_screen_render_by_id(fb, screen_id, mode, 0);
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

    /* Interactive desktop loop when SDL2 is enabled */
    dial_gesture_fsm_t fsm;
    dial_gesture_fsm_init(&fsm, strcmp(screen_id, "NP-01") == 0, 330000u);

    while (true) {
        dial_touch_event_t touch;
        dial_key_event_t   key;
        bool has_touch = false, has_key = false;
        if (!dial_hal_poll_events(&touch, &has_touch, &key, &has_key)) {
            break;
        }
        if (has_touch) {
            dial_gesture_fsm_step(&fsm, &touch);
        }
        dial_gesture_fsm_tick(&fsm, dial_hal_ticks_ms());
        dial_screen_render_by_id(fb, screen_id, mode, dial_hal_ticks_ms());
        dial_hal_present(fb);
    }

    dial_hal_shutdown();
    return 0;
}

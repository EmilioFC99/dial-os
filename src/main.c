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

/* Render showcase / diagnostic screen by ID into 360x360 framebuffer */
static void render_screen_by_id(dial_fb_t *fb,
                                const char *screen_id,
                                dial_mode_t mode,
                                uint32_t tick_ms) {
    const dial_palette_t *pal = dial_get_palette(mode);
    dial_draw_clear(fb, pal->bg);

    if (strcmp(screen_id, "TEST-ICONS") == 0) {
        /* Render all 70 Fjäll vector icons in a 10x7 circular-safe matrix */
        dial_draw_header(fb, pal, "Fjall 70 Icons", "", 22);
        const int cols = 10;
        const int start_x = 46;
        const int start_y = 48;
        const int cell_w = 27;
        const int cell_h = 36;
        for (int i = 0; i < (int)DIAL_ICON_COUNT; ++i) {
            int r = i / cols;
            int c = i % cols;
            float ix = (float)(start_x + c * cell_w);
            float iy = (float)(start_y + r * cell_h);
            uint32_t col = (i % 4 == 0) ? pal->moss :
                           (i % 4 == 1) ? pal->fjord :
                           (i % 4 == 2) ? pal->cloudberry : pal->text_1;
            dial_draw_icon(fb, (dial_icon_id_t)i, ix, iy, 18.0f, 1.75f, col, 255);
        }
        dial_draw_circle_mask(fb);
        return;
    }

    if (strcmp(screen_id, "LIB-01") == 0 || strcmp(screen_id, "TEST-PRIMITIVES") == 0) {
        /* Locked LIB-01 Fisheye Rim List: Favorites, Playlists, Artists, Albums, Genres */
        dial_draw_header(fb, pal, "Library", "", 42);
        const dial_rim_row_t rows[5] = {
            { .icon_name = "heart",    .title = "Favorites", .value_text = "64" },
            { .icon_name = "playlist", .title = "Playlists", .value_text = "12" },
            { .icon_name = "artist",   .title = "Artists",   .sub = "118 artists . A-Z", .has_chev = true },
            { .icon_name = "album",    .title = "Albums",    .value_text = "214" },
            { .icon_name = "genre",    .title = "Genres",    .value_text = "24" }
        };
        dial_draw_rim_list(fb, pal, rows, 5, 2, 64, tick_ms);
        dial_draw_scroll_rim(fb, pal, 0.10f, 0.30f);
        dial_draw_circle_mask(fb);
        return;
    }

    /* Default: Locked NP-01 Pure Full-Bleed Spinning Vinyl + r=38px Inner Seek Ring */
    dial_draw_art(fb, pal, DIAL_ART_FJORD, DIAL_CENTER_X, DIAL_CENTER_Y, 360, true, 0.0f);
    dial_draw_vinyl_seek_ring(fb, pal, 0.41f);
    dial_draw_rim_arc(fb, pal, 145.0f, 35.0f, 0.82f, pal->moss, (float)DIAL_RIM_ARC_R, 4.0f, true, false, true);
    dial_draw_text(fb, "82", 316, 172, 11, pal->moss, 255, false);
    dial_draw_top_grab(fb, pal);
    dial_draw_icon(fb, DIAL_ICON_UP, 170.0f, 324.0f, 20.0f, 2.2f, 0xECEEE8u, 216);
    dial_draw_circle_mask(fb);
}

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
    render_screen_by_id(fb, screen_id, mode, 0);
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
        render_screen_by_id(fb, screen_id, mode, dial_hal_ticks_ms());
        dial_hal_present(fb);
    }

    dial_hal_shutdown();
    return 0;
}

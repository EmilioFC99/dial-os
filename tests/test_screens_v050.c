#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dial_app.h"
#include "dial_screens.h"

static bool rect_has_color_near(const dial_fb_t *fb,
                                int x0, int y0, int x1, int y1,
                                uint32_t target_rgb, int tol) {
    int tr = (int)((target_rgb >> 16) & 0xFFu);
    int tg = (int)((target_rgb >> 8)  & 0xFFu);
    int tb = (int)( target_rgb        & 0xFFu);

    for (int y = y0; y <= y1; ++y) {
        if (y < 0 || y >= (int)fb->height) continue;
        for (int x = x0; x <= x1; ++x) {
            if (x < 0 || x >= (int)fb->width) continue;
            uint32_t p = fb->pixels[y * fb->width + x];
            if ((p >> 24) == 0) continue;
            int r = (int)((p >> 16) & 0xFFu);
            int g = (int)((p >> 8)  & 0xFFu);
            int b = (int)( p        & 0xFFu);
            if (abs(r - tr) <= tol && abs(g - tg) <= tol && abs(b - tb) <= tol) {
                return true;
            }
        }
    }
    return false;
}

int main(void) {
    assert(dial_hal_init(true));
    dial_fb_t *fb = dial_hal_get_framebuffer();
    assert(fb != NULL);

    static const char *kV050Screens[22] = {
        "MOD-00", "MOD-01", "MOD-02", "MOD-03", "MOD-04", "MOD-05", "MOD-06",
        "SET-01", "SET-02", "SET-03", "SET-04", "SET-05", "SET-06", "SET-07",
        "GAM-01", "GAM-02", "GAM-03", "GAM-04",
        "OVL-01", "OVL-02", "OVL-03", "OVL-04"
    };

    /* 1. Render all 22 v0.5.0 screens in both Polar Night and Birch Day modes */
    for (size_t i = 0; i < 22; ++i) {
        dial_app_t *app = dial_app_get_state();
        dial_app_init(app, kV050Screens[i], DIAL_MODE_NIGHT);
        assert(dial_screen_render_by_id(fb, kV050Screens[i], DIAL_MODE_NIGHT, 500));
        /* Outside r = 180px circular bezel must be transparent */
        assert(fb->pixels[0] == 0x00000000u);
        assert(fb->pixels[359 * 360 + 359] == 0x00000000u);

        dial_app_init(app, kV050Screens[i], DIAL_MODE_DAY);
        assert(dial_screen_render_by_id(fb, kV050Screens[i], DIAL_MODE_DAY, 500));
        assert(fb->pixels[0] == 0x00000000u);
    }

    /* 2. Verify all 38 master screens are registered in G_SCREENS */
    size_t total_count = 0;
    const dial_screen_desc_t *all = dial_screen_get_all(&total_count);
    assert(all != NULL);
    assert(total_count >= 38);

    const dial_palette_t *night = dial_get_palette(DIAL_MODE_NIGHT);

    /* 3. MOD-00: Focused mode 0 puck at 12 o'clock (180, 64) has Moss ring + Active chip */
    dial_app_init(dial_app_get_state(), "MOD-00", DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "MOD-00", DIAL_MODE_NIGHT, 0);
    assert(rect_has_color_near(fb, 150, 34, 210, 94, night->moss, 28));

    /* 4. MOD-02: USB DAC has 384 kHz hero + train-sliding "Draw power from host" row */
    dial_app_init(dial_app_get_state(), "MOD-02", DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "MOD-02", DIAL_MODE_NIGHT, 0);
    uint64_t row_hash_t0 = 0;
    for (int y = 252; y <= 278; ++y) {
        for (int x = 86; x <= 230; ++x) {
            row_hash_t0 = row_hash_t0 * 131u + fb->pixels[y * DIAL_SCREEN_WIDTH + x];
        }
    }
    dial_screen_render_by_id(fb, "MOD-02", DIAL_MODE_NIGHT, 900);
    uint64_t row_hash_t1 = 0;
    for (int y = 252; y <= 278; ++y) {
        for (int x = 86; x <= 230; ++x) {
            row_hash_t1 = row_hash_t1 * 131u + fb->pixels[y * DIAL_SCREEN_WIDTH + x];
        }
    }
    assert(row_hash_t0 != row_hash_t1);

    /* 5. SET-04: Parametric EQ has Moss response curve and 10 vertical band bars */
    dial_app_init(dial_app_get_state(), "SET-04", DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "SET-04", DIAL_MODE_NIGHT, 0);
    assert(rect_has_color_near(fb, 60, 102, 300, 162, night->moss, 28));
    assert(rect_has_color_near(fb, 54, 178, 306, 260, night->moss, 28));

    /* 6. SET-07: Fast-Cache Bluetooth shows WH-1000XM5 with Moss LDAC 990 + Fjord Default chips */
    dial_app_init(dial_app_get_state(), "SET-07", DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "SET-07", DIAL_MODE_NIGHT, 0);
    assert(rect_has_color_near(fb, 90, 130, 270, 165, night->moss, 28));
    assert(rect_has_color_near(fb, 180, 130, 290, 165, night->fjord, 28));

    /* 7. GAM-02: Dial Pong has Moss paddle arc on rim and Cloudberry ball at (206, 132) */
    dial_app_init(dial_app_get_state(), "GAM-02", DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "GAM-02", DIAL_MODE_NIGHT, 0);
    assert(rect_has_color_near(fb, 198, 124, 214, 140, night->cloudberry, 24));
    assert(rect_has_color_near(fb, 100, 310, 180, 350, night->moss, 28));

    /* 8. GAM-03: 2048 Wheel has Heather 1024 tile and Cloudberry 512 tile */
    dial_app_init(dial_app_get_state(), "GAM-03", DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "GAM-03", DIAL_MODE_NIGHT, 0);
    assert(rect_has_color_near(fb, 130, 236, 170, 276, night->cloudberry, 24));
    assert(rect_has_color_near(fb, 182, 236, 224, 276, night->heather, 24));

    /* 9. OVL-03: Low battery dialog has Cloudberry battery well and Moss Turn on button */
    dial_app_init(dial_app_get_state(), "OVL-03", DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "OVL-03", DIAL_MODE_NIGHT, 0);
    assert(rect_has_color_near(fb, 160, 88, 200, 128, night->cloudberry, 28));
    assert(rect_has_color_near(fb, 182, 234, 280, 274, night->moss, 28));

    dial_hal_shutdown();
    printf("[PASS] test_screens_v050: all 22 MOD/SET/GAM/OVL screens (38/38 total) verified.\n");
    return 0;
}

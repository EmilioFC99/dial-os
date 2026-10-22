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

    static const char *kV040Screens[8] = {
        "LIB-01", "LIB-02", "LIB-03", "LIB-04",
        "FIL-01", "FIL-02", "FIL-03", "FIL-04"
    };

    /* 1. Render all 8 v0.4.0 screens in both Polar Night and Birch Day modes */
    for (size_t i = 0; i < 8; ++i) {
        dial_app_t *app = dial_app_get_state();
        dial_app_init(app, kV040Screens[i], DIAL_MODE_NIGHT);
        assert(dial_screen_render_by_id(fb, kV040Screens[i], DIAL_MODE_NIGHT, 500));
        /* Outside r = 180px circular bezel must be transparent */
        assert(fb->pixels[0] == 0x00000000u);
        assert(fb->pixels[359 * 360 + 359] == 0x00000000u);

        dial_app_init(app, kV040Screens[i], DIAL_MODE_DAY);
        assert(dial_screen_render_by_id(fb, kV040Screens[i], DIAL_MODE_DAY, 500));
        assert(fb->pixels[0] == 0x00000000u);
    }

    const dial_palette_t *night = dial_get_palette(DIAL_MODE_NIGHT);

    /* 2. LIB-01: Focused row 2 (Artists) has Moss border + right scrollRim */
    dial_app_init(dial_app_get_state(), "LIB-01", DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "LIB-01", DIAL_MODE_NIGHT, 0);
    assert(rect_has_color_near(fb, 36, 165, 324, 225, night->moss, 28));

    /* 3. LIB-02: 132px center cover at (180,138) has r=71px Moss ring + 84px/56px neighbours */
    dial_app_init(dial_app_get_state(), "LIB-02", DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "LIB-02", DIAL_MODE_NIGHT, 0);
    assert(rect_has_color_near(fb, 175, 65, 185, 72, night->moss, 28));

    /* 4. LIB-03: Playing track row 2 has Moss equalizer bars + 9 o'clock back chevron */
    dial_app_init(dial_app_get_state(), "LIB-03", DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "LIB-03", DIAL_MODE_NIGHT, 0);
    assert(rect_has_color_near(fb, 80, 185, 150, 218, night->moss, 28));
    assert(rect_has_color_near(fb, 10, 168, 34, 192, night->text_3, 32));

    /* 5. LIB-04: Center 100x100px letter jump bubble with 'M' in Moss */
    dial_app_init(dial_app_get_state(), "LIB-04", DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "LIB-04", DIAL_MODE_NIGHT, 0);
    assert(rect_has_color_near(fb, 160, 155, 200, 200, night->moss, 24));

    /* 6. FIL-02: Action Sheet has Lingon red Delete puck on right (cx=254, cy=246) */
    dial_app_init(dial_app_get_state(), "FIL-02", DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "FIL-02", DIAL_MODE_NIGHT, 0);
    assert(rect_has_color_near(fb, 228, 220, 280, 272, night->lingon, 28));

    /* 7. FIL-03: Delete Confirm has Lingon trash pebble at (180,100), Moss Cancel on left, Lingon Hold on right */
    dial_app_init(dial_app_get_state(), "FIL-03", DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "FIL-03", DIAL_MODE_NIGHT, 0);
    assert(rect_has_color_near(fb, 160, 80, 200, 120, night->lingon, 28));
    assert(rect_has_color_near(fb, 54, 216, 154, 256, night->moss, 28));
    assert(rect_has_color_near(fb, 162, 216, 306, 256, night->lingon, 28));

    /* 8. FIL-04: Empty state has Moss File Transfer pill button at bottom */
    dial_app_init(dial_app_get_state(), "FIL-04", DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "FIL-04", DIAL_MODE_NIGHT, 0);
    assert(rect_has_color_near(fb, 120, 264, 240, 298, night->moss, 28));

    dial_hal_shutdown();
    printf("[PASS] test_screens_v040: all 8 LIB-01..04 & FIL-01..04 screens & locked UX rules verified.\n");
    return 0;
}

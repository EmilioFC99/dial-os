#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "dial_config.h"
#include "dial_theme.h"
#include "dial_hal.h"
#include "dial_polar.h"
#include "dial_draw.h"
#include "dial_screens.h"

static uint32_t fb_hash(const dial_fb_t *fb) {
    uint32_t h = 2166136261u;
    const size_t total = (size_t)fb->width * (size_t)fb->height;
    for (size_t i = 0; i < total; i += 3) {
        h = (h ^ fb->pixels[i]) * 16777619u;
    }
    return h;
}

static void assert_bezel_clipped(const dial_fb_t *fb) {
    for (int y = 0; y < DIAL_SCREEN_HEIGHT; ++y) {
        for (int x = 0; x < DIAL_SCREEN_WIDTH; ++x) {
            float dx = ((float)x + 0.5f) - (float)DIAL_CENTER_X;
            float dy = ((float)y + 0.5f) - (float)DIAL_CENTER_Y;
            if (sqrtf(dx * dx + dy * dy) > (float)DIAL_RADIUS) {
                assert(fb->pixels[y * DIAL_SCREEN_WIDTH + x] == 0x00000000u);
            }
        }
    }
}

/* 1. Verify all 13 Milestone 2 (v0.3.0) screens render in Night & Day modes */
static void test_all_v030_screens_render(void) {
    static const char *IDS[] = {
        "SYS-00",
        "LCK-01", "LCK-02",
        "HOM-01", "HOM-02", "HOM-03",
        "CC-01",  "CC-02",
        "NP-01",  "NP-02",  "NP-03", "NP-04", "NP-05"
    };
    const size_t n_ids = sizeof(IDS) / sizeof(IDS[0]);
    dial_fb_t *fb = dial_hal_get_framebuffer();

    for (size_t i = 0; i < n_ids; ++i) {
        const dial_screen_desc_t *desc = dial_screen_find(IDS[i]);
        assert(desc != NULL);
        assert(strcmp(desc->id, IDS[i]) == 0);

        assert(dial_screen_render_by_id(fb, IDS[i], DIAL_MODE_NIGHT, 0) == true);
        assert_bezel_clipped(fb);
        uint32_t h_night = fb_hash(fb);

        assert(dial_screen_render_by_id(fb, IDS[i], DIAL_MODE_DAY, 0) == true);
        assert_bezel_clipped(fb);
        uint32_t h_day = fb_hash(fb);

        /* Night and Day palettes must produce distinct framebuffers */
        assert(h_night != h_day);
    }
}

/* 2. Verify Locked UX Rule #2 & #4: NP-01 has r=38px seek ring; NP-05 Volume HUD has NO inner circle */
static void test_np01_vs_np05_inner_ring_rule(void) {
    dial_fb_t *fb = dial_hal_get_framebuffer();
    const dial_palette_t *pal = dial_get_palette(DIAL_MODE_NIGHT);

    /* On NP-01, the inner seek ring at r=38px (41% = 147.6 deg) has a Moss knob */
    float kx = 0.0f, ky = 0.0f;
    dial_polar_to_xy(360.0f * 0.41f, (float)DIAL_VINYL_SEEK_R, &kx, &ky);
    int ikx = (int)roundf(kx);
    int iky = (int)roundf(ky);

    dial_screen_render_by_id(fb, "NP-01", DIAL_MODE_NIGHT, 0);
    uint32_t np01_knob = fb->pixels[iky * DIAL_SCREEN_WIDTH + ikx] & 0xFFFFFFu;
    assert(np01_knob == pal->moss);

    /* On NP-05 (Volume HUD), Locked Rule #4 forbids rendering the r=38px inner vinyl seek ring */
    dial_screen_render_by_id(fb, "NP-05", DIAL_MODE_NIGHT, 0);
    uint32_t np05_at_knob = fb->pixels[iky * DIAL_SCREEN_WIDTH + ikx] & 0xFFFFFFu;
    assert(np05_at_knob != pal->moss);
}

/* 3. Verify Locked UX Rule #3: NP-02 Bottom Arc Pucks (234°, 198°, 162°, 126° at r=118px) */
static void test_np02_bottom_arc_pucks(void) {
    dial_fb_t *fb = dial_hal_get_framebuffer();
    const dial_palette_t *pal = dial_get_palette(DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "NP-02", DIAL_MODE_NIGHT, 0);

    const float angles[4] = { 234.0f, 198.0f, 162.0f, 126.0f };
    for (int i = 0; i < 4; ++i) {
        float px = 0.0f, py = 0.0f;
        dial_polar_to_xy(angles[i], (float)DIAL_QA_ARC_RADIUS, &px, &py);
        /* Check top edge of each 52px puck (radius 26px) */
        int bx = (int)roundf(px);
        int by = (int)roundf(py - 20.0f);
        uint32_t sample = fb->pixels[by * DIAL_SCREEN_WIDTH + bx] & 0xFFFFFFu;
        if (i == 0) {
            /* 234° Heart puck is active/tinted with Moss */
            uint32_t expected_fill = dial_color_lerp(pal->surface_2, pal->moss, 52);
            assert(sample == expected_fill);
        } else {
            /* 198°, 162°, 126° pucks are surface_2 */
            assert(sample == pal->surface_2);
        }
    }
}

/* 4. Verify Locked UX Rule #5: CC-01 3x2 Icon-Only Pucks Semantic Tints */
static void test_cc01_semantic_pucks(void) {
    dial_fb_t *fb = dial_hal_get_framebuffer();
    const dial_palette_t *pal = dial_get_palette(DIAL_MODE_NIGHT);
    dial_screen_render_by_id(fb, "CC-01", DIAL_MODE_NIGHT, 0);

    /* Sample background ring inside each 66px puck (offset +22px from center to avoid 26px icon) */
    uint32_t wifi_bg = fb->pixels[(131 - 24) * DIAL_SCREEN_WIDTH + 93]  & 0xFFFFFFu;
    uint32_t bt_bg   = fb->pixels[(131 - 24) * DIAL_SCREEN_WIDTH + 180] & 0xFFFFFFu;
    uint32_t mode_bg = fb->pixels[(131 - 24) * DIAL_SCREEN_WIDTH + 267] & 0xFFFFFFu;
    uint32_t gain_bg = fb->pixels[(219 - 24) * DIAL_SCREEN_WIDTH + 93]  & 0xFFFFFFu;
    uint32_t usb_bg  = fb->pixels[(219 - 24) * DIAL_SCREEN_WIDTH + 180] & 0xFFFFFFu;
    uint32_t lock_bg = fb->pixels[(219 - 24) * DIAL_SCREEN_WIDTH + 267] & 0xFFFFFFu;

    assert(wifi_bg == dial_color_lerp(pal->surface_1, pal->fjord, 46));
    assert(bt_bg   == dial_color_lerp(pal->surface_1, pal->fjord, 46));
    assert(mode_bg == dial_color_lerp(pal->surface_1, pal->moss, 46));
    assert(gain_bg == dial_color_lerp(pal->surface_1, pal->cloudberry, 46));
    assert(usb_bg  == pal->surface_1);
    assert(lock_bg == pal->surface_1);
}

int main(void) {
    dial_hal_init(true);
    test_all_v030_screens_render();
    test_np01_vs_np05_inner_ring_rule();
    test_np02_bottom_arc_pucks();
    test_cc01_semantic_pucks();
    dial_hal_shutdown();
    printf("[PASS] test_screens_v030: all 13 SYS/LCK/HOM/CC/NP screens & locked UX rules verified.\n");
    return 0;
}

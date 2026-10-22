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
#include "dial_app.h"

/* Helper to simulate a complete tap (down + up) at (x, y) */
static void sim_tap(dial_app_t *app, float x, float y, uint32_t *ts_ms) {
    dial_touch_event_t down = { .x = x, .y = y, .pressed = true,  .timestamp_ms = *ts_ms };
    dial_touch_event_t up   = { .x = x, .y = y, .pressed = false, .timestamp_ms = *ts_ms + 30u };
    dial_app_on_touch(app, &down);
    dial_app_on_touch(app, &up);
    *ts_ms += 40u;
}

/* 1. Test Live Interactive Seek on NP-01: Dragging r=38px Inner Ring Moves Rendered Knob */
static void test_interactive_np01_seek_and_render(void) {
    dial_fb_t *fb = dial_hal_get_framebuffer();
    const dial_palette_t *pal = dial_get_palette(DIAL_MODE_NIGHT);
    dial_app_t *app = dial_app_get_state();
    dial_app_init(app, "NP-01", DIAL_MODE_NIGHT);

    /* Grab inner vinyl ring at 12 o'clock (180, 142) and drag to 9 o'clock (142, 180) = 270 deg (75%) */
    dial_touch_event_t t0 = { .x = 180.0f, .y = 142.0f, .pressed = true,  .timestamp_ms = 100 };
    dial_touch_event_t t1 = { .x = 142.0f, .y = 180.0f, .pressed = true,  .timestamp_ms = 150 };
    dial_touch_event_t t2 = { .x = 142.0f, .y = 180.0f, .pressed = false, .timestamp_ms = 180 };

    dial_app_on_touch(app, &t0);
    dial_app_on_touch(app, &t1);
    dial_app_on_touch(app, &t2);

    assert(fabsf(app->seek_progress - 0.75f) < 0.01f);

    /* Render live framebuffer and verify the Moss thumb knob moved to 9 o'clock (142, 180) */
    dial_app_render(app, fb, 200);
    uint32_t knob_9oclock = fb->pixels[180 * DIAL_SCREEN_WIDTH + 142] & 0xFFFFFFu;
    assert(knob_9oclock == pal->moss);
}

/* 2. Test Live Single-Tap -> NP-02 Quick Actions -> Toggle Heart & Open Synced Lyrics NP-03 */
static void test_interactive_np02_quick_actions_flow(void) {
    dial_fb_t *fb = dial_hal_get_framebuffer();
    const dial_palette_t *pal = dial_get_palette(DIAL_MODE_NIGHT);
    dial_app_t *app = dial_app_get_state();
    dial_app_init(app, "NP-01", DIAL_MODE_NIGHT);

    uint32_t ts = 1000u;
    /* Single tap center on NP-01 -> wait 260ms disambiguation window -> opens NP-02 */
    sim_tap(app, 180.0f, 180.0f, &ts);
    ts += 270u;
    dial_app_tick(app, ts);
    assert(strcmp(app->screen_id, "NP-02") == 0);

    /* Tap Heart puck at 234° on r = 118px -> toggles is_favorited from true to false */
    float hx = 0.0f, hy = 0.0f;
    dial_polar_to_xy(234.0f, (float)DIAL_QA_ARC_RADIUS, &hx, &hy);
    assert(app->is_favorited == true);
    sim_tap(app, hx, hy, &ts);
    assert(app->is_favorited == false);

    /* Render NP-02 and verify the 234° Heart puck background is now unlit (surface_2) */
    dial_app_render(app, fb, ts);
    int sample_x = (int)roundf(hx);
    int sample_y = (int)roundf(hy - 20.0f);
    assert((fb->pixels[sample_y * DIAL_SCREEN_WIDTH + sample_x] & 0xFFFFFFu) == pal->surface_2);

    /* Tap Lyrics puck at 126° on r = 118px -> navigates to NP-03 Synced Lyrics */
    float lx = 0.0f, ly = 0.0f;
    dial_polar_to_xy(126.0f, (float)DIAL_QA_ARC_RADIUS, &lx, &ly);
    sim_tap(app, lx, ly, &ts);
    assert(strcmp(app->screen_id, "NP-03") == 0);

    /* Tap NP-03 -> returns to NP-01 */
    sim_tap(app, 180.0f, 180.0f, &ts);
    assert(strcmp(app->screen_id, "NP-01") == 0);
}

/* 3. Test Top Pull-Down -> CC-01 Puck Toggles -> CC-02 600ms Hold Unlock */
static void test_interactive_control_center_and_lock_shield(void) {
    dial_fb_t *fb = dial_hal_get_framebuffer();
    const dial_palette_t *pal = dial_get_palette(DIAL_MODE_NIGHT);
    dial_app_t *app = dial_app_get_state();
    dial_app_init(app, "NP-01", DIAL_MODE_NIGHT);

    uint32_t ts = 5000u;
    /* Pull down from top edge (180, 12) -> (180, 48) -> opens CC-01 */
    dial_touch_event_t pd0 = { .x = 180.0f, .y = 12.0f, .pressed = true,  .timestamp_ms = ts };
    dial_touch_event_t pd1 = { .x = 180.0f, .y = 48.0f, .pressed = true,  .timestamp_ms = ts + 30u };
    dial_touch_event_t pd2 = { .x = 180.0f, .y = 48.0f, .pressed = false, .timestamp_ms = ts + 50u };
    dial_app_on_touch(app, &pd0);
    dial_app_on_touch(app, &pd1);
    dial_app_on_touch(app, &pd2);
    ts += 60u;
    assert(strcmp(app->screen_id, "CC-01") == 0);

    /* Tap High Gain puck (93, 219) -> toggles high_gain off (unlit) */
    assert(app->high_gain == true);
    sim_tap(app, 93.0f, 219.0f, &ts);
    assert(app->high_gain == false);

    /* Tap USB Charge puck (180, 219) -> toggles usb_charge on (lit in Moss) */
    assert(app->usb_charge == false);
    sim_tap(app, 180.0f, 219.0f, &ts);
    assert(app->usb_charge == true);

    /* Render CC-01 and verify pixels reflect toggled Gain (unlit) and USB Charge (Moss) */
    dial_app_render(app, fb, ts);
    uint32_t gain_bg = fb->pixels[(219 - 24) * DIAL_SCREEN_WIDTH + 93]  & 0xFFFFFFu;
    uint32_t usb_bg  = fb->pixels[(219 - 24) * DIAL_SCREEN_WIDTH + 180] & 0xFFFFFFu;
    assert(gain_bg == pal->surface_1);
    assert(usb_bg  == dial_color_lerp(pal->surface_1, pal->moss, 46));

    /* Tap Lock puck (267, 219) -> engages Touch Lock Shield (CC-02) */
    sim_tap(app, 267.0f, 219.0f, &ts);
    assert(strcmp(app->screen_id, "CC-02") == 0);
    assert(app->touch_locked == true);

    /* Hold center (180, 152) for 600ms -> fills ring and unlocks back to CC-01 */
    dial_touch_event_t h_down = { .x = 180.0f, .y = 152.0f, .pressed = true, .timestamp_ms = ts };
    dial_app_on_touch(app, &h_down);
    dial_app_tick(app, ts + 300u);
    assert(fabsf(app->unlock_hold_progress - 0.5f) < 0.05f);
    dial_app_tick(app, ts + 610u);
    assert(app->touch_locked == false);
    assert(strcmp(app->screen_id, "CC-01") == 0);
}

/* 4. Test Lock Screen Unlock -> Home Carousel Rim Rotation -> Enter Library */
static void test_interactive_lock_to_home_to_library(void) {
    dial_app_t *app = dial_app_get_state();
    dial_app_init(app, "LCK-02", DIAL_MODE_NIGHT);

    uint32_t ts = 9000u;
    /* Swipe up from bottom (180, 344) -> (180, 290) unlocks LCK-02 to HOM-01 */
    dial_touch_event_t su0 = { .x = 180.0f, .y = 344.0f, .pressed = true,  .timestamp_ms = ts };
    dial_touch_event_t su1 = { .x = 180.0f, .y = 290.0f, .pressed = true,  .timestamp_ms = ts + 30u };
    dial_touch_event_t su2 = { .x = 180.0f, .y = 290.0f, .pressed = false, .timestamp_ms = ts + 50u };
    dial_app_on_touch(app, &su0);
    dial_app_on_touch(app, &su1);
    dial_app_on_touch(app, &su2);
    ts += 60u;
    assert(strcmp(app->screen_id, "HOM-01") == 0);
    assert(app->home_focus_idx == 0);

    /* Rotate outer rim 16 deg clockwise on HOM-01 -> focuses Library (home_focus_idx == 1) */
    float x = 0.0f, y = 0.0f;
    dial_polar_to_xy(40.0f, 166.0f, &x, &y);
    dial_touch_event_t r0 = { .x = x, .y = y, .pressed = true, .timestamp_ms = ts };
    dial_app_on_touch(app, &r0);

    dial_polar_to_xy(48.0f, 166.0f, &x, &y);
    dial_touch_event_t r1 = { .x = x, .y = y, .pressed = true, .timestamp_ms = ts + 100u };
    dial_app_on_touch(app, &r1);

    dial_polar_to_xy(56.0f, 166.0f, &x, &y);
    dial_touch_event_t r2 = { .x = x, .y = y, .pressed = true, .timestamp_ms = ts + 200u };
    dial_app_on_touch(app, &r2);

    dial_touch_event_t r3 = { .x = x, .y = y, .pressed = false, .timestamp_ms = ts + 220u };
    dial_app_on_touch(app, &r3);
    ts += 250u;
    assert(app->home_focus_idx == 1);

    /* Tap center pebble (180, 118) -> opens Library (LIB-01) */
    sim_tap(app, 180.0f, 118.0f, &ts);
    assert(strcmp(app->screen_id, "LIB-01") == 0);
}

int main(void) {
    dial_hal_init(true);
    test_interactive_np01_seek_and_render();
    test_interactive_np02_quick_actions_flow();
    test_interactive_control_center_and_lock_shield();
    test_interactive_lock_to_home_to_library();
    dial_hal_shutdown();
    printf("[PASS] test_ui_interaction: all live interactive UI flows & framebuffer mutations verified.\n");
    return 0;
}

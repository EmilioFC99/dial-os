#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "dial_config.h"
#include "dial_hal.h"
#include "dial_ipc.h"
#include "dial_polar.h"
#include "dial_gestures.h"

static void assert_near(float actual, float expected, float eps, const char *msg) {
    if (fabsf(actual - expected) > eps) {
        fprintf(stderr, "FAIL: %s (actual=%.3f, expected=%.3f)\n", msg, actual, expected);
        assert(fabsf(actual - expected) <= eps);
    }
}

/* 1. Test 12-o'clock clockwise polar math & inner vinyl seek ring mapping */
static void test_polar_coordinates(void) {
    float r = 0.0f, deg = 0.0f;

    /* 12 o'clock on r=38 inner vinyl seek ring: (180, 142) -> 0 deg (0.00) */
    dial_xy_to_polar(180.0f, 142.0f, &r, &deg);
    assert_near(r, 38.0f, 0.01f, "12 o'clock radius == 38");
    assert_near(deg, 0.0f, 0.01f, "12 o'clock angle == 0 deg");
    assert_near(dial_deg_to_progress(deg), 0.0f, 0.001f, "0 deg == 0% progress");

    /* 3 o'clock on r=38 inner vinyl seek ring: (218, 180) -> 90 deg (0.25) */
    dial_xy_to_polar(218.0f, 180.0f, &r, &deg);
    assert_near(r, 38.0f, 0.01f, "3 o'clock radius == 38");
    assert_near(deg, 90.0f, 0.01f, "3 o'clock angle == 90 deg");
    assert_near(dial_deg_to_progress(deg), 0.25f, 0.001f, "90 deg == 25% progress");

    /* 6 o'clock on r=38 inner vinyl seek ring: (180, 218) -> 180 deg (0.50) */
    dial_xy_to_polar(180.0f, 218.0f, &r, &deg);
    assert_near(r, 38.0f, 0.01f, "6 o'clock radius == 38");
    assert_near(deg, 180.0f, 0.01f, "6 o'clock angle == 180 deg");
    assert_near(dial_deg_to_progress(deg), 0.50f, 0.001f, "180 deg == 50% progress");

    /* 9 o'clock on r=38 inner vinyl seek ring: (142, 180) -> 270 deg (0.75) */
    dial_xy_to_polar(142.0f, 180.0f, &r, &deg);
    assert_near(r, 38.0f, 0.01f, "9 o'clock radius == 38");
    assert_near(deg, 270.0f, 0.01f, "9 o'clock angle == 270 deg");
    assert_near(dial_deg_to_progress(deg), 0.75f, 0.001f, "270 deg == 75% progress");

    /* Round-trip polar -> xy -> polar across all 360 degrees */
    for (int a = 0; a < 360; a += 15) {
        float x = 0.0f, y = 0.0f, r2 = 0.0f, d2 = 0.0f;
        dial_polar_to_xy((float)a, 118.0f, &x, &y);
        dial_xy_to_polar(x, y, &r2, &d2);
        assert_near(r2, 118.0f, 0.02f, "roundtrip radius");
        assert_near(d2, (float)a, 0.02f, "roundtrip angle");
    }

    /* Angle wrap-around differences */
    assert_near(dial_angle_diff(350.0f, 10.0f), 20.0f, 0.01f, "CW wrap diff");
    assert_near(dial_angle_diff(10.0f, 350.0f), -20.0f, 0.01f, "CCW wrap diff");

    /* Zone classification */
    assert(dial_classify_zone(38.0f, true)  == DIAL_ZONE_VINYL_SEEK);
    assert(dial_classify_zone(12.0f, true)  == DIAL_ZONE_CORE);
    assert(dial_classify_zone(100.0f, true) == DIAL_ZONE_STAGE);
    assert(dial_classify_zone(166.0f, true) == DIAL_ZONE_RIM);
    assert(dial_classify_zone(185.0f, true) == DIAL_ZONE_OUTSIDE);
}

/* 2. Test NP-01 Direct Inner Vinyl Seek Ring Drag (24 <= r <= 54 with radial capture) */
static void test_np01_inner_ring_direct_seek(void) {
    dial_hal_init(true);
    dial_gesture_fsm_t fsm;
    dial_gesture_fsm_init(&fsm, true, 200000u); /* 200,000 ms track */

    /* Grab inner ring at 12 o'clock (r = 38, deg = 0) */
    dial_touch_event_t t0 = { .x = 180.0f, .y = 142.0f, .pressed = true, .timestamp_ms = 100 };
    dial_gesture_event_t ev = dial_gesture_fsm_step(&fsm, &t0);
    assert(ev.type == DIAL_EVENT_NONE);

    /* Slide clockwise to 3 o'clock (90 deg = 25% = 50,000 ms) */
    dial_touch_event_t t1 = { .x = 218.0f, .y = 180.0f, .pressed = true, .timestamp_ms = 140 };
    ev = dial_gesture_fsm_step(&fsm, &t1);
    assert(ev.type == DIAL_EVENT_SEEK);
    assert_near(ev.seek_progress, 0.25f, 0.01f, "direct ring slide 90 deg -> 25%");
    assert(ev.seek_ms == 50000u);

    /* Drift thumb outward to r = 110px at 6 o'clock (180 deg = 50% = 100,000 ms) — radial capture! */
    dial_touch_event_t t2 = { .x = 180.0f, .y = 290.0f, .pressed = true, .timestamp_ms = 180 };
    ev = dial_gesture_fsm_step(&fsm, &t2);
    assert(ev.type == DIAL_EVENT_SEEK);
    assert_near(ev.seek_progress, 0.50f, 0.01f, "radial capture at 180 deg -> 50%");
    assert(ev.seek_ms == 100000u);

    /* Verify DIAL_CMD_SEEK ("0103") IPC messages were emitted */
    dial_ipc_msg_t msg;
    assert(dial_hal_pop_ipc(&msg) == true);
    assert(strcmp(msg.opcode, DIAL_CMD_SEEK) == 0);
    assert(strcmp(msg.payload, "50000") == 0);

    assert(dial_hal_pop_ipc(&msg) == true);
    assert(strcmp(msg.opcode, DIAL_CMD_SEEK) == 0);
    assert(strcmp(msg.payload, "100000") == 0);
}

/* 3. Test NP-01 Long-Press (300ms) Turntable Scrub Mode (r < 145px) */
static void test_np01_turntable_scrub(void) {
    dial_hal_reset_stats();
    dial_gesture_fsm_t fsm;
    dial_gesture_fsm_init(&fsm, true, 120000u); /* 120,000 ms track */

    /* Touch vinyl at r = 100px, 12 o'clock (180, 80) -> deg = 0 */
    dial_touch_event_t down = { .x = 180.0f, .y = 80.0f, .pressed = true, .timestamp_ms = 1000 };
    dial_gesture_fsm_step(&fsm, &down);

    /* Hold for 300ms (DIAL_TURNTABLE_HOLD_MS) -> enters DIAL_GESTURE_TURNTABLE_SCRUB */
    dial_gesture_event_t ev = dial_gesture_fsm_tick(&fsm, 1300);
    assert(ev.type == DIAL_EVENT_TURNTABLE_LOCKED);
    assert(dial_hal_get_haptic_count() == 1);

    /* Rotate 90 deg clockwise to 3 o'clock (280, 180) -> seek_ms = 30,000 */
    dial_touch_event_t rot = { .x = 280.0f, .y = 180.0f, .pressed = true, .timestamp_ms = 1350 };
    ev = dial_gesture_fsm_step(&fsm, &rot);
    assert(ev.type == DIAL_EVENT_SEEK);
    assert_near(ev.seek_progress, 0.25f, 0.01f, "turntable scrub +90 deg -> 25%");
    assert(ev.seek_ms == 30000u);

    dial_ipc_msg_t msg;
    assert(dial_hal_pop_ipc(&msg) == true);
    assert(strcmp(msg.opcode, DIAL_CMD_SEEK) == 0);
    assert(strcmp(msg.payload, "30000") == 0);
}

/* 4. Test NP-01 Single Tap (Quick Actions NP-02) vs Double Tap (<260ms Play/Pause 0201) */
static void test_np01_tap_vs_double_tap(void) {
    dial_hal_reset_stats();
    dial_gesture_fsm_t fsm;
    dial_gesture_fsm_init(&fsm, true, 240000u);

    /* A: Single tap -> waits 260ms -> emits DIAL_EVENT_NP_QUICK_ACTIONS */
    dial_touch_event_t d1 = { .x = 180.0f, .y = 180.0f, .pressed = true,  .timestamp_ms = 100 };
    dial_touch_event_t u1 = { .x = 180.0f, .y = 180.0f, .pressed = false, .timestamp_ms = 150 };
    assert(dial_gesture_fsm_step(&fsm, &d1).type == DIAL_EVENT_NONE);
    assert(dial_gesture_fsm_step(&fsm, &u1).type == DIAL_EVENT_NONE);
    assert(dial_gesture_fsm_tick(&fsm, 300).type == DIAL_EVENT_NONE); /* Only 150ms elapsed */

    dial_gesture_event_t qa_ev = dial_gesture_fsm_tick(&fsm, 415);    /* >= 260ms elapsed */
    assert(qa_ev.type == DIAL_EVENT_NP_QUICK_ACTIONS);
    assert(dial_hal_get_ipc_count() == 0);

    /* B: Double tap within 260ms -> immediately emits DIAL_EVENT_PLAY_PAUSE + IPC "0201" */
    dial_touch_event_t d2a = { .x = 180.0f, .y = 180.0f, .pressed = true,  .timestamp_ms = 1000 };
    dial_touch_event_t u2a = { .x = 180.0f, .y = 180.0f, .pressed = false, .timestamp_ms = 1040 };
    dial_touch_event_t d2b = { .x = 181.0f, .y = 180.0f, .pressed = true,  .timestamp_ms = 1180 };
    dial_touch_event_t u2b = { .x = 181.0f, .y = 180.0f, .pressed = false, .timestamp_ms = 1220 };

    assert(dial_gesture_fsm_step(&fsm, &d2a).type == DIAL_EVENT_NONE);
    assert(dial_gesture_fsm_step(&fsm, &u2a).type == DIAL_EVENT_NONE);
    assert(dial_gesture_fsm_step(&fsm, &d2b).type == DIAL_EVENT_NONE);
    dial_gesture_event_t pp_ev = dial_gesture_fsm_step(&fsm, &u2b);
    assert(pp_ev.type == DIAL_EVENT_PLAY_PAUSE);

    dial_ipc_msg_t msg;
    assert(dial_hal_pop_ipc(&msg) == true);
    assert(strcmp(msg.opcode, DIAL_CMD_PLAY_PAUSE) == 0);
}

/* 5. Test Outer Rim 15-deg Detent & 3x Fast-Spin Velocity Multiplier (> 180 deg/s) */
static void test_rim_rotary_detents(void) {
    dial_hal_reset_stats();
    dial_gesture_fsm_t fsm;
    dial_gesture_fsm_init(&fsm, false, 180000u);

    /* Slow spin on r = 166px (15 deg in 300ms = 50 deg/s <= 180 deg/s -> step = +1) */
    float x = 0.0f, y = 0.0f;
    dial_polar_to_xy(0.0f, 166.0f, &x, &y);
    dial_touch_event_t t0 = { .x = x, .y = y, .pressed = true, .timestamp_ms = 100 };
    dial_gesture_fsm_step(&fsm, &t0);

    dial_polar_to_xy(8.0f, 166.0f, &x, &y);
    dial_touch_event_t t1 = { .x = x, .y = y, .pressed = true, .timestamp_ms = 250 };
    dial_gesture_fsm_step(&fsm, &t1);

    dial_polar_to_xy(16.0f, 166.0f, &x, &y);
    dial_touch_event_t t2 = { .x = x, .y = y, .pressed = true, .timestamp_ms = 400 };
    dial_gesture_event_t ev = dial_gesture_fsm_step(&fsm, &t2);
    assert(ev.type == DIAL_EVENT_RIM_DETENT);
    assert(ev.detent_steps == 1);
    assert(dial_hal_get_last_haptic_ms() == 8);

    /* Fast spin on r = 166px (16 deg in 30ms = 533 deg/s > 180 deg/s -> step = +3) */
    dial_polar_to_xy(32.0f, 166.0f, &x, &y);
    dial_touch_event_t t3 = { .x = x, .y = y, .pressed = true, .timestamp_ms = 430 };
    ev = dial_gesture_fsm_step(&fsm, &t3);
    assert(ev.type == DIAL_EVENT_RIM_DETENT);
    assert(ev.detent_steps == 3);
}

/* 6. Test Top Pull-Down (CC-01), Bottom Swipe-Up (NP-04/Unlock), and 450/600ms Holds */
static void test_edge_swipes_and_holds(void) {
    dial_gesture_fsm_t fsm;
    dial_gesture_fsm_init(&fsm, false, 180000u);

    /* Top pull-down from y = 16 -> y = 50 */
    dial_touch_event_t td0 = { .x = 180.0f, .y = 16.0f, .pressed = true,  .timestamp_ms = 100 };
    dial_touch_event_t td1 = { .x = 180.0f, .y = 50.0f, .pressed = true,  .timestamp_ms = 140 };
    dial_touch_event_t td2 = { .x = 180.0f, .y = 50.0f, .pressed = false, .timestamp_ms = 160 };
    dial_gesture_fsm_step(&fsm, &td0);
    assert(dial_gesture_fsm_step(&fsm, &td1).type == DIAL_EVENT_OPEN_CONTROL_CENTER);
    dial_gesture_fsm_step(&fsm, &td2);

    /* Bottom swipe-up from y = 340 -> y = 300 */
    dial_touch_event_t bu0 = { .x = 180.0f, .y = 340.0f, .pressed = true,  .timestamp_ms = 200 };
    dial_touch_event_t bu1 = { .x = 180.0f, .y = 300.0f, .pressed = true,  .timestamp_ms = 240 };
    dial_touch_event_t bu2 = { .x = 180.0f, .y = 300.0f, .pressed = false, .timestamp_ms = 260 };
    dial_gesture_fsm_step(&fsm, &bu0);
    assert(dial_gesture_fsm_step(&fsm, &bu1).type == DIAL_EVENT_BOTTOM_SWIPE_UP);
    dial_gesture_fsm_step(&fsm, &bu2);

    /* 450ms (CC-01 submenu) and 600ms (FIL-03 delete / CC-02 unlock) holds */
    dial_touch_event_t h0 = { .x = 180.0f, .y = 180.0f, .pressed = true, .timestamp_ms = 1000 };
    dial_gesture_fsm_step(&fsm, &h0);
    assert(dial_gesture_fsm_tick(&fsm, 1450).type == DIAL_EVENT_HOLD_450MS);
    assert(dial_gesture_fsm_tick(&fsm, 1600).type == DIAL_EVENT_HOLD_600MS);
}

/* 7. 1,000-Step Sanitizer & Fuzz Loop */
static void test_fuzz_1000_steps(void) {
    dial_gesture_fsm_t fsm;
    dial_gesture_fsm_init(&fsm, true, 300000u);
    uint32_t ts = 10000u;

    for (int i = 0; i < 1000; ++i) {
        float angle = (float)((i * 23) % 360);
        float rad   = (float)(10 + (i * 17) % 190);
        float x = 0.0f, y = 0.0f;
        dial_polar_to_xy(angle, rad, &x, &y);
        dial_touch_event_t ev = {
            .x = x,
            .y = y,
            .pressed = (i % 7 != 0),
            .timestamp_ms = ts
        };
        dial_gesture_fsm_step(&fsm, &ev);
        dial_gesture_fsm_tick(&fsm, ts + 5);
        ts += 16;
    }
}

int main(void) {
    test_polar_coordinates();
    test_np01_inner_ring_direct_seek();
    test_np01_turntable_scrub();
    test_np01_tap_vs_double_tap();
    test_rim_rotary_detents();
    test_edge_swipes_and_holds();
    test_fuzz_1000_steps();
    dial_hal_shutdown();
    printf("[PASS] test_polar_gestures: all polar math, NP-01 seek, tap/2x-tap, rim detent & fuzz tests passed.\n");
    return 0;
}

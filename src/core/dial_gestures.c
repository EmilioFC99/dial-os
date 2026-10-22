#include "dial_gestures.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define DIAL_DRIFT_TAP_MAX_PX     8.0f
#define DIAL_SWIPE_MIN_PX        22.0f
#define DIAL_RIM_LOCK_MIN_DEG     5.0f
#define DIAL_RING_SEEK_LOCK_DEG   3.0f
#define DIAL_TURNTABLE_MAX_R    145.0f

void dial_gesture_fsm_init(dial_gesture_fsm_t *fsm,
                           bool is_now_playing_screen,
                           uint32_t track_duration_ms) {
    if (!fsm) return;
    memset(fsm, 0, sizeof(*fsm));
    fsm->state                 = DIAL_GESTURE_IDLE;
    fsm->is_now_playing_screen = is_now_playing_screen;
    fsm->track_duration_ms     = (track_duration_ms > 0) ? track_duration_ms : 330000u; /* Default 5:30 */
}

static void emit_seek_ipc(dial_gesture_fsm_t *fsm, float deg, dial_gesture_event_t *ev) {
    float norm_deg = dial_normalize_deg(deg);
    fsm->current_seek_deg = norm_deg;
    ev->type          = DIAL_EVENT_SEEK;
    ev->deg           = norm_deg;
    ev->seek_progress = dial_deg_to_progress(norm_deg);
    ev->seek_ms       = dial_deg_to_seek_ms(norm_deg, fsm->track_duration_ms);

    char payload[32];
    snprintf(payload, sizeof(payload), "%u", (unsigned int)ev->seek_ms);
    dial_hal_send_ipc(DIAL_CMD_SEEK, payload);
}

dial_gesture_event_t dial_gesture_fsm_step(dial_gesture_fsm_t *fsm,
                                           const dial_touch_event_t *touch) {
    dial_gesture_event_t ev;
    memset(&ev, 0, sizeof(ev));
    if (!fsm || !touch) return ev;

    float r = 0.0f, deg = 0.0f;
    dial_xy_to_polar(touch->x, touch->y, &r, &deg);
    ev.x   = touch->x;
    ev.y   = touch->y;
    ev.r   = r;
    ev.deg = deg;

    /* 1. POINTER DOWN (New touch started) */
    if (touch->pressed && !fsm->touch_active) {
        fsm->touch_active   = true;
        fsm->start_x        = touch->x;
        fsm->start_y        = touch->y;
        fsm->start_r        = r;
        fsm->start_deg      = deg;
        fsm->start_ts_ms    = touch->timestamp_ms;
        fsm->last_x         = touch->x;
        fsm->last_y         = touch->y;
        fsm->last_r         = r;
        fsm->last_deg       = deg;
        fsm->last_ts_ms     = touch->timestamp_ms;
        fsm->rim_accum_deg  = 0.0f;
        fsm->smoothed_vel_deg_s = 0.0f;
        fsm->hold_450_fired = false;
        fsm->hold_600_fired = false;
        fsm->state          = DIAL_GESTURE_EVALUATING;

        /* If a pending single tap expired before this touch down, commit it first */
        if (fsm->pending_single_tap &&
            (touch->timestamp_ms - fsm->first_tap_up_ts_ms) > DIAL_DOUBLE_TAP_MAX_MS) {
            fsm->pending_single_tap = false;
        }
        return ev;
    }

    /* 2. POINTER MOVE (While pressed) */
    if (touch->pressed && fsm->touch_active) {
        const float dx_total = touch->x - fsm->start_x;
        const float dy_total = touch->y - fsm->start_y;
        const float drift    = sqrtf(dx_total * dx_total + dy_total * dy_total);
        const float d_theta  = dial_angle_diff(fsm->last_deg, deg);
        const uint32_t dt_ms = (touch->timestamp_ms > fsm->last_ts_ms)
                                   ? (touch->timestamp_ms - fsm->last_ts_ms)
                                   : 1u;
        const float inst_vel = (fabsf(d_theta) * 1000.0f) / (float)dt_ms;

        if (fsm->state == DIAL_GESTURE_EVALUATING) {
            /* Check Top Edge Pull-Down (y < 40 -> Control Center CC-01) */
            if (fsm->start_y < (float)DIAL_TOP_SWIPE_MAX_Y &&
                dy_total > DIAL_SWIPE_MIN_PX &&
                fabsf(dx_total) < 24.0f) {
                fsm->state = DIAL_GESTURE_TOP_PULL;
                fsm->pending_single_tap = false;
                ev.type = DIAL_EVENT_OPEN_CONTROL_CENTER;
                dial_hal_haptic_tick(15);
            }
            /* Check Bottom Edge Swipe-Up (y > 320 -> NP-04 Track Details / Unlock) */
            else if (fsm->start_y > (float)DIAL_BOTTOM_SWIPE_MIN_Y &&
                     dy_total < -DIAL_SWIPE_MIN_PX &&
                     fabsf(dx_total) < 24.0f) {
                fsm->state = DIAL_GESTURE_BOTTOM_SWIPE;
                fsm->pending_single_tap = false;
                ev.type = DIAL_EVENT_BOTTOM_SWIPE_UP;
                dial_hal_haptic_tick(15);
            }
            /* Check NP-01 Direct Inner Vinyl Ring Slide (24px <= start_r <= 54px) */
            else if (fsm->is_now_playing_screen &&
                     fsm->start_r >= (float)DIAL_VINYL_SEEK_GRAB_MIN &&
                     fsm->start_r <= (float)DIAL_VINYL_SEEK_GRAB_MAX &&
                     (fabsf(dial_angle_diff(fsm->start_deg, deg)) >= DIAL_RING_SEEK_LOCK_DEG ||
                      drift >= 4.0f)) {
                fsm->state = DIAL_GESTURE_VINYL_RING_SEEK;
                fsm->pending_single_tap = false;
                dial_hal_haptic_tick(8);
            }
            /* Check Outer Rim Virtual Click-Wheel (start_r >= 150px) */
            else if (fsm->start_r >= (float)DIAL_RIM_INNER_RADIUS) {
                if (fabsf(fsm->rim_accum_deg + d_theta) >= DIAL_RIM_LOCK_MIN_DEG) {
                    fsm->state = DIAL_GESTURE_RIM_ROTARY;
                    fsm->pending_single_tap = false;
                } else {
                    fsm->rim_accum_deg += d_theta;
                }
            }
            /* Check Horizontal Swipe in Stage/Center (Prev / Next / Back) */
            else if (fsm->start_r < (float)DIAL_RIM_INNER_RADIUS &&
                     fabsf(dx_total) > DIAL_SWIPE_MIN_PX &&
                     fabsf(dx_total) > 1.5f * fabsf(dy_total)) {
                fsm->state = DIAL_GESTURE_SWIPE_HORIZ;
                fsm->pending_single_tap = false;
                ev.type = (dx_total > 0.0f) ? DIAL_EVENT_SWIPE_RIGHT : DIAL_EVENT_SWIPE_LEFT;
            }
        }

        /* Handle ongoing locked states */
        if (fsm->state == DIAL_GESTURE_VINYL_RING_SEEK) {
            /* Radial capture: even if thumb drifts outside 24..54px, track angle directly */
            emit_seek_ipc(fsm, deg, &ev);
        } else if (fsm->state == DIAL_GESTURE_TURNTABLE_SCRUB) {
            /* Turntable scrub: angular delta rotates the inner vinyl seek ring */
            float next_deg = dial_normalize_deg(fsm->current_seek_deg + d_theta);
            emit_seek_ipc(fsm, next_deg, &ev);
        } else if (fsm->state == DIAL_GESTURE_RIM_ROTARY) {
            if (fsm->last_r >= (float)DIAL_RIM_INNER_RADIUS) {
                fsm->rim_accum_deg += d_theta;
            }
            fsm->smoothed_vel_deg_s = 0.6f * inst_vel + 0.4f * fsm->smoothed_vel_deg_s;

            if (fabsf(fsm->rim_accum_deg) >= DIAL_ROTARY_STEP_DEG) {
                int32_t raw_ticks = (int32_t)(fsm->rim_accum_deg / DIAL_ROTARY_STEP_DEG);
                fsm->rim_accum_deg -= (float)raw_ticks * DIAL_ROTARY_STEP_DEG;

                int32_t mult = (fsm->smoothed_vel_deg_s > DIAL_ROTARY_FAST_DEG_S) ? 3 : 1;
                ev.type              = DIAL_EVENT_RIM_DETENT;
                ev.detent_steps      = raw_ticks * mult;
                ev.angular_vel_deg_s = fsm->smoothed_vel_deg_s;
                dial_hal_haptic_tick(8);
            }
        }

        fsm->last_x     = touch->x;
        fsm->last_y     = touch->y;
        fsm->last_r     = r;
        fsm->last_deg   = deg;
        fsm->last_ts_ms = touch->timestamp_ms;
        return ev;
    }

    /* 3. POINTER UP (Touch released) */
    if (!touch->pressed && fsm->touch_active) {
        const float dx_total = touch->x - fsm->start_x;
        const float dy_total = touch->y - fsm->start_y;
        const float drift    = sqrtf(dx_total * dx_total + dy_total * dy_total);

        if (fsm->state == DIAL_GESTURE_EVALUATING && drift < DIAL_DRIFT_TAP_MAX_PX) {
            if (fsm->is_now_playing_screen) {
                /* Check if this is the 2nd tap within DIAL_DOUBLE_TAP_MAX_MS (260 ms) */
                if (fsm->pending_single_tap &&
                    (touch->timestamp_ms - fsm->first_tap_up_ts_ms) <= DIAL_DOUBLE_TAP_MAX_MS) {
                    fsm->pending_single_tap = false;
                    ev.type = DIAL_EVENT_PLAY_PAUSE;
                    dial_hal_haptic_tick(15);
                    dial_hal_send_ipc(DIAL_CMD_PLAY_PAUSE, "toggle");
                } else {
                    /* First tap: arm 260ms disambiguation window */
                    fsm->pending_single_tap = true;
                    fsm->first_tap_up_ts_ms = touch->timestamp_ms;
                    fsm->first_tap_x        = touch->x;
                    fsm->first_tap_y        = touch->y;
                }
            } else {
                ev.type = DIAL_EVENT_TAP;
            }
        }

        fsm->touch_active = false;
        fsm->state        = DIAL_GESTURE_IDLE;
        return ev;
    }

    return ev;
}

dial_gesture_event_t dial_gesture_fsm_tick(dial_gesture_fsm_t *fsm,
                                           uint32_t now_ms) {
    dial_gesture_event_t ev;
    memset(&ev, 0, sizeof(ev));
    if (!fsm) return ev;

    /* 1. Check NP-01 Single Tap timeout (>= 260 ms without second tap -> Open Quick Actions NP-02) */
    if (!fsm->touch_active && fsm->pending_single_tap) {
        if ((now_ms - fsm->first_tap_up_ts_ms) >= DIAL_DOUBLE_TAP_MAX_MS) {
            fsm->pending_single_tap = false;
            ev.type = DIAL_EVENT_NP_QUICK_ACTIONS;
            ev.x    = fsm->first_tap_x;
            ev.y    = fsm->first_tap_y;
            dial_xy_to_polar(ev.x, ev.y, &ev.r, &ev.deg);
            return ev;
        }
    }

    /* 2. Check active hold timers while finger is stationary */
    if (fsm->touch_active &&
        (fsm->state == DIAL_GESTURE_EVALUATING || fsm->state == DIAL_GESTURE_HOLD_LOCKED)) {
        const float dx    = fsm->last_x - fsm->start_x;
        const float dy    = fsm->last_y - fsm->start_y;
        const float drift = sqrtf(dx * dx + dy * dy);

        if (drift < DIAL_DRIFT_TAP_MAX_PX) {
            const uint32_t held_ms = (now_ms >= fsm->start_ts_ms) ? (now_ms - fsm->start_ts_ms) : 0u;

            /* NP-01 Turntable Scrub Hold: 300ms anywhere on vinyl (r < 145px) */
            if (fsm->is_now_playing_screen &&
                fsm->state == DIAL_GESTURE_EVALUATING &&
                fsm->start_r < DIAL_TURNTABLE_MAX_R &&
                held_ms >= DIAL_TURNTABLE_HOLD_MS) {
                fsm->state              = DIAL_GESTURE_TURNTABLE_SCRUB;
                fsm->pending_single_tap = false;
                fsm->current_seek_deg   = fsm->start_deg;
                ev.type                 = DIAL_EVENT_TURNTABLE_LOCKED;
                ev.x                    = fsm->last_x;
                ev.y                    = fsm->last_y;
                ev.r                    = fsm->last_r;
                ev.deg                  = fsm->last_deg;
                dial_hal_haptic_tick(15);
                return ev;
            }

            /* Standard 450ms & 600ms Holds (CC-01 submenus, FIL-02 sheet, FIL-03 delete, CC-02 unlock) */
            if (!fsm->is_now_playing_screen) {
                if (!fsm->hold_450_fired && held_ms >= DIAL_CC_HOLD_MENU_MS && held_ms < DIAL_HOLD_CONFIRM_MS) {
                    fsm->hold_450_fired = true;
                    fsm->state          = DIAL_GESTURE_HOLD_LOCKED;
                    ev.type             = DIAL_EVENT_HOLD_450MS;
                    ev.x                = fsm->last_x;
                    ev.y                = fsm->last_y;
                    ev.r                = fsm->last_r;
                    ev.deg              = fsm->last_deg;
                    dial_hal_haptic_tick(20);
                    return ev;
                }
                if (!fsm->hold_600_fired && held_ms >= DIAL_HOLD_CONFIRM_MS) {
                    fsm->hold_450_fired = true;
                    fsm->hold_600_fired = true;
                    fsm->state          = DIAL_GESTURE_HOLD_LOCKED;
                    ev.type             = DIAL_EVENT_HOLD_600MS;
                    ev.x                = fsm->last_x;
                    ev.y                = fsm->last_y;
                    ev.r                = fsm->last_r;
                    ev.deg              = fsm->last_deg;
                    dial_hal_haptic_tick(25);
                    return ev;
                }
            }
        }
    }

    return ev;
}

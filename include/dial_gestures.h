#ifndef DIAL_GESTURES_H
#define DIAL_GESTURES_H

#include <stdint.h>
#include <stdbool.h>
#include "dial_config.h"
#include "dial_polar.h"
#include "dial_hal.h"

/* Internal Gesture Lock States */
typedef enum {
    DIAL_GESTURE_IDLE             = 0,
    DIAL_GESTURE_EVALUATING       = 1,
    DIAL_GESTURE_TOP_PULL         = 2, /* y < 40 pull down -> CC-01 */
    DIAL_GESTURE_BOTTOM_SWIPE     = 3, /* y > 320 swipe up -> NP-04 / Unlock */
    DIAL_GESTURE_RIM_ROTARY       = 4, /* r >= 150 rotary virtual click-wheel */
    DIAL_GESTURE_VINYL_RING_SEEK  = 5, /* NP-01 direct slide on r=38 (24<=r<=54) with radial capture */
    DIAL_GESTURE_TURNTABLE_SCRUB  = 6, /* NP-01 300ms hold on r < 145 + rotate */
    DIAL_GESTURE_SWIPE_HORIZ      = 7, /* Horizontal swipe (Prev/Next/Back) */
    DIAL_GESTURE_HOLD_LOCKED      = 8  /* Long-press 450ms / 600ms */
} dial_gesture_state_t;

/* High-Level Disambiguated UI Events Emitted by FSM */
typedef enum {
    DIAL_EVENT_NONE                = 0,
    DIAL_EVENT_TAP                 = 1, /* Single tap on standard screen */
    DIAL_EVENT_NP_QUICK_ACTIONS    = 2, /* 1 touch on NP-01 -> Toggle NP-02 Quick Actions */
    DIAL_EVENT_PLAY_PAUSE          = 3, /* 2 quick touches (<260ms) on NP-01 -> Toggle Play/Pause (0201) */
    DIAL_EVENT_SEEK                = 4, /* Inner vinyl ring slide or turntable scrub -> Seek (0103) */
    DIAL_EVENT_TURNTABLE_LOCKED    = 5, /* 300ms hold on vinyl entered turntable scrub mode */
    DIAL_EVENT_RIM_DETENT          = 6, /* 15 deg rim step (+/-1 normal, +/-3 when >180 deg/s) */
    DIAL_EVENT_OPEN_CONTROL_CENTER = 7, /* Top edge pull-down (y < 40) -> CC-01 */
    DIAL_EVENT_BOTTOM_SWIPE_UP     = 8, /* Bottom edge swipe-up (y > 320) -> NP-04 / Unlock */
    DIAL_EVENT_SWIPE_LEFT          = 9, /* Horizontal swipe left */
    DIAL_EVENT_SWIPE_RIGHT         = 10,/* Horizontal swipe right */
    DIAL_EVENT_HOLD_450MS          = 11,/* 450ms hold (CC-01 submenu / FIL-02 action sheet) */
    DIAL_EVENT_HOLD_600MS          = 12 /* 600ms hold (FIL-03 delete confirm / CC-02 unlock) */
} dial_event_type_t;

typedef struct {
    dial_event_type_t type;
    float             x;
    float             y;
    float             r;
    float             deg;
    int32_t           detent_steps;   /* +/-1 or +/-3 for DIAL_EVENT_RIM_DETENT */
    float             seek_progress;  /* [0.0, 1.0) for DIAL_EVENT_SEEK */
    uint32_t          seek_ms;        /* Target ms for DIAL_CMD_SEEK ("0103") */
    float             angular_vel_deg_s;
} dial_gesture_event_t;

typedef struct {
    dial_gesture_state_t state;
    bool                 is_now_playing_screen; /* Enables NP-01 inner ring seek, turntable scrub & 2x tap */
    uint32_t             track_duration_ms;     /* Duration used to compute DIAL_CMD_SEEK ms */
    float                current_seek_deg;      /* Current track angle [0, 360) for turntable delta */

    /* Pointer down snapshot */
    bool                 touch_active;
    float                start_x;
    float                start_y;
    float                start_r;
    float                start_deg;
    uint32_t             start_ts_ms;

    /* Previous motion sample */
    float                last_x;
    float                last_y;
    float                last_r;
    float                last_deg;
    uint32_t             last_ts_ms;

    /* Accumulators */
    float                rim_accum_deg;
    float                smoothed_vel_deg_s;
    bool                 hold_450_fired;
    bool                 hold_600_fired;

    /* NP-01 single-tap vs double-tap (< 260 ms) disambiguation */
    bool                 pending_single_tap;
    uint32_t             first_tap_up_ts_ms;
    float                first_tap_x;
    float                first_tap_y;
} dial_gesture_fsm_t;

/* Initialize or reset gesture state machine */
void dial_gesture_fsm_init(dial_gesture_fsm_t *fsm,
                           bool is_now_playing_screen,
                           uint32_t track_duration_ms);

/* Process a touch down/move/up sample and return any triggered event */
dial_gesture_event_t dial_gesture_fsm_step(dial_gesture_fsm_t *fsm,
                                           const dial_touch_event_t *touch);

/* Advance timer-driven transitions (300ms turntable hold, 260ms single-tap commit, 450/600ms holds) */
dial_gesture_event_t dial_gesture_fsm_tick(dial_gesture_fsm_t *fsm,
                                           uint32_t now_ms);

#endif /* DIAL_GESTURES_H */

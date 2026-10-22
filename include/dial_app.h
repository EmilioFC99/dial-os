#ifndef DIAL_APP_H
#define DIAL_APP_H

#include <stdint.h>
#include <stdbool.h>
#include "dial_config.h"
#include "dial_theme.h"
#include "dial_hal.h"
#include "dial_gestures.h"

typedef struct {
    char               screen_id[24];
    char               prev_screen_id[24];
    dial_mode_t        mode;

    /* Transport & Now Playing (NP-01..05, LCK-02) */
    bool               is_playing;
    float              seek_progress;       /* [0.00, 0.999] mapped to 0°..359° on r=38px ring */
    int                volume;              /* 0..100 */
    int                brightness;          /* 0..100 */
    bool               is_favorited;        /* NP-02 heart puck at 234° */
    bool               is_shuffled;         /* NP-02 shuffle puck at 198° */
    uint32_t           volume_hud_until_ms; /* Auto-dismiss timestamp for NP-05 Volume HUD */

    /* Control Center (CC-01 & CC-02) */
    bool               wifi_on;             /* Row 1 Col 0: Fjord */
    bool               bt_on;               /* Row 1 Col 1: Fjord */
    int                active_mode_idx;     /* Row 1 Col 2: 0..5 mode_* in Moss */
    bool               high_gain;           /* Row 2 Col 0: true=gain_high (cloudberry), false=gain_low */
    bool               usb_charge;          /* Row 2 Col 1: true=moss, false=unlit */
    bool               touch_locked;        /* Row 2 Col 2: opens CC-02 Touch Lock Shield */
    float              unlock_hold_progress;/* 0.0..1.0 ring fill during 600ms hold on CC-02 */

    /* Navigation, Library & Files State (HOM, LIB-01..04, FIL-01..04) */
    int                home_focus_idx;      /* 0..5 app pebble focus on HOM-01..03 */
    int                list_focus_idx;      /* 0..4 row focus on LIB-01 */
    int                album_focus_idx;     /* 0..5 album cover focus on LIB-02 */
    int                track_focus_idx;     /* 0..4 track row focus on LIB-03 */
    int                playing_track_idx;   /* 0..4 active playing track on LIB-03 */
    int                artist_focus_idx;    /* 0..4 artist row focus on LIB-04 */
    char               artist_jump_letter;  /* 'A'..'Z' fast-spin letter bubble on LIB-04 */
    int                file_focus_idx;      /* 0..4 file/folder row focus on FIL-01 */
    float              delete_hold_progress;/* 0.0..1.0 ring/button fill during 600ms hold on FIL-03 */
    bool               file_deleted;        /* Set true after 600ms hold on FIL-03 */

    /* Embedded Gesture Disambiguation FSM */
    dial_gesture_fsm_t fsm;
} dial_app_t;

/* Access singleton application state (used by live simulator & screen renderers) */
dial_app_t *dial_app_get_state(void);

/* Reset application state to deterministic Fjäll design-sheet defaults */
void dial_app_init(dial_app_t *app, const char *initial_screen_id, dial_mode_t mode);

/* Switch active screen and configure gesture FSM mode */
void dial_app_set_screen(dial_app_t *app, const char *screen_id);

/* Feed a touch down/move/up event into the live UI state machine */
dial_gesture_event_t dial_app_on_touch(dial_app_t *app, const dial_touch_event_t *touch);

/* Feed a hardware button or simulator hotkey event into the live UI state machine */
void dial_app_on_key(dial_app_t *app, const dial_key_event_t *key, const dial_fb_t *fb);

/* Advance timers (260ms single-tap, 300ms turntable hold, 600ms unlock hold, 800ms Volume HUD fade) */
dial_gesture_event_t dial_app_tick(dial_app_t *app, uint32_t now_ms);

/* Render the current active screen of `app` into `fb` */
void dial_app_render(dial_app_t *app, dial_fb_t *fb, uint32_t tick_ms);

#endif /* DIAL_APP_H */

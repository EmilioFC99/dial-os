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

    /* Modes, Settings & Arcade State (MOD-00..06, SET-01..07, GAM-01..04) */
    int                mode_picker_focus;   /* 0..5 focused mode puck on MOD-00 */
    int                settings_focus_idx;  /* 0..4 focused category row on SET-01 */
    int                audio_focus_idx;     /* 0..4 focused row on SET-02 */
    bool               gapless_on;          /* Gapless toggle on SET-02 */
    int                dac_filter_idx;      /* 0..4 applied CS43131 DAC filter on SET-03 */
    int                eq_preset_idx;       /* 0=Acoustic, 1=Vocal, 2=Flat, 3=Custom on SET-04 */
    int                eq_focus_band;       /* 0..9 focused Parametric EQ band on SET-04 */
    float              eq_band_norm[10];    /* Normalized band heights [0.10..0.90] on SET-04 */
    int                wifi_focus_idx;      /* 0..4 focused row on SET-06 */
    int                bt_focus_idx;        /* 0..4 focused row on SET-07 */
    float              pong_paddle_deg;     /* Paddle center angle on GAM-02 Dial Pong */

    /* Live Non-Blocking Feedback Toast Overlay (auto-dismisses after 1600 ms) */
    char               toast_text[64];
    char               toast_icon[24];
    uint32_t           toast_tone;          /* 24-bit Fjäll palette color (moss/fjord/cloudberry/lingon) */
    uint32_t           toast_until_ms;

    /* Embedded Gesture Disambiguation FSM */
    dial_gesture_fsm_t fsm;
} dial_app_t;

/* Access singleton application state (used by live simulator & screen renderers) */
dial_app_t *dial_app_get_state(void);

/* Reset application state to deterministic Fjäll design-sheet defaults */
void dial_app_init(dial_app_t *app, const char *initial_screen_id, dial_mode_t mode);

/* Switch active screen and configure gesture FSM mode */
void dial_app_set_screen(dial_app_t *app, const char *screen_id);

/* Trigger a non-blocking real-time feedback toast pill over the active screen */
void dial_app_show_toast(dial_app_t *app,
                         const char *text,
                         const char *icon_name,
                         uint32_t tone,
                         uint32_t now_ms);

/* Feed a touch down/move/up event into the live UI state machine */
dial_gesture_event_t dial_app_on_touch(dial_app_t *app, const dial_touch_event_t *touch);

/* Feed a hardware button or simulator hotkey event into the live UI state machine */
void dial_app_on_key(dial_app_t *app, const dial_key_event_t *key, const dial_fb_t *fb);

/* Advance timers (260ms single-tap, 300ms turntable hold, 600ms unlock hold, 800ms Volume HUD fade) */
dial_gesture_event_t dial_app_tick(dial_app_t *app, uint32_t now_ms);

/* Render the current active screen of `app` into `fb` */
void dial_app_render(dial_app_t *app, dial_fb_t *fb, uint32_t tick_ms);

#endif /* DIAL_APP_H */

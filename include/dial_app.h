#ifndef DIAL_APP_H
#define DIAL_APP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dial_config.h"
#include "dial_theme.h"
#include "dial_hal.h"
#include "dial_polar.h"
#include "dial_gestures.h"
#include "dial_draw.h"
#include "dial_icons.h"
#include "dial_media.h"

/* Core DialOS Screen Identifiers */
typedef enum {
    DIAL_SCREEN_HOME        = 0, /* HOM-01..03: 6-Pebble Circular Launcher Carousel */
    DIAL_SCREEN_NOW_PLAYING = 1, /* NP-01..05:  Spinning Vinyl, Seek Ring, QA, Details, Vol HUD */
    DIAL_SCREEN_FILES       = 2, /* FIL-01..04: Real Directory & Audio File Browser */
    DIAL_SCREEN_LIBRARY     = 3, /* LIB-01..04: Indexed Music Library */
    DIAL_SCREEN_MODES       = 4, /* MOD-00..06: Operating Modes */
    DIAL_SCREEN_SETTINGS    = 5, /* SET-01..07: System & Audio Settings */
    DIAL_SCREEN_ARCADE      = 6  /* GAM-01..04: Circular Mini-Games */
} dial_screen_id_t;

#define DIAL_NAV_STACK_MAX      16
#define DIAL_FOLDER_DEPTH_MAX   16
#define DIAL_HOME_APP_COUNT     6

typedef struct {
    dial_screen_id_t screen;
    size_t           focus_idx;
} dial_nav_frame_t;

typedef struct {
    /* Theme & Global State */
    dial_mode_t        theme_mode;
    uint32_t           last_tick_ms;
    float              vinyl_spin_deg;

    /* Real Hierarchical Navigation Stack (nav_stack) */
    dial_screen_id_t   current_screen;
    dial_nav_frame_t   nav_stack[DIAL_NAV_STACK_MAX];
    size_t             nav_depth;

    /* Gesture Disambiguation FSM */
    dial_gesture_fsm_t gesture_fsm;

    /* Home Launcher State (HOM-01..03) */
    size_t             home_focus_idx; /* 0=NP, 1=Lib, 2=Files, 3=Modes, 4=Settings, 5=Arcade */
    bool               home_wheel_mode;/* HOM-03 6-pebble radial wheel on long-press */

    /* Files Directory Browser State (FIL-01..04) */
    size_t             files_focus_idx;
    size_t             folder_focus_stack[DIAL_FOLDER_DEPTH_MAX];
    size_t             folder_depth;
    bool               files_action_sheet_open;   /* FIL-02 long-press action sheet */
    bool               files_delete_confirm_open; /* FIL-03 600ms hold-to-delete dialog */

    /* Now Playing State & Overlays (NP-01..04) */
    bool               np_qa_open;          /* NP-02 single-tap Quick Actions overlay */
    bool               np_lyrics_open;      /* NP-03 Synchronized Lyrics overlay */
    bool               np_playlist_open;    /* Add to Playlist sheet */
    bool               np_details_open;     /* NP-04 swipe-up Track Details bottom sheet */
    bool               np_favorite;
    bool               np_added_to_playlist;
    size_t             np_playlist_focus_idx;
    bool               rim_slider_active;   /* Active drag on right-rim volume / left-rim brightness */
    bool               vinyl_grabbed;       /* True while seeking/scrubbing holds the vinyl disc */
    float              vinyl_grab_spin_deg; /* Disc rotation angle when finger grabbed the disc */
    float              vinyl_grab_finger_deg;/* Finger polar angle when finger grabbed the disc */
    float              vinyl_last_finger_deg;/* Previous finger polar angle during seek rotation */

    /* Control Center Global Overlay (CC-01..02) */
    bool               cc_open;
    bool               cc_touch_locked;     /* CC-02 hold-to-unlock shield */
    bool               cc_wifi_on;
    bool               cc_bt_on;
    bool               cc_high_gain;
    bool               cc_usb_charge;
    int                brightness_pct;
} dial_app_t;

/* Initialize the DialOS application state machine and navigation stack */
void dial_app_init(dial_app_t *app, dial_mode_t initial_mode);

/* Push a new child screen onto nav_stack and switch to it */
bool dial_app_push_screen(dial_app_t *app, dial_screen_id_t target_screen);

/* Universal "Go Back" action (pops active overlay, parent folder, or nav_stack) */
bool dial_app_pop_back(dial_app_t *app);

/* Process a raw touch sample through the gesture FSM and dispatch UI actions */
void dial_app_handle_touch(dial_app_t *app, const dial_touch_event_t *touch);

/* Process a physical button or simulator hotkey event */
void dial_app_handle_key(dial_app_t *app, const dial_key_event_t *key);

/* Advance timer-driven animations (vinyl spin, hold timers, HUD fade) */
void dial_app_tick(dial_app_t *app, uint32_t now_ms);

/* Render the active screen and any active overlay into the 360x360 circular framebuffer */
void dial_app_render(dial_app_t *app, dial_fb_t *fb);

#endif /* DIAL_APP_H */

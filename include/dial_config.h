#ifndef DIAL_CONFIG_H
#define DIAL_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* Display Physical Geometry (1.8" 360x360 Circular Touch LCD, 283 PPI) */
#define DIAL_SCREEN_WIDTH        360
#define DIAL_SCREEN_HEIGHT       360
#define DIAL_CENTER_X            180
#define DIAL_CENTER_Y            180
#define DIAL_RADIUS              180

/* Concentric Radial Zones (Fjäll Geometry Spec) */
#define DIAL_CORE_RADIUS         60   /* 0 <= r < 60: Core hero zone */
#define DIAL_STAGE_OUTER_RADIUS  150  /* 60 <= r < 150: Stage (lists, cards, pucks) */
#define DIAL_RIM_INNER_RADIUS    150  /* 150 <= r <= 180: Rotary Click-Wheel Rim */
#define DIAL_RIM_OUTER_RADIUS    180
#define DIAL_RIM_ARC_RADIUS      166  /* Radius of right-side volume & brightness arcs */

/* Now Playing (NP-01) Inner Vinyl Seek Ring (0° = 0% at 12 o'clock -> 359° = 99%) */
#define DIAL_VINYL_SEEK_RADIUS   38   /* Visual progress/seek ring radius around center hole */
#define DIAL_VINYL_SEEK_GRAB_MIN 24   /* Inner grab radius for direct ring slide */
#define DIAL_VINYL_SEEK_GRAB_MAX 54   /* Outer grab radius for direct ring slide */
#define DIAL_TURNTABLE_HOLD_MS   300  /* Long-press on vinyl (r < 145) enters turntable seek */

/* Quick Actions (NP-02) Bottom Arc Geometry */
#define DIAL_QA_ARC_RADIUS       118  /* 4 round action pucks at 234°, 198°, 162°, 126° */
#define DIAL_QA_PUCK_DIAMETER    52

/* Control Center (CC-01) 3x2 Icon Puck Grid */
#define DIAL_CC_PUCK_DIAMETER    66
#define DIAL_CC_HOLD_MENU_MS     450  /* Hold Wi-Fi, Bluetooth, or Mode puck opens submenu */

/* Global Edge & Destructive Thresholds */
#define DIAL_TOP_SWIPE_MAX_Y     40   /* Swipe down from y < 40 opens Control Center (CC-01) */
#define DIAL_BOTTOM_SWIPE_MIN_Y  320  /* Swipe up from y > 320 opens Track Details (NP-04) / unlocks */
#define DIAL_DOUBLE_TAP_MAX_MS   260  /* 2 quick touches on NP-01 toggles Play/Pause */
#define DIAL_HOLD_CONFIRM_MS     600  /* Hold duration for Lingon delete confirm (FIL-03) & unlock (CC-02) */
#define DIAL_RESTART_THRESHOLD_S 3    /* <= 3s goes to previous track, > 3s restarts current track */

/* Rotary Virtual Click-Wheel Sensitivity */
#define DIAL_ROTARY_STEP_DEG     15.0f /* 15° of rim rotation = 1 detent + 8ms haptic tick */
#define DIAL_ROTARY_FAST_DEG_S   180.0f/* Angular velocity > 180°/s = 3 items per detent */

/* Performance & Refresh */
#define DIAL_TARGET_FPS          60
#define DIAL_MAX_RAM_BYTES       (24 * 1024 * 1024) /* 24.0 MB strict UI footprint cap */
#define DIAL_VINYL_RPM           16.67f             /* Spinning vinyl speed (50% slower) */

#endif /* DIAL_CONFIG_H */

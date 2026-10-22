#ifndef DIAL_POLAR_H
#define DIAL_POLAR_H

#include <stdint.h>
#include <stdbool.h>
#include "dial_config.h"
#include "dial_theme.h"

/* Concentric Radial Zones on the 360x360 Circular Display */
typedef enum {
    DIAL_ZONE_CORE       = 0, /* 0 <= r < 60: Core hero zone (includes inner vinyl ring) */
    DIAL_ZONE_VINYL_SEEK = 1, /* 24 <= r <= 54: Inner vinyl seek ring grab band on NP-01 */
    DIAL_ZONE_STAGE      = 2, /* 60 <= r < 150: Stage (lists, cards, pucks) */
    DIAL_ZONE_RIM        = 3, /* 150 <= r <= 180: Virtual Rotary Click-Wheel Rim */
    DIAL_ZONE_OUTSIDE    = 4  /* r > 180: Outside physical circular bezel */
} dial_zone_t;

/* Convert Cartesian screen coordinates (x, y) to Polar (r, deg) where:
 *   Center = (180, 180)
 *   0.0°   = 12 o'clock (top)
 *   90.0°  = 3 o'clock  (right)
 *   180.0° = 6 o'clock  (bottom)
 *   270.0° = 9 o'clock  (left)
 */
void dial_xy_to_polar(float x, float y, float *out_r, float *out_deg);

/* Convert Polar (deg clockwise from 12 o'clock, radius r) to Cartesian (x, y) */
void dial_polar_to_xy(float deg, float r, float *out_x, float *out_y);

/* Normalize any angle in degrees into [0.0f, 360.0f) */
float dial_normalize_deg(float deg);

/* Shortest signed angular difference (deg_to - deg_from) in (-180.0f, +180.0f] */
float dial_angle_diff(float deg_from, float deg_to);

/* Classify radius r into concentric zone (with optional vinyl seek grab detection) */
dial_zone_t dial_classify_zone(float r, bool check_vinyl_seek_band);

/* Map inner vinyl seek ring angle [0°, 360°) -> normalized track progress [0.0, 1.0) */
float dial_deg_to_progress(float deg);

/* Map inner vinyl seek ring angle [0°, 360°) -> target seek position in milliseconds */
uint32_t dial_deg_to_seek_ms(float deg, uint32_t track_duration_ms);

#endif /* DIAL_POLAR_H */

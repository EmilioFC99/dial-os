#include "dial_polar.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float dial_normalize_deg(float deg) {
    float d = fmodf(deg, 360.0f);
    if (d < 0.0f) {
        d += 360.0f;
    }
    /* Guard against -0.0f or 360.0f float rounding */
    if (d >= 360.0f || d == -0.0f) {
        d = 0.0f;
    }
    return d;
}

void dial_xy_to_polar(float x, float y, float *out_r, float *out_deg) {
    const float dx = x - (float)DIAL_CENTER_X;
    const float dy = y - (float)DIAL_CENTER_Y;
    const float r  = sqrtf(dx * dx + dy * dy);

    if (out_r) {
        *out_r = r;
    }
    if (out_deg) {
        if (r < 1e-4f) {
            *out_deg = 0.0f;
        } else {
            /* 12 o'clock (dx=0, dy<0) is 0 deg, clockwise positive: atan2(dx, -dy) */
            const float rad = atan2f(dx, -dy);
            const float deg = rad * (180.0f / (float)M_PI);
            *out_deg = dial_normalize_deg(deg);
        }
    }
}

void dial_polar_to_xy(float deg, float r, float *out_x, float *out_y) {
    const float rad = deg * ((float)M_PI / 180.0f);
    if (out_x) {
        *out_x = (float)DIAL_CENTER_X + r * sinf(rad);
    }
    if (out_y) {
        *out_y = (float)DIAL_CENTER_Y - r * cosf(rad);
    }
}

float dial_angle_diff(float deg_from, float deg_to) {
    float diff = fmodf(deg_to - deg_from, 360.0f);
    if (diff > 180.0f) {
        diff -= 360.0f;
    } else if (diff <= -180.0f) {
        diff += 360.0f;
    }
    return diff;
}

dial_zone_t dial_classify_zone(float r, bool check_vinyl_seek_band) {
    if (r > (float)DIAL_RIM_OUTER_RADIUS) {
        return DIAL_ZONE_OUTSIDE;
    }
    if (check_vinyl_seek_band &&
        r >= (float)DIAL_VINYL_SEEK_GRAB_MIN &&
        r <= (float)DIAL_VINYL_SEEK_GRAB_MAX) {
        return DIAL_ZONE_VINYL_SEEK;
    }
    if (r < (float)DIAL_CORE_RADIUS) {
        return DIAL_ZONE_CORE;
    }
    if (r < (float)DIAL_STAGE_OUTER_RADIUS) {
        return DIAL_ZONE_STAGE;
    }
    return DIAL_ZONE_RIM;
}

float dial_deg_to_progress(float deg) {
    float norm = dial_normalize_deg(deg);
    float p = norm / 360.0f;
    if (p < 0.0f) p = 0.0f;
    if (p >= 1.0f) p = 0.9999f;
    return p;
}

uint32_t dial_deg_to_seek_ms(float deg, uint32_t track_duration_ms) {
    float p = dial_deg_to_progress(deg);
    return (uint32_t)(p * (float)track_duration_ms + 0.5f);
}

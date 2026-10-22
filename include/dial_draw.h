#ifndef DIAL_DRAW_H
#define DIAL_DRAW_H

#include <stdint.h>
#include <stdbool.h>
#include "dial_config.h"
#include "dial_theme.h"
#include "dial_hal.h"
#include "dial_polar.h"

/* Procedural Album Artwork Variants (matching fjall-design-system.html) */
typedef enum {
    DIAL_ART_FJORD  = 0,
    DIAL_ART_RINGS  = 1,
    DIAL_ART_AURORA = 2,
    DIAL_ART_MOSS   = 3,
    DIAL_ART_BERRY  = 4,
    DIAL_ART_SNOW   = 5
} dial_art_kind_t;

/* Fisheye Rim List Row Descriptor (matching rimList in fjall-design-system.html) */
typedef struct {
    const char *icon_name;   /* Optional Fjäll icon name (e.g. "heart", "playlist") */
    uint32_t    icon_tint;   /* 0 = default text_2, otherwise custom token color */
    const char *lead_text;   /* Optional tabular number lead (e.g. "01", "02") */
    uint32_t    lead_color;
    const char *title;       /* Primary row title */
    const char *sub;         /* Secondary subtext (shown when focused or always_sub) */
    bool        always_sub;
    bool        marquee;     /* .train horizontal marquee on title when true */
    const char *value_text;  /* Right-side value string or chip text */
    bool        value_is_chip;
    uint32_t    chip_color;  /* 0 = neutral surface_3, otherwise moss/fjord/cloudberry */
    bool        has_toggle;
    bool        toggle_on;
    bool        has_chev;
    bool        has_check;
    bool        playing_eq;  /* Animated moss equalizer bars + "Playing" */
} dial_rim_row_t;

/* Retrieve active Fjäll palette (DIAL_PALETTE_NIGHT or DIAL_PALETTE_DAY) */
const dial_palette_t *dial_get_palette(dial_mode_t mode);

/* Linear RGB color interpolation: t=0 -> c0, t=255 -> c1 (CSS color-mix equivalent) */
uint32_t dial_color_lerp(uint32_t c0, uint32_t c1, uint8_t t);

/* Fill entire circular viewport (r <= 180px) with background color, zeroing r > 180px */
void dial_draw_clear(dial_fb_t *fb, uint32_t bg_color);

/* Enforce strict r = 180px circular bezel mask (outside pixels set to 0x00000000) */
void dial_draw_circle_mask(dial_fb_t *fb);

/* Blend a single pixel with alpha (0..255), clipped to 360x360 and r <= 180px */
void dial_draw_pixel_aa(dial_fb_t *fb, int x, int y, uint32_t rgb, uint8_t alpha);

/* Anti-aliased filled & stroked circles */
void dial_draw_circle_fill(dial_fb_t *fb, float cx, float cy, float r, uint32_t rgb, uint8_t alpha);
void dial_draw_circle_stroke(dial_fb_t *fb, float cx, float cy, float r, float stroke_w, uint32_t rgb, uint8_t alpha);

/* Anti-aliased thick circular arc (a0_deg -> a1_deg clockwise from 12 o'clock) */
void dial_draw_arc_aa(dial_fb_t *fb,
                      float cx,
                      float cy,
                      float r,
                      float a0_deg,
                      float a1_deg,
                      float stroke_w,
                      uint32_t rgb,
                      uint8_t alpha,
                      bool round_caps,
                      bool dashed);

/* Anti-aliased line segment with round caps */
void dial_draw_line_aa(dial_fb_t *fb,
                       float x0,
                       float y0,
                       float x1,
                       float y1,
                       float stroke_w,
                       uint32_t rgb,
                       uint8_t alpha);

/* Fjäll Outer Rim Arc (track + value + optional thumb knob, r = 166px default) */
void dial_draw_rim_arc(dial_fb_t *fb,
                       const dial_palette_t *pal,
                       float a0_deg,
                       float a1_deg,
                       float value_01,
                       uint32_t accent_rgb,
                       float r,
                       float stroke_w,
                       bool show_track,
                       bool dashed,
                       bool show_knob);

/* Fjäll Right-Rim Scroll Indicator (52° to 128°, r = 170px, w = 3px) */
void dial_draw_scroll_rim(dial_fb_t *fb,
                          const dial_palette_t *pal,
                          float pos_01,
                          float len_01);

/* Locked NP-01 Inner Vinyl Circle Progress & Seek Ring (r = 38px, 0°=0% -> 359°=99%) */
void dial_draw_vinyl_seek_ring(dial_fb_t *fb,
                               const dial_palette_t *pal,
                               float progress_01);

/* Anti-aliased rounded rectangle (with optional fill and inset border) */
void dial_draw_round_rect(dial_fb_t *fb,
                          float x,
                          float y,
                          float w,
                          float h,
                          float radius,
                          uint32_t fill_rgb,
                          uint8_t fill_alpha,
                          float border_w,
                          uint32_t border_rgb,
                          uint8_t border_alpha);

/* Fjäll UI Components: Pill, Chip, Toggle Switch, 72% Scrim, Top Grab Handle */
void dial_draw_pill(dial_fb_t *fb,
                    const dial_palette_t *pal,
                    int cx,
                    int y,
                    const char *icon_name,
                    const char *label,
                    bool active);

void dial_draw_chip(dial_fb_t *fb,
                    const dial_palette_t *pal,
                    int x,
                    int y,
                    const char *label,
                    uint32_t accent_rgb,
                    int *out_width);

void dial_draw_toggle(dial_fb_t *fb,
                      const dial_palette_t *pal,
                      int x,
                      int y,
                      bool on);

void dial_draw_scrim(dial_fb_t *fb, const dial_palette_t *pal, uint8_t alpha);
void dial_draw_top_grab(dial_fb_t *fb, const dial_palette_t *pal);
void dial_draw_status_bar(dial_fb_t *fb, const dial_palette_t *pal, const char *time_str, int battery_pct, uint32_t override_rgb);
void dial_draw_header(dial_fb_t *fb, const dial_palette_t *pal, const char *overline, const char *title, int y);
void dial_draw_hint(dial_fb_t *fb, const dial_palette_t *pal, const char *icon_name, const char *text, uint32_t override_rgb);
void dial_draw_horizon(dial_fb_t *fb, const dial_palette_t *pal);

/* Fjäll Fisheye Rim List (focused d=0: 284x52 r=18, d=1: 258x44 r=15, d>=2: 226x44 r=15) */
void dial_draw_rim_list(dial_fb_t *fb,
                        const dial_palette_t *pal,
                        const dial_rim_row_t *rows,
                        size_t row_count,
                        size_t focus_index,
                        int top_y,
                        uint32_t tick_ms);

/* Procedural Album Cover Art + Full-Bleed Spinning Vinyl Grooves & Vignette */
void dial_draw_art(dial_fb_t *fb,
                   const dial_palette_t *pal,
                   dial_art_kind_t kind,
                   int cx,
                   int cy,
                   int diameter,
                   bool vinyl_grooves,
                   float spin_deg);

/* Typography & .train Horizontal Marquee Text Engine */
int  dial_measure_text(const char *text, int font_px, int letter_spacing_px);
void dial_draw_text(dial_fb_t *fb,
                    const char *text,
                    int x,
                    int y,
                    int font_px,
                    uint32_t rgb,
                    uint8_t alpha,
                    bool centered);

void dial_draw_text_marquee(dial_fb_t *fb,
                            const char *text,
                            int clip_x,
                            int clip_y,
                            int clip_w,
                            int font_px,
                            uint32_t rgb,
                            uint8_t alpha,
                            uint32_t tick_ms);

#endif /* DIAL_DRAW_H */

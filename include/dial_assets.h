/**
 * @file dial_assets.h
 * @brief Pre-baked CoreText 8-bit anti-aliased fonts & Fjäll Design System vinyl textures.
 */
#ifndef DIAL_ASSETS_H
#define DIAL_ASSETS_H

#include <stdint.h>

#define DIAL_ASSET_CHAR_COUNT 111
#define DIAL_VINYL_TEX_DIM    180
#define DIAL_VINYL_TEX_COUNT  6

typedef enum {
    DIAL_FONT_FACE_SANS_10 = 0,
    DIAL_FONT_FACE_SANS_11 = 1,
    DIAL_FONT_FACE_SANS_12 = 2,
    DIAL_FONT_FACE_SANS_13 = 3,
    DIAL_FONT_FACE_SANS_15 = 4,
    DIAL_FONT_FACE_SANS_16 = 5,
    DIAL_FONT_FACE_SANS_18 = 6,
    DIAL_FONT_FACE_SANS_20 = 7,
    DIAL_FONT_FACE_SANS_22 = 8,
    DIAL_FONT_FACE_SANS_28 = 9,
    DIAL_FONT_FACE_SANS_44 = 10,
    DIAL_FONT_FACE_SANS_52 = 11,
    DIAL_FONT_FACE_MONO_11 = 12,
    DIAL_FONT_FACE_MONO_13 = 13,
    DIAL_FONT_FACE_COUNT = 14
} dial_font_face_id_t;

typedef struct {
    uint32_t codepoint;
    uint16_t width;
    uint16_t height;
    int16_t  bearing_x;
    int16_t  bearing_y;
    int16_t  advance_x;
    uint32_t bitmap_offset;
} dial_baked_glyph_t;

typedef struct {
    float                    size_px;
    int                      ascent;
    int                      descent;
    int                      line_height;
    int                      is_mono;
    const dial_baked_glyph_t *glyphs;
} dial_baked_font_t;

extern const uint8_t           g_dial_font_atlas[286303];
extern const dial_baked_font_t g_dial_fonts[DIAL_FONT_FACE_COUNT];
extern const uint16_t          g_dial_vinyl_covers[DIAL_VINYL_TEX_COUNT][DIAL_VINYL_TEX_DIM * DIAL_VINYL_TEX_DIM];

#endif /* DIAL_ASSETS_H */

/* dial_theme_fjall.h — generated from the Fjäll design system v0.1.0
 * Replaces the Stone palette in include/dial_theme.h. Do not edit by hand.
 */
#ifndef DIAL_THEME_FJALL_H
#define DIAL_THEME_FJALL_H

#include <stdint.h>

typedef enum { DIAL_MODE_NIGHT = 0, DIAL_MODE_DAY } dial_mode_t;

typedef struct {
    uint32_t bg         ; /* Pine Night · screen background */
    uint32_t surface_1  ; /* Surface 1 · rows, cards */
    uint32_t surface_2  ; /* Surface 2 · focused row, icon wells */
    uint32_t surface_3  ; /* Surface 3 · pressed, tracks, grips */
    uint32_t line       ; /* Hairline · borders, dividers */
    uint32_t text_1     ; /* Snow · primary text */
    uint32_t text_2     ; /* Lichen · secondary text */
    uint32_t text_3     ; /* Granite · hints, disabled */
    uint32_t moss       ; /* Mossa · primary accent */
    uint32_t fjord      ; /* Fjord · connectivity, info */
    uint32_t cloudberry ; /* Hjortron (cloudberry) · warmth, warning, score */
    uint32_t lingon     ; /* Lingon · destructive, error */
    uint32_t birch      ; /* Björk sand · neutral highlight, brightness */
    uint32_t heather    ; /* Ljung (heather) · modes app tint */
    uint32_t on_accent  ; /* Text/icon on a filled accent */
} dial_palette_t;

/* Polar Night — 0xRRGGBB  (RGB565 in comment) */
static const dial_palette_t DIAL_PALETTE_NIGHT = {
    .bg          = 0x121615,  /* 0x10A2 */
    .surface_1   = 0x1A1F1D,  /* 0x18E3 */
    .surface_2   = 0x232927,  /* 0x2144 */
    .surface_3   = 0x2E3532,  /* 0x29A6 */
    .line        = 0x2A312E,  /* 0x2985 */
    .text_1      = 0xECEEE8,  /* 0xEF7D */
    .text_2      = 0xA2ABA5,  /* 0xA554 */
    .text_3      = 0x6F7974,  /* 0x6BCE */
    .moss        = 0x8DB596,  /* 0x8DB2 */
    .fjord       = 0x8FB3C9,  /* 0x8D99 */
    .cloudberry  = 0xE2A66A,  /* 0xE52D */
    .lingon      = 0xE07F76,  /* 0xE3EE */
    .birch       = 0xD8CFBD,  /* 0xDE77 */
    .heather     = 0xB7A0C6,  /* 0xB518 */
    .on_accent   = 0x0F1D14,  /* 0x08E2 */
};

/* Birch Day — 0xRRGGBB  (RGB565 in comment) */
static const dial_palette_t DIAL_PALETTE_DAY = {
    .bg          = 0xF4F1EA,  /* 0xF79D */
    .surface_1   = 0xEAE6DD,  /* 0xEF3B */
    .surface_2   = 0xDFDAD0,  /* 0xDEDA */
    .surface_3   = 0xD0CABE,  /* 0xD657 */
    .line        = 0xD9D3C7,  /* 0xDE98 */
    .text_1      = 0x1C2220,  /* 0x1904 */
    .text_2      = 0x545D58,  /* 0x52EB */
    .text_3      = 0x7E8681,  /* 0x7C30 */
    .moss        = 0x47755A,  /* 0x43AB */
    .fjord       = 0x3B6A86,  /* 0x3B50 */
    .cloudberry  = 0xA9652B,  /* 0xAB25 */
    .lingon      = 0xAE463D,  /* 0xAA27 */
    .birch       = 0x857456,  /* 0x83AA */
    .heather     = 0x76598A,  /* 0x72D1 */
    .on_accent   = 0xFFFFFF,  /* 0xFFFF */
};

/* Geometry (device px, 360x360) */
#define DIAL_RIM_INNER_R      150
#define DIAL_RIM_ARC_R        166
#define DIAL_CORE_R           60
#define DIAL_VINYL_SEEK_R     38   /* Inner vinyl progress/seek ring: 0 deg = 0%, 359 deg = 99% */
#define DIAL_SAFE_SQUARE      254
#define DIAL_TOUCH_MIN        44
#define DIAL_DETENT_DEG       15

/* Spacing (4px grid) */
#define DIAL_SPACE_1          4
#define DIAL_SPACE_2          8
#define DIAL_SPACE_3          12
#define DIAL_SPACE_4          16
#define DIAL_SPACE_5          20
#define DIAL_SPACE_6          24
#define DIAL_SPACE_7          32
#define DIAL_SPACE_8          48

/* Radius */
#define DIAL_RADIUS_XS       8
#define DIAL_RADIUS_SM       12
#define DIAL_RADIUS_MD       18
#define DIAL_RADIUS_LG       24
#define DIAL_RADIUS_XL       30
#define DIAL_RADIUS_PILL     999

/* Stroke */
#define DIAL_STROKE_ICON_X100 175
#define DIAL_STROKE_FOCUS_X10 15
#define DIAL_STROKE_RIM       4

/* Type scale: px, weight */
#define DIAL_FONT_DISPLAY   64, 300
#define DIAL_FONT_HERO      40, 400
#define DIAL_FONT_TITLE     22, 600
#define DIAL_FONT_HEADLINE  17, 600
#define DIAL_FONT_BODY      15, 500
#define DIAL_FONT_SUB       13, 500
#define DIAL_FONT_CAPTION   11, 600

/* Motion (ms) */
#define DIAL_MOTION_INSTANT  80
#define DIAL_MOTION_QUICK    140
#define DIAL_MOTION_SETTLE   200
#define DIAL_MOTION_GLIDE    320
#define DIAL_MOTION_DRIFT    1200
#define DIAL_SCRIM_ALPHA      184  /* 72% of 255 */

#endif /* DIAL_THEME_FJALL_H */

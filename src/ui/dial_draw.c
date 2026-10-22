#include "dial_draw.h"
#include "dial_icons.h"
#include "dial_assets.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const dial_palette_t *dial_get_palette(dial_mode_t mode) {
    return (mode == DIAL_MODE_DAY) ? &DIAL_PALETTE_DAY : &DIAL_PALETTE_NIGHT;
}

uint32_t dial_color_lerp(uint32_t c0, uint32_t c1, uint8_t t) {
    uint32_t inv = 255u - (uint32_t)t;
    uint32_t r0 = (c0 >> 16) & 0xFFu, g0 = (c0 >> 8) & 0xFFu, b0 = c0 & 0xFFu;
    uint32_t r1 = (c1 >> 16) & 0xFFu, g1 = (c1 >> 8) & 0xFFu, b1 = c1 & 0xFFu;
    uint32_t r  = (r0 * inv + r1 * (uint32_t)t + 127u) / 255u;
    uint32_t g  = (g0 * inv + g1 * (uint32_t)t + 127u) / 255u;
    uint32_t b  = (b0 * inv + b1 * (uint32_t)t + 127u) / 255u;
    return (r << 16) | (g << 8) | b;
}

void dial_draw_clear(dial_fb_t *fb, uint32_t bg_color) {
    if (!fb) return;
    const float cx = (float)DIAL_CENTER_X;
    const float cy = (float)DIAL_CENTER_Y;
    const float max_r = (float)DIAL_RADIUS;

    for (int y = 0; y < (int)fb->height; ++y) {
        const float dy = ((float)y + 0.5f) - cy;
        for (int x = 0; x < (int)fb->width; ++x) {
            const float dx = ((float)x + 0.5f) - cx;
            const float r  = sqrtf(dx * dx + dy * dy);
            if (r > max_r) {
                fb->pixels[y * fb->width + x] = 0x00000000u;
            } else {
                fb->pixels[y * fb->width + x] = 0xFF000000u | (bg_color & 0xFFFFFFu);
            }
        }
    }
}

void dial_draw_circle_mask(dial_fb_t *fb) {
    if (!fb) return;
    const float cx = (float)DIAL_CENTER_X;
    const float cy = (float)DIAL_CENTER_Y;
    const float max_r = (float)DIAL_RADIUS;

    for (int y = 0; y < (int)fb->height; ++y) {
        const float dy = ((float)y + 0.5f) - cy;
        for (int x = 0; x < (int)fb->width; ++x) {
            const float dx = ((float)x + 0.5f) - cx;
            const float r  = sqrtf(dx * dx + dy * dy);
            if (r > max_r) {
                fb->pixels[y * fb->width + x] = 0x00000000u;
            }
        }
    }
}

void dial_draw_pixel_aa(dial_fb_t *fb, int x, int y, uint32_t rgb, uint8_t alpha) {
    if (!fb || alpha == 0) return;
    if (x < 0 || x >= (int)fb->width || y < 0 || y >= (int)fb->height) return;

    /* Enforce circular bezel clipping: zero writes outside r = 180px */
    const float dx = ((float)x + 0.5f) - (float)DIAL_CENTER_X;
    const float dy = ((float)y + 0.5f) - (float)DIAL_CENTER_Y;
    if ((dx * dx + dy * dy) > ((float)DIAL_RADIUS * (float)DIAL_RADIUS)) {
        return;
    }

    uint32_t *dst_ptr = &fb->pixels[y * fb->width + x];
    if (alpha == 255) {
        *dst_ptr = 0xFF000000u | (rgb & 0xFFFFFFu);
        return;
    }

    uint32_t dst_rgb = *dst_ptr & 0xFFFFFFu;
    uint32_t blended = dial_color_lerp(dst_rgb, rgb & 0xFFFFFFu, alpha);
    *dst_ptr = 0xFF000000u | blended;
}

void dial_draw_circle_fill(dial_fb_t *fb, float cx, float cy, float r, uint32_t rgb, uint8_t alpha) {
    if (!fb || r <= 0.0f || alpha == 0) return;
    int min_x = (int)floorf(cx - r - 1.5f);
    int min_y = (int)floorf(cy - r - 1.5f);
    int max_x = (int)ceilf(cx + r + 1.5f);
    int max_y = (int)ceilf(cy + r + 1.5f);
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= (int)fb->width)  max_x = (int)fb->width - 1;
    if (max_y >= (int)fb->height) max_y = (int)fb->height - 1;

    for (int y = min_y; y <= max_y; ++y) {
        const float dy = ((float)y + 0.5f) - cy;
        for (int x = min_x; x <= max_x; ++x) {
            const float dx = ((float)x + 0.5f) - cx;
            const float d  = sqrtf(dx * dx + dy * dy);
            float cov = (r + 0.5f) - d;
            if (cov > 0.0f) {
                if (cov > 1.0f) cov = 1.0f;
                dial_draw_pixel_aa(fb, x, y, rgb, (uint8_t)((float)alpha * cov + 0.5f));
            }
        }
    }
}

void dial_draw_circle_stroke(dial_fb_t *fb, float cx, float cy, float r, float stroke_w, uint32_t rgb, uint8_t alpha) {
    if (!fb || r <= 0.0f || stroke_w <= 0.0f || alpha == 0) return;
    const float half_w = stroke_w * 0.5f;
    int min_x = (int)floorf(cx - r - half_w - 1.5f);
    int min_y = (int)floorf(cy - r - half_w - 1.5f);
    int max_x = (int)ceilf(cx + r + half_w + 1.5f);
    int max_y = (int)ceilf(cy + r + half_w + 1.5f);
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= (int)fb->width)  max_x = (int)fb->width - 1;
    if (max_y >= (int)fb->height) max_y = (int)fb->height - 1;

    for (int y = min_y; y <= max_y; ++y) {
        const float dy = ((float)y + 0.5f) - cy;
        for (int x = min_x; x <= max_x; ++x) {
            const float dx = ((float)x + 0.5f) - cx;
            const float d  = fabsf(sqrtf(dx * dx + dy * dy) - r);
            float cov = (half_w + 0.5f) - d;
            if (cov > 0.0f) {
                if (cov > 1.0f) cov = 1.0f;
                dial_draw_pixel_aa(fb, x, y, rgb, (uint8_t)((float)alpha * cov + 0.5f));
            }
        }
    }
}

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
                      bool dashed) {
    if (!fb || r <= 0.0f || stroke_w <= 0.0f || alpha == 0) return;
    const float span = a1_deg - a0_deg;
    if (fabsf(span) < 0.05f) return;

    const float mid_deg   = (a0_deg + a1_deg) * 0.5f;
    const float half_span = fabsf(span) * 0.5f;
    const float half_w    = stroke_w * 0.5f;

    int min_x = (int)floorf(cx - r - half_w - 2.0f);
    int min_y = (int)floorf(cy - r - half_w - 2.0f);
    int max_x = (int)ceilf(cx + r + half_w + 2.0f);
    int max_y = (int)ceilf(cy + r + half_w + 2.0f);
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= (int)fb->width)  max_x = (int)fb->width - 1;
    if (max_y >= (int)fb->height) max_y = (int)fb->height - 1;

    for (int y = min_y; y <= max_y; ++y) {
        const float dy = ((float)y + 0.5f) - cy;
        for (int x = min_x; x <= max_x; ++x) {
            const float dx = ((float)x + 0.5f) - cx;
            const float pr = sqrtf(dx * dx + dy * dy);
            if (fabsf(pr - r) > half_w + 2.0f) continue;

            const float pdeg = atan2f(dx, -dy) * (180.0f / (float)M_PI);
            const float rel  = dial_angle_diff(mid_deg, pdeg);

            if (dashed) {
                float arc_pos = (rel + half_span) * ((float)M_PI / 180.0f) * r;
                if (fmodf(fabsf(arc_pos), 8.0f) > 2.2f) continue;
            }

            float d = 1e9f;
            if (fabsf(rel) <= half_span) {
                d = fabsf(pr - r);
            } else if (round_caps) {
                float cap_deg = mid_deg + ((rel > 0.0f) ? half_span : -half_span);
                float cap_rad = cap_deg * ((float)M_PI / 180.0f);
                float qx = cx + r * sinf(cap_rad);
                float qy = cy - r * cosf(cap_rad);
                float cdx = ((float)x + 0.5f) - qx;
                float cdy = ((float)y + 0.5f) - qy;
                d = sqrtf(cdx * cdx + cdy * cdy);
            }

            float cov = (half_w + 0.5f) - d;
            if (cov > 0.0f) {
                if (cov > 1.0f) cov = 1.0f;
                dial_draw_pixel_aa(fb, x, y, rgb, (uint8_t)((float)alpha * cov + 0.5f));
            }
        }
    }
}

void dial_draw_line_aa(dial_fb_t *fb,
                       float x0,
                       float y0,
                       float x1,
                       float y1,
                       float stroke_w,
                       uint32_t rgb,
                       uint8_t alpha) {
    if (!fb || stroke_w <= 0.0f || alpha == 0) return;
    const float half_w = stroke_w * 0.5f;
    int min_x = (int)floorf(fminf(x0, x1) - half_w - 2.0f);
    int min_y = (int)floorf(fminf(y0, y1) - half_w - 2.0f);
    int max_x = (int)ceilf(fmaxf(x0, x1) + half_w + 2.0f);
    int max_y = (int)ceilf(fmaxf(y0, y1) + half_w + 2.0f);
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= (int)fb->width)  max_x = (int)fb->width - 1;
    if (max_y >= (int)fb->height) max_y = (int)fb->height - 1;

    const float vx = x1 - x0;
    const float vy = y1 - y0;
    const float len2 = vx * vx + vy * vy;

    for (int y = min_y; y <= max_y; ++y) {
        const float py = (float)y + 0.5f;
        for (int x = min_x; x <= max_x; ++x) {
            const float px = (float)x + 0.5f;
            float t = 0.0f;
            if (len2 > 1e-6f) {
                t = ((px - x0) * vx + (py - y0) * vy) / len2;
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;
            }
            const float qx = x0 + t * vx;
            const float qy = y0 + t * vy;
            const float d  = sqrtf((px - qx) * (px - qx) + (py - qy) * (py - qy));
            float cov = (half_w + 0.5f) - d;
            if (cov > 0.0f) {
                if (cov > 1.0f) cov = 1.0f;
                dial_draw_pixel_aa(fb, x, y, rgb, (uint8_t)((float)alpha * cov + 0.5f));
            }
        }
    }
}

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
                       bool show_knob) {
    if (!fb || !pal) return;
    if (r <= 0.0f) r = (float)DIAL_RIM_ARC_R;
    if (stroke_w <= 0.0f) stroke_w = (float)DIAL_STROKE_RIM;
    if (accent_rgb == 0) accent_rgb = pal->moss;

    if (show_track) {
        dial_draw_arc_aa(fb, (float)DIAL_CENTER_X, (float)DIAL_CENTER_Y,
                         r, a0_deg, a1_deg, stroke_w, pal->surface_3, 255, true, dashed);
    }
    if (value_01 > 0.0f) {
        if (value_01 > 1.0f) value_01 = 1.0f;
        const float av = a0_deg + (a1_deg - a0_deg) * value_01;
        dial_draw_arc_aa(fb, (float)DIAL_CENTER_X, (float)DIAL_CENTER_Y,
                         r, a0_deg, av, stroke_w, accent_rgb, 255, true, false);
        if (show_knob) {
            float kx = 0.0f, ky = 0.0f;
            dial_polar_to_xy(av, r, &kx, &ky);
            dial_draw_circle_fill(fb, kx, ky, stroke_w + 2.5f, accent_rgb, 255);
            dial_draw_circle_stroke(fb, kx, ky, stroke_w + 2.5f, 2.5f, pal->bg, 255);
        }
    }
}

void dial_draw_scroll_rim(dial_fb_t *fb,
                          const dial_palette_t *pal,
                          float pos_01,
                          float len_01) {
    if (!fb || !pal) return;
    dial_draw_rim_arc(fb, pal, 52.0f, 128.0f, 0.0f, pal->moss, 170.0f, 3.0f, true, false, false);
    float a0 = 52.0f + 76.0f * pos_01;
    float a1 = 52.0f + 76.0f * (pos_01 + len_01);
    dial_draw_arc_aa(fb, (float)DIAL_CENTER_X, (float)DIAL_CENTER_Y,
                     170.0f, a0, a1, 3.0f, pal->moss, 255, true, false);
}

void dial_draw_vinyl_seek_ring(dial_fb_t *fb,
                               const dial_palette_t *pal,
                               float progress_01) {
    if (!fb || !pal) return;
    if (progress_01 < 0.0f) progress_01 = 0.0f;
    if (progress_01 > 0.999f) progress_01 = 0.999f;

    const float cx = (float)DIAL_CENTER_X;
    const float cy = (float)DIAL_CENTER_Y;
    const float r  = (float)DIAL_VINYL_SEEK_R; /* 38px */

    /* Center label fill + subtle track ring */
    dial_draw_circle_fill(fb, cx, cy, r, pal->bg, 115);
    dial_draw_circle_stroke(fb, cx, cy, r, 3.0f, pal->text_1, 56);

    /* Active progress arc in Moss clockwise from 12 o'clock (0 deg) */
    const float end_deg = 360.0f * progress_01;
    if (end_deg > 0.5f) {
        dial_draw_arc_aa(fb, cx, cy, r, 0.0f, end_deg, 3.5f, pal->moss, 255, true, false);
    }

    /* Thumb knob at current track angle */
    float kx = 0.0f, ky = 0.0f;
    dial_polar_to_xy(end_deg, r, &kx, &ky);
    dial_draw_circle_fill(fb, kx, ky, 4.5f, pal->moss, 255);
    dial_draw_circle_stroke(fb, kx, ky, 4.5f, 1.8f, pal->bg, 255);

    /* Center spindle hole (r = 6px) */
    dial_draw_circle_fill(fb, cx, cy, 6.0f, pal->bg, 255);
    dial_draw_circle_stroke(fb, cx, cy, 6.0f, 1.5f, pal->text_1, 90);
}

static float sd_round_rect(float px, float py, float x, float y, float w, float h, float r) {
    float cx = x + w * 0.5f;
    float cy = y + h * 0.5f;
    float hx = w * 0.5f - r;
    float hy = h * 0.5f - r;
    float dx = fabsf(px - cx) - hx;
    float dy = fabsf(py - cy) - hy;
    float ox = fmaxf(dx, 0.0f);
    float oy = fmaxf(dy, 0.0f);
    return sqrtf(ox * ox + oy * oy) + fminf(fmaxf(dx, dy), 0.0f) - r;
}

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
                          uint8_t border_alpha) {
    if (!fb || w <= 0.0f || h <= 0.0f) return;
    float max_r = fminf(w, h) * 0.5f;
    if (radius > max_r) radius = max_r;

    int min_x = (int)floorf(x - 2.0f);
    int min_y = (int)floorf(y - 2.0f);
    int max_x = (int)ceilf(x + w + 2.0f);
    int max_y = (int)ceilf(y + h + 2.0f);
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= (int)fb->width)  max_x = (int)fb->width - 1;
    if (max_y >= (int)fb->height) max_y = (int)fb->height - 1;

    for (int py = min_y; py <= max_y; ++py) {
        const float cy = (float)py + 0.5f;
        for (int px = min_x; px <= max_x; ++px) {
            const float cx = (float)px + 0.5f;
            const float sd = sd_round_rect(cx, cy, x, y, w, h, radius);

            if (fill_alpha > 0) {
                float fill_cov = 0.5f - sd;
                if (fill_cov > 0.0f) {
                    if (fill_cov > 1.0f) fill_cov = 1.0f;
                    dial_draw_pixel_aa(fb, px, py, fill_rgb,
                                       (uint8_t)((float)fill_alpha * fill_cov + 0.5f));
                }
            }
            if (border_w > 0.0f && border_alpha > 0) {
                /* Inset border ring [-border_w, 0] */
                float b_cov = fminf(0.5f - sd, sd + border_w + 0.5f);
                if (b_cov > 0.0f) {
                    if (b_cov > 1.0f) b_cov = 1.0f;
                    dial_draw_pixel_aa(fb, px, py, border_rgb,
                                       (uint8_t)((float)border_alpha * b_cov + 0.5f));
                }
            }
        }
    }
}

/* ── CoreText 8-Bit (256-Level Alpha) Proportional Font Renderer (ASCII + UTF-8 Nordic) ── */
static const dial_baked_font_t *select_baked_font(int font_px) {
    if (font_px <= 10) return &g_dial_fonts[DIAL_FONT_FACE_SANS_10];
    if (font_px == 11) return &g_dial_fonts[DIAL_FONT_FACE_SANS_11];
    if (font_px == 12) return &g_dial_fonts[DIAL_FONT_FACE_SANS_12];
    if (font_px == 13) return &g_dial_fonts[DIAL_FONT_FACE_SANS_13];
    if (font_px <= 15) return &g_dial_fonts[DIAL_FONT_FACE_SANS_15];
    if (font_px <= 17) return &g_dial_fonts[DIAL_FONT_FACE_SANS_16];
    if (font_px <= 19) return &g_dial_fonts[DIAL_FONT_FACE_SANS_18];
    if (font_px <= 21) return &g_dial_fonts[DIAL_FONT_FACE_SANS_20];
    if (font_px <= 25) return &g_dial_fonts[DIAL_FONT_FACE_SANS_22];
    if (font_px <= 36) return &g_dial_fonts[DIAL_FONT_FACE_SANS_28];
    if (font_px <= 47) return &g_dial_fonts[DIAL_FONT_FACE_SANS_44];
    return &g_dial_fonts[DIAL_FONT_FACE_SANS_52];
}

static uint32_t next_utf8_codepoint(const unsigned char **pp) {
    const unsigned char *p = *pp;
    if (!p || !*p) return 0;
    unsigned char c = *p;
    if (c < 0x80u) {
        *pp = p + 1;
        return (uint32_t)c;
    } else if ((c & 0xE0u) == 0xC0u && p[1]) {
        uint32_t cp = (((uint32_t)(c & 0x1Fu)) << 6) | ((uint32_t)(p[1] & 0x3Fu));
        *pp = p + 2;
        return cp;
    } else if ((c & 0xF0u) == 0xE0u && p[1] && p[2]) {
        uint32_t cp = (((uint32_t)(c & 0x0Fu)) << 12) |
                      (((uint32_t)(p[1] & 0x3Fu)) << 6) |
                      ((uint32_t)(p[2] & 0x3Fu));
        *pp = p + 3;
        if (cp == 0x2212u) cp = 0x2013u; /* Unicode minus -> en-dash */
        return cp;
    }
    *pp = p + 1;
    return (uint32_t)'.';
}

static const dial_baked_glyph_t *lookup_glyph(const dial_baked_font_t *font, uint32_t cp) {
    if (cp >= 32u && cp <= 126u) {
        return &font->glyphs[cp - 32u];
    }
    for (size_t i = 95; i < DIAL_ASSET_CHAR_COUNT; ++i) {
        if (font->glyphs[i].codepoint == cp) {
            return &font->glyphs[i];
        }
    }
    return &font->glyphs['.' - 32];
}

int dial_measure_text(const char *text, int font_px, int letter_spacing_px) {
    if (!text || !*text || font_px <= 0) return 0;
    const dial_baked_font_t *font = select_baked_font(font_px);
    int extra_track = letter_spacing_px + (font_px == 11 ? 1 : 0);
    int total_w = 0;
    int count = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        uint32_t cp = next_utf8_codepoint(&p);
        if (!cp) break;
        const dial_baked_glyph_t *g = lookup_glyph(font, cp);
        total_w += (int)g->advance_x + extra_track;
        count++;
    }
    if (count > 0) total_w -= extra_track;
    return total_w;
}

static void draw_text_clipped(dial_fb_t *fb,
                              const char *text,
                              int x,
                              int y,
                              int font_px,
                              uint32_t rgb,
                              uint8_t alpha,
                              int clip_min_x,
                              int clip_max_x) {
    if (!fb || !text || !*text || font_px <= 0 || alpha == 0) return;
    const dial_baked_font_t *font = select_baked_font(font_px);
    int extra_track = (font_px == 11 ? 1 : 0);
    int baseline_y = y + (int)roundf(font->size_px * 0.80f);
    int pen_x = x;

    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        uint32_t cp = next_utf8_codepoint(&p);
        if (!cp) break;
        const dial_baked_glyph_t *g = lookup_glyph(font, cp);

        if (g->width > 0 && g->height > 0) {
            int gx0 = pen_x + (int)g->bearing_x;
            int gy0 = baseline_y - (int)g->bearing_y;
            const uint8_t *bmp = &g_dial_font_atlas[g->bitmap_offset];

            for (int ry = 0; ry < (int)g->height; ++ry) {
                int py = gy0 + ry;
                if (py < 0 || py >= (int)fb->height) continue;
                const uint8_t *row = &bmp[ry * (int)g->width];
                for (int rx = 0; rx < (int)g->width; ++rx) {
                    int px = gx0 + rx;
                    if (px < clip_min_x || px > clip_max_x) continue;
                    uint8_t cov = row[rx];
                    if (cov > 0) {
                        uint8_t a = (uint8_t)(((uint32_t)alpha * (uint32_t)cov + 127u) / 255u);
                        dial_draw_pixel_aa(fb, px, py, rgb, a);
                    }
                }
            }
        }
        pen_x += (int)g->advance_x + extra_track;
    }
}

void dial_draw_text(dial_fb_t *fb,
                    const char *text,
                    int x,
                    int y,
                    int font_px,
                    uint32_t rgb,
                    uint8_t alpha,
                    bool centered) {
    if (!fb || !text) return;
    int draw_x = x;
    if (centered) {
        int w = dial_measure_text(text, font_px, 0);
        draw_x = x - w / 2;
    }
    draw_text_clipped(fb, text, draw_x, y, font_px, rgb, alpha, 0, (int)fb->width - 1);
}

void dial_draw_text_marquee(dial_fb_t *fb,
                            const char *text,
                            int clip_x,
                            int clip_y,
                            int clip_w,
                            int font_px,
                            uint32_t rgb,
                            uint8_t alpha,
                            uint32_t tick_ms) {
    if (!fb || !text || clip_w <= 0) return;
    int text_w = dial_measure_text(text, font_px, 0);
    if (text_w <= clip_w) {
        draw_text_clipped(fb, text, clip_x, clip_y, font_px, rgb, alpha,
                          clip_x, clip_x + clip_w);
        return;
    }

    /* Smooth horizontal train-slide marquee (40 px/s with 600ms end pause) */
    int overflow = text_w - clip_w;
    uint32_t cycle_ms = (uint32_t)(overflow * 25 + 1200);
    uint32_t phase_ms = tick_ms % cycle_ms;
    int offset_px = 0;
    if (phase_ms > 400) {
        offset_px = (int)((phase_ms - 400) / 25);
        if (offset_px > overflow) offset_px = overflow;
    }
    draw_text_clipped(fb, text, clip_x - offset_px, clip_y, font_px, rgb, alpha,
                      clip_x, clip_x + clip_w);
}

/* ── High-Level Fjäll UI Components ── */

void dial_draw_pill(dial_fb_t *fb,
                    const dial_palette_t *pal,
                    int cx,
                    int y,
                    const char *icon_name,
                    const char *label,
                    bool active) {
    if (!fb || !pal || !label) return;
    int text_w = dial_measure_text(label, 12, 0);
    int icon_w = icon_name ? 18 : 0;
    int w = text_w + icon_w + 24;
    int h = 30;
    int x = cx - w / 2;

    uint32_t bg = active ? dial_color_lerp(pal->surface_2, pal->moss, 46) : pal->surface_1;
    uint32_t border = active ? pal->moss : pal->line;
    uint32_t fg = active ? pal->moss : pal->text_1;

    dial_draw_round_rect(fb, (float)x, (float)y, (float)w, (float)h, 15.0f,
                         bg, 255, active ? 1.5f : 1.0f, border, 255);
    int tx = x + 12;
    if (icon_name) {
        dial_draw_icon_by_name(fb, icon_name, (float)tx, (float)(y + 8), 14.0f, 1.75f, fg, 255);
        tx += 18;
    }
    dial_draw_text(fb, label, tx, y + 9, 12, fg, 255, false);
}

void dial_draw_chip(dial_fb_t *fb,
                    const dial_palette_t *pal,
                    int x,
                    int y,
                    const char *label,
                    uint32_t accent_rgb,
                    int *out_width) {
    if (!fb || !pal || !label) return;
    int text_w = dial_measure_text(label, 10, 0);
    int w = text_w + 14;
    int h = 20;
    if (out_width) *out_width = w;

    uint32_t bg = (accent_rgb != 0) ? dial_color_lerp(pal->surface_1, accent_rgb, 42) : pal->surface_2;
    uint32_t fg = (accent_rgb != 0) ? accent_rgb : pal->text_2;
    dial_draw_round_rect(fb, (float)x, (float)y, (float)w, (float)h, 10.0f,
                         bg, 255, 1.0f, (accent_rgb != 0) ? accent_rgb : pal->line, 120);
    dial_draw_text(fb, label, x + 7, y + 5, 10, fg, 255, false);
}

void dial_draw_toggle(dial_fb_t *fb,
                      const dial_palette_t *pal,
                      int x,
                      int y,
                      bool on) {
    if (!fb || !pal) return;
    uint32_t track_rgb = on ? pal->moss : pal->surface_3;
    uint32_t thumb_rgb = on ? pal->on_accent : pal->text_2;
    dial_draw_round_rect(fb, (float)x, (float)y, 36.0f, 20.0f, 10.0f,
                         track_rgb, 255, 0.0f, 0, 0);
    float kx = on ? (float)(x + 26) : (float)(x + 10);
    dial_draw_circle_fill(fb, kx, (float)(y + 10), 7.0f, thumb_rgb, 255);
}

void dial_draw_scrim(dial_fb_t *fb, const dial_palette_t *pal, uint8_t alpha) {
    if (!fb || !pal) return;
    if (alpha == 0) alpha = DIAL_SCRIM_ALPHA; /* 184 = 72% */
    dial_draw_circle_fill(fb, (float)DIAL_CENTER_X, (float)DIAL_CENTER_Y,
                          (float)DIAL_RADIUS, pal->bg, alpha);
}

void dial_draw_top_grab(dial_fb_t *fb, const dial_palette_t *pal) {
    if (!fb || !pal) return;
    dial_draw_round_rect(fb, 166.0f, 10.0f, 28.0f, 4.0f, 2.0f,
                         pal->text_1, 90, 0.0f, 0, 0);
}

void dial_draw_status_bar(dial_fb_t *fb,
                          const dial_palette_t *pal,
                          const char *time_str,
                          int battery_pct,
                          uint32_t override_rgb) {
    if (!fb || !pal) return;
    uint32_t fg = (override_rgb != 0) ? override_rgb : pal->text_2;
    const char *ts = time_str ? time_str : "21:45";
    dial_draw_text(fb, ts, 134, 24, 11, fg, 230, false);
    dial_draw_circle_fill(fb, 176.0f, 29.0f, 1.5f, fg, 140);
    dial_draw_icon(fb, DIAL_ICON_BATTERY, 182.0f, 21.0f, 15.0f, 1.75f, fg, 230);
    char bat_buf[16];
    snprintf(bat_buf, sizeof(bat_buf), "%d%%", battery_pct);
    dial_draw_text(fb, bat_buf, 200, 24, 11, fg, 230, false);
}

void dial_draw_header(dial_fb_t *fb,
                      const dial_palette_t *pal,
                      const char *overline,
                      const char *title,
                      int y) {
    if (!fb || !pal) return;
    int cur_y = y;
    if (overline && overline[0]) {
        char upper[64];
        size_t i = 0;
        for (; overline[i] && i < sizeof(upper) - 1; ++i) {
            upper[i] = (char)toupper((unsigned char)overline[i]);
        }
        upper[i] = '\0';
        dial_draw_text(fb, upper, DIAL_CENTER_X, cur_y, 11, pal->text_3, 255, true);
        cur_y += 16;
    }
    if (title && title[0]) {
        dial_draw_text(fb, title, DIAL_CENTER_X, cur_y, 17, pal->text_1, 255, true);
    }
}

void dial_draw_hint(dial_fb_t *fb,
                    const dial_palette_t *pal,
                    const char *icon_name,
                    const char *text,
                    uint32_t override_rgb) {
    if (!fb || !pal || !text) return;
    uint32_t fg = (override_rgb != 0) ? override_rgb : pal->text_3;
    int tw = dial_measure_text(text, 10, 0);
    int iw = icon_name ? 16 : 0;
    int start_x = DIAL_CENTER_X - (tw + iw) / 2;
    if (icon_name) {
        dial_draw_icon_by_name(fb, icon_name, (float)start_x, 317.0f, 12.0f, 2.0f, fg, 210);
        start_x += 16;
    }
    dial_draw_text(fb, text, start_x, 318, 10, fg, 210, false);
}

void dial_draw_horizon(dial_fb_t *fb, const dial_palette_t *pal) {
    if (!fb || !pal) return;
    /* Stylized Nordic mountain ridge silhouette with subpixel AA edges */
    const int peaks1_x[8] = {0, 54, 92, 140, 196, 236, 300, 360};
    const int peaks1_y[8] = {300, 264, 288, 248, 292, 270, 306, 280};
    const int peaks2_x[6] = {0, 70, 130, 210, 290, 360};
    const int peaks2_y[6] = {326, 300, 318, 294, 322, 308};

    for (int x = 0; x < DIAL_SCREEN_WIDTH; ++x) {
        float y1 = 360.0f;
        for (int s = 0; s < 7; ++s) {
            if (x >= peaks1_x[s] && x <= peaks1_x[s + 1]) {
                float t = (float)(x - peaks1_x[s]) / (float)(peaks1_x[s + 1] - peaks1_x[s]);
                y1 = (float)peaks1_y[s] + t * (float)(peaks1_y[s + 1] - peaks1_y[s]);
                break;
            }
        }
        float y2 = 360.0f;
        for (int s = 0; s < 5; ++s) {
            if (x >= peaks2_x[s] && x <= peaks2_x[s + 1]) {
                float t = (float)(x - peaks2_x[s]) / (float)(peaks2_x[s + 1] - peaks2_x[s]);
                y2 = (float)peaks2_y[s] + t * (float)(peaks2_y[s + 1] - peaks2_y[s]);
                break;
            }
        }
        int start_y = (int)floorf(y1) - 1;
        if (start_y < 0) start_y = 0;
        for (int y = start_y; y < DIAL_SCREEN_HEIGHT; ++y) {
            float fy = (float)y + 0.5f;
            float cov1 = fy - y1 + 0.5f;
            if (cov1 > 0.0f) {
                if (cov1 > 1.0f) cov1 = 1.0f;
                dial_draw_pixel_aa(fb, x, y, pal->surface_1, (uint8_t)(cov1 * 255.0f + 0.5f));
            }
            float cov2 = fy - y2 + 0.5f;
            if (cov2 > 0.0f) {
                if (cov2 > 1.0f) cov2 = 1.0f;
                dial_draw_pixel_aa(fb, x, y, pal->surface_2, (uint8_t)(cov2 * 255.0f + 0.5f));
            }
        }
    }
}

void dial_draw_rim_list(dial_fb_t *fb,
                        const dial_palette_t *pal,
                        const dial_rim_row_t *rows,
                        size_t row_count,
                        size_t focus_index,
                        int top_y,
                        uint32_t tick_ms) {
    if (!fb || !pal || !rows || row_count == 0) return;

    int cur_y = top_y;
    for (size_t i = 0; i < row_count; ++i) {
        const dial_rim_row_t *r = &rows[i];
        size_t d = (i > focus_index) ? (i - focus_index) : (focus_index - i);
        bool focused = (d == 0);

        float rw = (d == 0) ? 284.0f : (d == 1 ? 258.0f : 226.0f);
        float rh = (d == 0) ? 52.0f  : 44.0f;
        float rad = (d == 0) ? 18.0f : 15.0f;
        uint8_t op = (d == 0) ? 255 : (d == 1 ? 235 : 115);

        float rx = (float)DIAL_CENTER_X - rw * 0.5f;
        float ry = (float)cur_y;

        uint32_t fill = focused ? pal->surface_2 : pal->surface_1;
        uint32_t brd  = focused ? pal->moss      : pal->line;
        float    bw   = focused ? 1.5f           : 1.0f;

        dial_draw_round_rect(fb, rx, ry, rw, rh, rad, fill, op, bw, brd, op);

        int text_x = (int)rx + 14;
        if (r->icon_name) {
            uint32_t ic_col = (r->icon_tint != 0) ? r->icon_tint : (focused ? pal->moss : pal->text_2);
            dial_draw_circle_fill(fb, rx + 26.0f, ry + rh * 0.5f, 15.0f,
                                  dial_color_lerp(fill, ic_col, 38), op);
            dial_draw_icon_by_name(fb, r->icon_name, rx + 17.5f, ry + rh * 0.5f - 8.5f,
                                   17.0f, 1.75f, ic_col, op);
            text_x = (int)rx + 48;
        } else if (r->lead_text) {
            uint32_t lc = (r->lead_color != 0) ? r->lead_color : pal->text_3;
            dial_draw_text(fb, r->lead_text, (int)rx + 16, (int)(ry + rh * 0.5f - 6.0f),
                           12, lc, op, false);
            text_x = (int)rx + 44;
        }

        bool show_sub = (r->sub && r->sub[0] && (focused || r->always_sub)) || r->playing_eq;
        int title_y = show_sub ? (int)(ry + 11.0f) : (int)(ry + rh * 0.5f - 7.0f);
        int max_title_w = (int)(rx + rw) - text_x - 56;

        if (r->title) {
            if (r->marquee) {
                dial_draw_text_marquee(fb, r->title, text_x, title_y, max_title_w,
                                       14, pal->text_1, op, tick_ms);
            } else {
                dial_draw_text(fb, r->title, text_x, title_y, 14, pal->text_1, op, false);
            }
        }

        if (r->playing_eq) {
            /* Animated 4-bar Moss equalizer + "Playing" */
            const int eq_h[4] = {6, 11, 8, 10};
            for (int b = 0; b < 4; ++b) {
                int bh = eq_h[b];
                dial_draw_round_rect(fb, (float)(text_x + b * 4), (float)(title_y + 26 - bh),
                                     2.5f, (float)bh, 1.0f, pal->moss, op, 0.0f, 0, 0);
            }
            dial_draw_text(fb, "Playing", text_x + 22, title_y + 16, 11, pal->moss, op, false);
        } else if (show_sub && r->sub) {
            dial_draw_text(fb, r->sub, text_x, title_y + 16, 11, pal->text_2, op, false);
        }

        /* Right-hand accessory (chevron, check, toggle, chip, or value text) */
        int right_x = (int)(rx + rw) - 14;
        if (r->has_toggle) {
            dial_draw_toggle(fb, pal, right_x - 36, (int)(ry + rh * 0.5f - 10.0f), r->toggle_on);
        } else if (r->has_check) {
            dial_draw_icon(fb, DIAL_ICON_CHECK, (float)(right_x - 18), ry + rh * 0.5f - 9.0f,
                           18.0f, 2.2f, pal->moss, op);
        } else if (r->has_chev) {
            dial_draw_icon(fb, DIAL_ICON_CHEV, (float)(right_x - 16), ry + rh * 0.5f - 8.0f,
                           16.0f, 1.75f, pal->text_3, op);
        } else if (r->value_text && r->value_text[0]) {
            if (r->value_is_chip) {
                int cw = dial_measure_text(r->value_text, 10, 0) + 14;
                dial_draw_chip(fb, pal, right_x - cw, (int)(ry + rh * 0.5f - 10.0f),
                               r->value_text, r->chip_color, NULL);
            } else {
                int vw = dial_measure_text(r->value_text, 12, 0);
                dial_draw_text(fb, r->value_text, right_x - vw, (int)(ry + rh * 0.5f - 6.0f),
                               12, pal->text_2, op, false);
            }
        }

        cur_y += (int)rh + DIAL_SPACE_2;
    }
}

static uint32_t unpack_rgb565_to_888(uint16_t p) {
    uint32_t r = ((p >> 11) & 0x1Fu) * 255u / 31u;
    uint32_t g = ((p >> 5)  & 0x3Fu) * 255u / 63u;
    uint32_t b = ( p        & 0x1Fu) * 255u / 31u;
    return (r << 16) | (g << 8) | b;
}

static uint32_t sample_cover_bilinear(const uint16_t *tex, float u01, float v01) {
    if (u01 < 0.0f) u01 = 0.0f;
    if (u01 > 1.0f) u01 = 1.0f;
    if (v01 < 0.0f) v01 = 0.0f;
    if (v01 > 1.0f) v01 = 1.0f;

    float fx = u01 * (float)(DIAL_VINYL_TEX_DIM - 1);
    float fy = v01 * (float)(DIAL_VINYL_TEX_DIM - 1);
    int x0 = (int)fx;
    int y0 = (int)fy;
    int x1 = (x0 + 1 < DIAL_VINYL_TEX_DIM) ? (x0 + 1) : x0;
    int y1 = (y0 + 1 < DIAL_VINYL_TEX_DIM) ? (y0 + 1) : y0;
    uint8_t tx = (uint8_t)((fx - (float)x0) * 255.0f);
    uint8_t ty = (uint8_t)((fy - (float)y0) * 255.0f);

    uint32_t c00 = unpack_rgb565_to_888(tex[y0 * DIAL_VINYL_TEX_DIM + x0]);
    uint32_t c10 = unpack_rgb565_to_888(tex[y0 * DIAL_VINYL_TEX_DIM + x1]);
    uint32_t c01 = unpack_rgb565_to_888(tex[y1 * DIAL_VINYL_TEX_DIM + x0]);
    uint32_t c11 = unpack_rgb565_to_888(tex[y1 * DIAL_VINYL_TEX_DIM + x1]);

    uint32_t top = dial_color_lerp(c00, c10, tx);
    uint32_t bot = dial_color_lerp(c01, c11, tx);
    return dial_color_lerp(top, bot, ty);
}

void dial_draw_art(dial_fb_t *fb,
                   const dial_palette_t *pal,
                   dial_art_kind_t kind,
                   int cx,
                   int cy,
                   int diameter,
                   bool vinyl_grooves,
                   float spin_deg) {
    if (!fb || !pal || diameter <= 0) return;
    const float rad = (float)diameter * 0.5f;
    const float spin_rad = spin_deg * ((float)M_PI / 180.0f);
    const float cos_s = cosf(spin_rad);
    const float sin_s = sinf(spin_rad);
    const uint16_t *tex = g_dial_vinyl_covers[(int)kind % DIAL_VINYL_TEX_COUNT];

    int min_x = (int)floorf((float)cx - rad - 1.0f);
    int min_y = (int)floorf((float)cy - rad - 1.0f);
    int max_x = (int)ceilf((float)cx + rad + 1.0f);
    int max_y = (int)ceilf((float)cy + rad + 1.0f);
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= (int)fb->width)  max_x = (int)fb->width - 1;
    if (max_y >= (int)fb->height) max_y = (int)fb->height - 1;

    for (int py = min_y; py <= max_y; ++py) {
        const float dy = ((float)py + 0.5f) - (float)cy;
        for (int px = min_x; px <= max_x; ++px) {
            const float dx = ((float)px + 0.5f) - (float)cx;
            const float r  = sqrtf(dx * dx + dy * dy);
            if (r > rad + 0.5f) continue;

            const float rx = (dx * cos_s + dy * sin_s) / rad;
            const float ry = (-dx * sin_s + dy * cos_s) / rad;
            const float u01 = (rx + 1.0f) * 0.5f;
            const float v01 = (ry + 1.0f) * 0.5f;

            uint32_t col = sample_cover_bilinear(tex, u01, v01);

            if (vinyl_grooves) {
                /* Smooth anti-aliased vinyl groove rings + radial edge vignette (Locked Rule #1) */
                float norm_r = (r / rad) * 50.0f;
                const float gr[6] = {47.0f, 43.0f, 39.5f, 36.0f, 33.0f, 30.0f};
                for (int g = 0; g < 6; ++g) {
                    float gd = fabsf(norm_r - gr[g]);
                    if (gd < 0.65f) {
                        uint8_t ga = (uint8_t)((1.0f - gd / 0.65f) * 60.0f);
                        col = dial_color_lerp(col, 0x000000u, ga);
                    }
                }
                if (norm_r < 11.0f) {
                    uint8_t center_dim = (uint8_t)((1.0f - norm_r / 11.0f) * 85.0f);
                    col = dial_color_lerp(col, pal->bg, center_dim);
                } else if (norm_r > 36.0f) {
                    uint8_t vig = (uint8_t)(((norm_r - 36.0f) / 14.0f) * 194.0f);
                    col = dial_color_lerp(col, pal->bg, vig);
                }
            }

            float cov = (rad + 0.5f) - r;
            if (cov > 1.0f) cov = 1.0f;
            dial_draw_pixel_aa(fb, px, py, col, (uint8_t)(255.0f * cov + 0.5f));
        }
    }
}

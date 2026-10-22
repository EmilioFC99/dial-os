/**
 * tools/bake_fjall_assets.m
 * Build-time asset compiler for DialOS (Fjäll Design System).
 *
 * Uses macOS CoreText and CoreGraphics to pre-bake:
 *  1. 8-bit (256-level grayscale alpha) proportional anti-aliased glyph atlases
 *     for all Fjäll typography sizes (10px..52px Sans + 11px/13px Mono),
 *     including UTF-8 Nordic characters (ä, ö, å, Ä, Ö, Å, ø, ·, —, °, “, ”).
 *  2. The 6 exact Fjäll Design System album covers (fjord, rings, aurora,
 *     moss, berry, snow) rasterized with CoreGraphics vector anti-aliasing
 *     at 240x240 RGB565 for bilinear-filtered 360x360 spinning vinyl.
 *
 * Outputs platform-agnostic C99 files:
 *  - include/dial_assets.h
 *  - src/ui/dial_assets.c
 */

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

static const uint32_t kCharset[] = {
    /* Printable ASCII 32..126 */
    32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
    48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
    64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
    80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
    96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
    112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,
    /* Extended Nordic & UI symbols */
    0x00B0, /* ° degree */
    0x00B7, /* · middle dot */
    0x00C4, /* Ä */
    0x00C5, /* Å */
    0x00D6, /* Ö */
    0x00D8, /* Ø */
    0x00E4, /* ä */
    0x00E5, /* å */
    0x00F6, /* ö */
    0x00F8, /* ø */
    0x2013, /* – en-dash */
    0x2014, /* — em-dash */
    0x201C, /* “ */
    0x201D, /* ” */
    0x2022, /* • bullet */
    0x2026  /* … ellipsis */
};
#define NUM_CHARS (sizeof(kCharset) / sizeof(kCharset[0]))

typedef struct {
    const char *enum_name;
    const char *ps_name;
    const char *fallback_ps;
    float       size_px;
    int         is_mono;
} font_spec_t;

static const font_spec_t kFontSpecs[] = {
    { "DIAL_FONT_FACE_SANS_10", "AvenirNext-DemiBold", "HelveticaNeue-Medium",   10.5f, 0 },
    { "DIAL_FONT_FACE_SANS_11", "AvenirNext-DemiBold", "HelveticaNeue-Medium",   11.5f, 0 },
    { "DIAL_FONT_FACE_SANS_12", "AvenirNext-Medium",   "HelveticaNeue-Medium",   12.5f, 0 },
    { "DIAL_FONT_FACE_SANS_13", "AvenirNext-Medium",   "HelveticaNeue",          13.5f, 0 },
    { "DIAL_FONT_FACE_SANS_15", "AvenirNext-DemiBold", "HelveticaNeue-Medium",   15.5f, 0 },
    { "DIAL_FONT_FACE_SANS_16", "AvenirNext-DemiBold", "HelveticaNeue-Bold",     16.5f, 0 },
    { "DIAL_FONT_FACE_SANS_18", "AvenirNext-DemiBold", "HelveticaNeue-Bold",     18.5f, 0 },
    { "DIAL_FONT_FACE_SANS_20", "AvenirNext-DemiBold", "HelveticaNeue-Bold",     20.5f, 0 },
    { "DIAL_FONT_FACE_SANS_22", "AvenirNext-DemiBold", "HelveticaNeue-Bold",     22.5f, 0 },
    { "DIAL_FONT_FACE_SANS_28", "AvenirNext-Medium",   "HelveticaNeue-Light",    28.0f, 0 },
    { "DIAL_FONT_FACE_SANS_44", "AvenirNext-Medium",   "HelveticaNeue-Light",    44.0f, 0 },
    { "DIAL_FONT_FACE_SANS_52", "AvenirNext-Medium",   "HelveticaNeue-Light",    52.0f, 0 },
    { "DIAL_FONT_FACE_MONO_11", "Menlo-Regular",       "CourierNewPS-BoldMT",    11.0f, 1 },
    { "DIAL_FONT_FACE_MONO_13", "Menlo-Regular",       "CourierNewPS-BoldMT",    13.0f, 1 }
};
#define NUM_FONTS (sizeof(kFontSpecs) / sizeof(kFontSpecs[0]))

typedef struct {
    uint32_t codepoint;
    uint16_t width;
    uint16_t height;
    int16_t  bearing_x;
    int16_t  bearing_y;
    int16_t  advance_x;
    uint32_t bitmap_offset;
} baked_glyph_t;

static CTFontRef load_font(const char *ps_name, const char *fallback_ps, float size_px) {
    NSString *name = [NSString stringWithUTF8String:ps_name];
    CTFontRef f = CTFontCreateWithName((__bridge CFStringRef)name, size_px, NULL);
    if (f) return f;
    NSString *fb = [NSString stringWithUTF8String:fallback_ps];
    return CTFontCreateWithName((__bridge CFStringRef)fb, size_px, NULL);
}

static void set_rgb_fill(CGContextRef ctx, uint32_t hex, float alpha) {
    CGContextSetRGBFillColor(ctx,
                             ((hex >> 16) & 0xFF) / 255.0f,
                             ((hex >> 8)  & 0xFF) / 255.0f,
                             ( hex        & 0xFF) / 255.0f,
                             alpha);
}

static void set_rgb_stroke(CGContextRef ctx, uint32_t hex, float alpha) {
    CGContextSetRGBStrokeColor(ctx,
                               ((hex >> 16) & 0xFF) / 255.0f,
                               ((hex >> 8)  & 0xFF) / 255.0f,
                               ( hex        & 0xFF) / 255.0f,
                               alpha);
}

static void fill_poly(CGContextRef ctx, const float *pts, int count, uint32_t hex, float alpha) {
    set_rgb_fill(ctx, hex, alpha);
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx, pts[0], pts[1]);
    for (int i = 1; i < count; ++i) {
        CGContextAddLineToPoint(ctx, pts[i * 2], pts[i * 2 + 1]);
    }
    CGContextClosePath(ctx);
    CGContextFillPath(ctx);
}

static void fill_circle(CGContextRef ctx, float cx, float cy, float r, uint32_t hex, float alpha) {
    set_rgb_fill(ctx, hex, alpha);
    CGContextFillEllipseInRect(ctx, CGRectMake(cx - r, cy - r, r * 2.0f, r * 2.0f));
}

/* Render the 6 exact Fjäll Design System covers (from fjall-design-system.html lines 741-751)
 * in a [0..100] x [0..100] top-down coordinate space scaled to dim x dim */
static void bake_fjall_cover(int kind, uint16_t *out_rgb565, int dim) {
    uint8_t *rgba = (uint8_t *)calloc((size_t)(dim * dim * 4), 1);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(rgba, (size_t)dim, (size_t)dim, 8, (size_t)(dim * 4),
                                             cs, (CGBitmapInfo)kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGContextSetAllowsAntialiasing(ctx, true);
    CGContextSetShouldAntialias(ctx, true);

    /* Flip CG coordinates so (0,0) is top-left and (100,100) is bottom-right like SVG */
    CGContextTranslateCTM(ctx, 0.0f, (CGFloat)dim);
    CGContextScaleCTM(ctx, (CGFloat)dim / 100.0f, -(CGFloat)dim / 100.0f);
    CGContextSetLineCap(ctx, kCGLineCapRound);

    switch (kind) {
        case 0: { /* fjord */
            set_rgb_fill(ctx, 0x20313Au, 1.0f);
            CGContextFillRect(ctx, CGRectMake(0, 0, 100, 100));
            fill_circle(ctx, 68.0f, 34.0f, 11.0f, 0xE2A66Au, 0.90f);
            const float m1[] = {0,58, 18,40, 30,50, 46,30, 64,52, 78,42, 100,58, 100,100, 0,100};
            fill_poly(ctx, m1, 9, 0x3E5A68u, 1.0f);
            const float m2[] = {0,68, 22,54, 38,64, 58,48, 76,62, 100,54, 100,100, 0,100};
            fill_poly(ctx, m2, 8, 0x2C4450u, 1.0f);
            set_rgb_fill(ctx, 0x18262Du, 1.0f);
            CGContextFillRect(ctx, CGRectMake(0, 74, 100, 26));
            set_rgb_stroke(ctx, 0x8FB3C9u, 0.50f);
            CGContextSetLineWidth(ctx, 1.2f);
            CGContextBeginPath(ctx);
            CGContextMoveToPoint(ctx, 8, 82);  CGContextAddLineToPoint(ctx, 38, 82);
            CGContextMoveToPoint(ctx, 52, 88); CGContextAddLineToPoint(ctx, 88, 88);
            CGContextMoveToPoint(ctx, 20, 94); CGContextAddLineToPoint(ctx, 42, 94);
            CGContextStrokePath(ctx);
            break;
        }
        case 1: { /* rings */
            set_rgb_fill(ctx, 0xD8CFBDu, 1.0f);
            CGContextFillRect(ctx, CGRectMake(0, 0, 100, 100));
            const float rs[8] = {46,38,31,25,19,13,8,4};
            for (int i = 0; i < 8; ++i) {
                float cx = 52.0f - (float)i * 0.6f;
                float cy = 50.0f + (float)i * 0.4f;
                float rx = rs[i] + 2.0f;
                float ry = rs[i];
                set_rgb_stroke(ctx, 0x7A6A4Eu, 0.35f + (float)i * 0.07f);
                CGContextSetLineWidth(ctx, (i % 2) ? 1.4f : 2.4f);
                CGContextStrokeEllipseInRect(ctx, CGRectMake(cx - rx, cy - ry, rx * 2.0f, ry * 2.0f));
            }
            set_rgb_stroke(ctx, 0x5C4F38u, 0.50f);
            CGContextSetLineWidth(ctx, 1.2f);
            CGContextBeginPath(ctx);
            CGContextMoveToPoint(ctx, 52, 50);
            CGContextAddLineToPoint(ctx, 92, 28);
            CGContextStrokePath(ctx);
            break;
        }
        case 2: { /* aurora */
            set_rgb_fill(ctx, 0x121A1Fu, 1.0f);
            CGContextFillRect(ctx, CGRectMake(0, 0, 100, 100));
            set_rgb_fill(ctx, 0x8DB596u, 0.55f);
            CGContextBeginPath(ctx);
            CGContextMoveToPoint(ctx, -10, 60);
            CGContextAddCurveToPoint(ctx, 20, 20, 50, 70, 110, 26);
            CGContextAddLineToPoint(ctx, 110, 44);
            CGContextAddCurveToPoint(ctx, 60, 84, 30, 34, -10, 74);
            CGContextClosePath(ctx);
            CGContextFillPath(ctx);

            set_rgb_fill(ctx, 0xB7A0C6u, 0.45f);
            CGContextBeginPath(ctx);
            CGContextMoveToPoint(ctx, -10, 50);
            CGContextAddCurveToPoint(ctx, 30, 14, 58, 60, 110, 16);
            CGContextAddLineToPoint(ctx, 110, 26);
            CGContextAddCurveToPoint(ctx, 60, 66, 30, 26, -10, 62);
            CGContextClosePath(ctx);
            CGContextFillPath(ctx);

            const float stars[6][2] = {{12,14},{30,8},{78,12},{88,40},{60,6},{20,30}};
            for (int i = 0; i < 6; ++i) {
                fill_circle(ctx, stars[i][0], stars[i][1], 0.9f, 0xECEEE8u, 1.0f);
            }
            const float m[] = {0,86, 30,76, 54,84, 80,72, 100,80, 100,100, 0,100};
            fill_poly(ctx, m, 7, 0x0B1013u, 1.0f);
            break;
        }
        case 3: { /* moss */
            set_rgb_fill(ctx, 0x3D5443u, 1.0f);
            CGContextFillRect(ctx, CGRectMake(0, 0, 100, 100));
            const uint32_t cols[4] = {0x8DB596u, 0x6E9479u, 0x56785Fu, 0xA9C4AEu};
            for (int i = 0; i < 26; ++i) {
                float x = (float)((i * 37) % 100);
                float y = (float)((i * 53) % 100);
                float r = (float)(6 + (i * 7) % 14);
                fill_circle(ctx, x, y, r, cols[i % 4], 0.55f);
            }
            fill_circle(ctx, 50.0f, 50.0f, 16.0f, 0xECEEE8u, 0.90f);
            break;
        }
        case 4: { /* berry */
            set_rgb_fill(ctx, 0xE8DCC6u, 1.0f);
            CGContextFillRect(ctx, CGRectMake(0, 0, 100, 100));
            fill_circle(ctx, 50.0f, 50.0f, 28.0f, 0xE2A66Au, 1.0f);
            for (int i = 0; i < 12; ++i) {
                float a = (float)i * 30.0f * (float)M_PI / 180.0f;
                fill_circle(ctx, 50.0f + 16.0f * cosf(a), 50.0f + 16.0f * sinf(a), 6.5f, 0xD08B4Cu, 1.0f);
            }
            fill_circle(ctx, 50.0f, 50.0f, 8.0f, 0xB8733Au, 1.0f);
            break;
        }
        case 5: { /* snow */
            set_rgb_fill(ctx, 0xECEEE8u, 1.0f);
            CGContextFillRect(ctx, CGRectMake(0, 0, 100, 100));
            const float m[] = {0,70, 30,40, 46,56, 64,34, 100,70, 100,100, 0,100};
            fill_poly(ctx, m, 7, 0xA2ABA5u, 1.0f);
            const float s1[] = {30,40, 38,48, 34,50, 46,56, 40,44};
            fill_poly(ctx, s1, 5, 0xFFFFFFu, 1.0f);
            const float s2[] = {64,34, 72,44, 66,44, 74,52, 60,42};
            fill_poly(ctx, s2, 5, 0xFFFFFFu, 1.0f);
            set_rgb_fill(ctx, 0x6F7974u, 1.0f);
            CGContextFillRect(ctx, CGRectMake(0, 78, 100, 22));
            break;
        }
    }

    CGContextRelease(ctx);
    CGColorSpaceRelease(cs);

    for (int i = 0; i < dim * dim; ++i) {
        int r = rgba[i * 4 + 0];
        int g = rgba[i * 4 + 1];
        int b = rgba[i * 4 + 2];
        out_rgb565[i] = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }
    free(rgba);
}

int main(void) {
    @autoreleasepool {
        uint8_t *atlas_pool = (uint8_t *)malloc(2 * 1024 * 1024);
        uint32_t pool_used = 0;

        baked_glyph_t all_glyphs[NUM_FONTS][NUM_CHARS];
        int font_ascent[NUM_FONTS];
        int font_descent[NUM_FONTS];
        int font_line_height[NUM_FONTS];

        CGColorSpaceRef grayCS = CGColorSpaceCreateDeviceGray();

        for (size_t fi = 0; fi < NUM_FONTS; ++fi) {
            CTFontRef font = load_font(kFontSpecs[fi].ps_name, kFontSpecs[fi].fallback_ps, kFontSpecs[fi].size_px);
            CGFloat ascent = CTFontGetAscent(font);
            CGFloat descent = CTFontGetDescent(font);
            CGFloat leading = CTFontGetLeading(font);
            font_ascent[fi] = (int)ceil(ascent);
            font_descent[fi] = (int)ceil(descent);
            font_line_height[fi] = (int)ceil(ascent + descent + leading);

            for (size_t ci = 0; ci < NUM_CHARS; ++ci) {
                uint32_t cp = kCharset[ci];
                UniChar uc = (UniChar)cp;
                CGGlyph glyph = 0;
                CTFontRef drawFont = font;
                bool releaseDrawFont = false;

                if (!CTFontGetGlyphsForCharacters(font, &uc, &glyph, 1) || glyph == 0) {
                    NSString *str = [[NSString alloc] initWithCharacters:&uc length:1];
                    drawFont = CTFontCreateForString(font, (__bridge CFStringRef)str, CFRangeMake(0, 1));
                    releaseDrawFont = true;
                    CTFontGetGlyphsForCharacters(drawFont, &uc, &glyph, 1);
                }

                CGSize adv = CGSizeZero;
                CGRect bbox = CGRectZero;
                CTFontGetAdvancesForGlyphs(drawFont, kCTFontOrientationHorizontal, &glyph, &adv, 1);
                CTFontGetBoundingRectsForGlyphs(drawFont, kCTFontOrientationHorizontal, &glyph, &bbox, 1);

                int pad = 2;
                int gw = (int)ceil(bbox.size.width) + pad * 2;
                int gh = (int)ceil(bbox.size.height) + pad * 2;
                if (cp == 32 || gw <= pad * 2 || gh <= pad * 2) {
                    all_glyphs[fi][ci].codepoint = cp;
                    all_glyphs[fi][ci].width = 0;
                    all_glyphs[fi][ci].height = 0;
                    all_glyphs[fi][ci].bearing_x = 0;
                    all_glyphs[fi][ci].bearing_y = 0;
                    all_glyphs[fi][ci].advance_x = (int16_t)round(adv.width);
                    all_glyphs[fi][ci].bitmap_offset = pool_used;
                    if (releaseDrawFont) CFRelease(drawFont);
                    continue;
                }

                uint8_t *buf = (uint8_t *)calloc((size_t)(gw * gh), 1);
                CGContextRef ctx = CGBitmapContextCreate(buf, (size_t)gw, (size_t)gh, 8, (size_t)gw,
                                                         grayCS, (CGBitmapInfo)kCGImageAlphaNone);
                CGContextSetAllowsAntialiasing(ctx, true);
                CGContextSetShouldAntialias(ctx, true);
                CGContextSetAllowsFontSmoothing(ctx, true);
                CGContextSetShouldSmoothFonts(ctx, false);
                CGContextSetAllowsFontSubpixelPositioning(ctx, true);
                CGContextSetShouldSubpixelPositionFonts(ctx, true);
                CGContextSetGrayFillColor(ctx, 1.0, 1.0);

                CGFloat draw_x = (CGFloat)pad - floor(bbox.origin.x);
                CGFloat draw_y = (CGFloat)pad - floor(bbox.origin.y);
                CGPoint pos = CGPointMake(draw_x, draw_y);
                CTFontDrawGlyphs(drawFont, &glyph, &pos, 1, ctx);
                CGContextRelease(ctx);

                int min_x = gw, max_x = -1, min_y = gh, max_y = -1;
                for (int y = 0; y < gh; ++y) {
                    for (int x = 0; x < gw; ++x) {
                        if (buf[y * gw + x] > 4) {
                            if (x < min_x) min_x = x;
                            if (x > max_x) max_x = x;
                            if (y < min_y) min_y = y;
                            if (y > max_y) max_y = y;
                        }
                    }
                }

                if (max_x < min_x || max_y < min_y) {
                    all_glyphs[fi][ci].codepoint = cp;
                    all_glyphs[fi][ci].width = 0;
                    all_glyphs[fi][ci].height = 0;
                    all_glyphs[fi][ci].bearing_x = 0;
                    all_glyphs[fi][ci].bearing_y = 0;
                    all_glyphs[fi][ci].advance_x = (int16_t)round(adv.width);
                    all_glyphs[fi][ci].bitmap_offset = pool_used;
                } else {
                    int tw = max_x - min_x + 1;
                    int th = max_y - min_y + 1;
                    int baseline_row = (int)round((CGFloat)(gh - 1) - draw_y);
                    int bearing_x = min_x - (int)round(draw_x);
                    int bearing_y = baseline_row - min_y;

                    all_glyphs[fi][ci].codepoint = cp;
                    all_glyphs[fi][ci].width = (uint16_t)tw;
                    all_glyphs[fi][ci].height = (uint16_t)th;
                    all_glyphs[fi][ci].bearing_x = (int16_t)bearing_x;
                    all_glyphs[fi][ci].bearing_y = (int16_t)bearing_y;
                    all_glyphs[fi][ci].advance_x = (int16_t)round(adv.width);
                    all_glyphs[fi][ci].bitmap_offset = pool_used;

                    for (int y = min_y; y <= max_y; ++y) {
                        for (int x = min_x; x <= max_x; ++x) {
                            atlas_pool[pool_used++] = buf[y * gw + x];
                        }
                    }
                }

                free(buf);
                if (releaseDrawFont) CFRelease(drawFont);
            }
            CFRelease(font);
        }
        CGColorSpaceRelease(grayCS);

        const int kTexDim = 180;
        uint16_t *covers = (uint16_t *)malloc((size_t)(6 * kTexDim * kTexDim * sizeof(uint16_t)));
        for (int c = 0; c < 6; ++c) {
            bake_fjall_cover(c, &covers[c * kTexDim * kTexDim], kTexDim);
        }

        FILE *fh = fopen("include/dial_assets.h", "w");
        fprintf(fh, "/**\n * @file dial_assets.h\n * @brief Pre-baked CoreText 8-bit anti-aliased fonts & Fjäll Design System vinyl textures.\n */\n");
        fprintf(fh, "#ifndef DIAL_ASSETS_H\n#define DIAL_ASSETS_H\n\n#include <stdint.h>\n\n");
        fprintf(fh, "#define DIAL_ASSET_CHAR_COUNT %zu\n", NUM_CHARS);
        fprintf(fh, "#define DIAL_VINYL_TEX_DIM    180\n");
        fprintf(fh, "#define DIAL_VINYL_TEX_COUNT  6\n\n");
        fprintf(fh, "typedef enum {\n");
        for (size_t fi = 0; fi < NUM_FONTS; ++fi) {
            fprintf(fh, "    %s = %zu,\n", kFontSpecs[fi].enum_name, fi);
        }
        fprintf(fh, "    DIAL_FONT_FACE_COUNT = %zu\n} dial_font_face_id_t;\n\n", NUM_FONTS);
        fprintf(fh, "typedef struct {\n"
                    "    uint32_t codepoint;\n"
                    "    uint16_t width;\n"
                    "    uint16_t height;\n"
                    "    int16_t  bearing_x;\n"
                    "    int16_t  bearing_y;\n"
                    "    int16_t  advance_x;\n"
                    "    uint32_t bitmap_offset;\n"
                    "} dial_baked_glyph_t;\n\n");
        fprintf(fh, "typedef struct {\n"
                    "    float                    size_px;\n"
                    "    int                      ascent;\n"
                    "    int                      descent;\n"
                    "    int                      line_height;\n"
                    "    int                      is_mono;\n"
                    "    const dial_baked_glyph_t *glyphs;\n"
                    "} dial_baked_font_t;\n\n");
        fprintf(fh, "extern const uint8_t           g_dial_font_atlas[%u];\n", pool_used);
        fprintf(fh, "extern const dial_baked_font_t g_dial_fonts[DIAL_FONT_FACE_COUNT];\n");
        fprintf(fh, "extern const uint16_t          g_dial_vinyl_covers[DIAL_VINYL_TEX_COUNT][DIAL_VINYL_TEX_DIM * DIAL_VINYL_TEX_DIM];\n\n");
        fprintf(fh, "#endif /* DIAL_ASSETS_H */\n");
        fclose(fh);

        FILE *fc = fopen("src/ui/dial_assets.c", "w");
        fprintf(fc, "/**\n * @file dial_assets.c\n * @brief Auto-generated by tools/bake_fjall_assets.m — DO NOT EDIT MANUALLY.\n */\n");
        fprintf(fc, "#include \"dial_assets.h\"\n\n");

        fprintf(fc, "const uint8_t g_dial_font_atlas[%u] = {\n    ", pool_used);
        for (uint32_t i = 0; i < pool_used; ++i) {
            fprintf(fc, "%u,", atlas_pool[i]);
            if ((i & 31) == 31) fprintf(fc, "\n    ");
        }
        fprintf(fc, "\n};\n\n");

        for (size_t fi = 0; fi < NUM_FONTS; ++fi) {
            fprintf(fc, "static const dial_baked_glyph_t kGlyphs_%zu[DIAL_ASSET_CHAR_COUNT] = {\n", fi);
            for (size_t ci = 0; ci < NUM_CHARS; ++ci) {
                const baked_glyph_t *g = &all_glyphs[fi][ci];
                fprintf(fc, "    { 0x%04Xu, %u, %u, %d, %d, %d, %uu },\n",
                        g->codepoint, g->width, g->height, g->bearing_x, g->bearing_y, g->advance_x, g->bitmap_offset);
            }
            fprintf(fc, "};\n\n");
        }

        fprintf(fc, "const dial_baked_font_t g_dial_fonts[DIAL_FONT_FACE_COUNT] = {\n");
        for (size_t fi = 0; fi < NUM_FONTS; ++fi) {
            fprintf(fc, "    { %.1ff, %d, %d, %d, %d, kGlyphs_%zu },\n",
                    kFontSpecs[fi].size_px, font_ascent[fi], font_descent[fi],
                    font_line_height[fi], kFontSpecs[fi].is_mono, fi);
        }
        fprintf(fc, "};\n\n");

        fprintf(fc, "const uint16_t g_dial_vinyl_covers[DIAL_VINYL_TEX_COUNT][DIAL_VINYL_TEX_DIM * DIAL_VINYL_TEX_DIM] = {\n");
        for (int c = 0; c < 6; ++c) {
            fprintf(fc, "    {\n        ");
            for (int i = 0; i < kTexDim * kTexDim; ++i) {
                fprintf(fc, "0x%04Xu,", covers[c * kTexDim * kTexDim + i]);
                if ((i & 15) == 15) fprintf(fc, "\n        ");
            }
            fprintf(fc, "\n    },\n");
        }
        fprintf(fc, "};\n");
        fclose(fc);

        printf("[bake_fjall_assets] Generated %u bytes of 8-bit AA font atlas + %zu KB of Fjäll covers.\n",
               pool_used, (size_t)(6 * kTexDim * kTexDim * 2) / 1024);

        free(covers);
        free(atlas_pool);
    }
    return 0;
}

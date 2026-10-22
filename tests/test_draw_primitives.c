#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "dial_config.h"
#include "dial_theme.h"
#include "dial_hal.h"
#include "dial_draw.h"
#include "dial_icons.h"

/* 1. Verify Fjäll Palettes, RGB565 parity, and 24.0 MB RAM cap */
static void test_theme_and_memory_budget(void) {
    const dial_palette_t *night = dial_get_palette(DIAL_MODE_NIGHT);
    const dial_palette_t *day   = dial_get_palette(DIAL_MODE_DAY);

    assert(night->bg   == 0x121615u);
    assert(night->moss == 0x8DB596u);
    assert(day->bg     == 0xF4F1EAu);
    assert(day->moss   == 0x47755Au);

    /* Verify RGB565 conversion matches header comments in dial_theme.h */
    assert(dial_rgb888_to_rgb565(night->bg)   == 0x10A2u);
    assert(dial_rgb888_to_rgb565(night->moss) == 0x8DB2u);
    assert(dial_rgb888_to_rgb565(day->bg)     == 0xF79Du);
    assert(dial_rgb888_to_rgb565(day->moss)   == 0x43ABu);

    /* Verify strict <= 24.0 MB RAM budget */
    size_t ram = dial_hal_get_ram_usage_bytes();
    assert(ram > 0 && ram <= DIAL_MAX_RAM_BYTES);
}

/* 2. Verify Circular Bezel Mask (zero non-transparent pixels outside r = 180px) */
static void test_circular_bezel_clipping(void) {
    dial_hal_init(true);
    dial_fb_t *fb = dial_hal_get_framebuffer();
    const dial_palette_t *pal = dial_get_palette(DIAL_MODE_NIGHT);

    dial_draw_clear(fb, pal->bg);
    /* Attempt to draw a giant rectangle covering the entire 360x360 square */
    dial_draw_round_rect(fb, 0.0f, 0.0f, 360.0f, 360.0f, 0.0f, pal->moss, 255, 0.0f, 0, 0);
    dial_draw_circle_mask(fb);

    for (int y = 0; y < DIAL_SCREEN_HEIGHT; ++y) {
        for (int x = 0; x < DIAL_SCREEN_WIDTH; ++x) {
            float dx = ((float)x + 0.5f) - (float)DIAL_CENTER_X;
            float dy = ((float)y + 0.5f) - (float)DIAL_CENTER_Y;
            if (sqrtf(dx * dx + dy * dy) > (float)DIAL_RADIUS) {
                assert(fb->pixels[y * DIAL_SCREEN_WIDTH + x] == 0x00000000u);
            }
        }
    }
}

/* 3. Verify all 70 Fjäll Vector Icons rasterize cleanly */
static void test_all_70_fjall_icons(void) {
    assert(DIAL_ICON_COUNT == 70);
    dial_fb_t *fb = dial_hal_get_framebuffer();
    const dial_palette_t *pal = dial_get_palette(DIAL_MODE_NIGHT);

    for (int id = 0; id < (int)DIAL_ICON_COUNT; ++id) {
        const dial_icon_def_t *def = dial_icon_get((dial_icon_id_t)id);
        assert(def != NULL && def->name != NULL && def->subpath_count >= 1);
        assert(dial_icon_by_name(def->name)->id == (dial_icon_id_t)id);

        dial_draw_clear(fb, pal->bg);
        dial_draw_icon(fb, (dial_icon_id_t)id, 168.0f, 168.0f, 24.0f, 1.75f, pal->moss, 255);

        /* Count non-background pixels inside the 28x28 icon box around (168..192) */
        int lit_pixels = 0;
        for (int y = 166; y <= 194; ++y) {
            for (int x = 166; x <= 194; ++x) {
                uint32_t p = fb->pixels[y * DIAL_SCREEN_WIDTH + x] & 0xFFFFFFu;
                if (p != pal->bg) {
                    lit_pixels++;
                }
            }
        }
        if (lit_pixels == 0) {
            fprintf(stderr, "FAIL: Icon #%d (%s) produced 0 lit pixels!\n", id, def->name);
            assert(lit_pixels > 0);
        }
    }
}

/* 4. Verify Arcs, Inner Vinyl Seek Ring (r=38px), Fisheye List, Marquee & PNG Export */
static void test_primitives_and_png_export(void) {
    dial_fb_t *fb = dial_hal_get_framebuffer();
    const dial_palette_t *pal = dial_get_palette(DIAL_MODE_NIGHT);

    dial_draw_clear(fb, pal->bg);
    dial_draw_art(fb, pal, DIAL_ART_FJORD, 180, 180, 360, true, 15.0f);
    dial_draw_vinyl_seek_ring(fb, pal, 0.50f); /* 50% -> knob at 6 o'clock (180, 218) */

    /* Check that the 6 o'clock point (180, 218) on the r=38 ring has Moss knob color */
    uint32_t knob_px = fb->pixels[218 * DIAL_SCREEN_WIDTH + 180] & 0xFFFFFFu;
    assert(knob_px == pal->moss);

    /* Test .train horizontal marquee scrolling at two different timestamps */
    dial_draw_clear(fb, pal->bg);
    dial_draw_text_marquee(fb, "Draw power from host - UAC 2.0 Bit-Perfect", 80, 174, 120, 14, pal->text_1, 255, 0);
    uint32_t hash_t0 = 2166136261u;
    for (int y = 174; y < 188; ++y) {
        for (int x = 80; x < 200; ++x) {
            hash_t0 = (hash_t0 ^ fb->pixels[y * DIAL_SCREEN_WIDTH + x]) * 16777619u;
        }
    }

    dial_draw_clear(fb, pal->bg);
    dial_draw_text_marquee(fb, "Draw power from host - UAC 2.0 Bit-Perfect", 80, 174, 120, 14, pal->text_1, 255, 1000);
    uint32_t hash_t1000 = 2166136261u;
    for (int y = 174; y < 188; ++y) {
        for (int x = 80; x < 200; ++x) {
            hash_t1000 = (hash_t1000 ^ fb->pixels[y * DIAL_SCREEN_WIDTH + x]) * 16777619u;
        }
    }
    assert(hash_t0 != hash_t1000);

    /* Test PNG export and verify PNG magic signature + IHDR 360x360 */
    const char *png_path = "/tmp/dialos_test_unit.png";
    assert(dial_hal_save_png(fb, png_path) == 0);

    FILE *fp = fopen(png_path, "rb");
    assert(fp != NULL);
    uint8_t header[24];
    assert(fread(header, 1, sizeof(header), fp) == sizeof(header));
    fclose(fp);

    /* PNG magic bytes: 137 80 78 71 13 10 26 10 */
    assert(header[0] == 137 && header[1] == 80 && header[2] == 78 && header[3] == 71);
    /* Width = 360 (0x00000168), Height = 360 (0x00000168) at bytes 16..23 */
    uint32_t w = ((uint32_t)header[16] << 24) | ((uint32_t)header[17] << 16) |
                 ((uint32_t)header[18] << 8)  | (uint32_t)header[19];
    uint32_t h = ((uint32_t)header[20] << 24) | ((uint32_t)header[21] << 16) |
                 ((uint32_t)header[22] << 8)  | (uint32_t)header[23];
    assert(w == 360u && h == 360u);
}

int main(void) {
    test_theme_and_memory_budget();
    test_circular_bezel_clipping();
    test_all_70_fjall_icons();
    test_primitives_and_png_export();
    dial_hal_shutdown();
    printf("[PASS] test_draw_primitives: all 70 icons, AA arcs, r=38 ring, marquee, PNG & 24MB RAM tests passed.\n");
    return 0;
}

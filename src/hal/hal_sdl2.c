#include "dial_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#ifdef DIAL_HAS_SDL2
#include <SDL2/SDL.h>
#endif

/* Static arena-allocated framebuffer & IPC ring buffer (zero dynamic heap in render loop) */
static dial_fb_t g_fb;
static bool      g_initialized = false;
static bool      g_headless    = true;

#define DIAL_IPC_RING_CAP 64
static dial_ipc_msg_t g_ipc_ring[DIAL_IPC_RING_CAP];
static size_t         g_ipc_head  = 0;
static size_t         g_ipc_tail  = 0;
static size_t         g_ipc_count = 0;

static uint32_t g_haptic_count   = 0;
static uint16_t g_last_haptic_ms = 0;

#ifdef DIAL_HAS_SDL2
static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture  *g_texture  = NULL;
#endif

bool dial_hal_init(bool headless) {
    memset(&g_fb, 0, sizeof(g_fb));
    g_fb.width  = DIAL_SCREEN_WIDTH;
    g_fb.height = DIAL_SCREEN_HEIGHT;
    g_headless  = headless;
    dial_hal_reset_stats();

#ifdef DIAL_HAS_SDL2
    if (!headless) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            fprintf(stderr, "[HAL] SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }
        g_window = SDL_CreateWindow(
            "DialOS Simulator — Fjäll (360x360)",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            DIAL_SCREEN_WIDTH,
            DIAL_SCREEN_HEIGHT,
            SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
        );
        if (!g_window) {
            fprintf(stderr, "[HAL] SDL_CreateWindow failed: %s\n", SDL_GetError());
            return false;
        }
        g_renderer = SDL_CreateRenderer(
            g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
        );
        if (!g_renderer) {
            g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
        }
        g_texture = SDL_CreateTexture(
            g_renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            DIAL_SCREEN_WIDTH,
            DIAL_SCREEN_HEIGHT
        );
    }
#endif

    g_initialized = true;
    return true;
}

void dial_hal_shutdown(void) {
#ifdef DIAL_HAS_SDL2
    if (g_texture)  { SDL_DestroyTexture(g_texture);   g_texture  = NULL; }
    if (g_renderer) { SDL_DestroyRenderer(g_renderer); g_renderer = NULL; }
    if (g_window)   { SDL_DestroyWindow(g_window);     g_window   = NULL; }
    if (!g_headless) {
        SDL_Quit();
    }
#endif
    g_initialized = false;
}

dial_fb_t *dial_hal_get_framebuffer(void) {
    if (!g_initialized) {
        dial_hal_init(true);
    }
    return &g_fb;
}

void dial_hal_present(dial_fb_t *fb) {
    if (!fb) return;

    /* Sync RGB565 mirror buffer for Snowsky DISC /dev/fb0 parity */
    const size_t total = (size_t)fb->width * (size_t)fb->height;
    for (size_t i = 0; i < total; ++i) {
        fb->rgb565[i] = dial_rgb888_to_rgb565(fb->pixels[i] & 0xFFFFFFu);
    }

#ifdef DIAL_HAS_SDL2
    if (!g_headless && g_renderer && g_texture) {
        SDL_UpdateTexture(g_texture, NULL, fb->pixels, (int)(fb->width * sizeof(uint32_t)));
        SDL_RenderClear(g_renderer);
        SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
        SDL_RenderPresent(g_renderer);
    }
#endif
}

static void write_be32(uint8_t *dst, uint32_t val) {
    dst[0] = (uint8_t)((val >> 24) & 0xFFu);
    dst[1] = (uint8_t)((val >> 16) & 0xFFu);
    dst[2] = (uint8_t)((val >> 8)  & 0xFFu);
    dst[3] = (uint8_t)(val         & 0xFFu);
}

static bool write_png_chunk(FILE *fp, const char type[4], const uint8_t *data, uint32_t len) {
    uint8_t len_be[4];
    uint8_t crc_be[4];
    write_be32(len_be, len);
    if (fwrite(len_be, 1, 4, fp) != 4) return false;
    if (fwrite(type, 1, 4, fp) != 4) return false;

    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (const Bytef *)type, 4);
    if (len > 0 && data != NULL) {
        if (fwrite(data, 1, len, fp) != len) return false;
        crc = crc32(crc, (const Bytef *)data, len);
    }
    write_be32(crc_be, (uint32_t)crc);
    return fwrite(crc_be, 1, 4, fp) == 4;
}

int dial_hal_save_png(const dial_fb_t *fb, const char *filepath) {
    if (!fb || !filepath) return -1;

    const uint32_t w = fb->width;
    const uint32_t h = fb->height;
    const size_t row_bytes = (size_t)w * 4u + 1u; /* Filter byte 0 + RGBA */
    const size_t raw_size  = row_bytes * (size_t)h;

    static uint8_t raw_scanlines[DIAL_SCREEN_HEIGHT * (DIAL_SCREEN_WIDTH * 4 + 1)];
    static uint8_t compressed[DIAL_SCREEN_HEIGHT * (DIAL_SCREEN_WIDTH * 4 + 64)];

    for (uint32_t y = 0; y < h; ++y) {
        uint8_t *row = &raw_scanlines[y * row_bytes];
        row[0] = 0; /* Filter type 0 (None) */
        for (uint32_t x = 0; x < w; ++x) {
            uint32_t argb = fb->pixels[y * w + x];
            row[1 + x * 4 + 0] = (uint8_t)((argb >> 16) & 0xFFu); /* R */
            row[1 + x * 4 + 1] = (uint8_t)((argb >> 8)  & 0xFFu); /* G */
            row[1 + x * 4 + 2] = (uint8_t)(argb         & 0xFFu); /* B */
            row[1 + x * 4 + 3] = (uint8_t)((argb >> 24) & 0xFFu); /* A */
        }
    }

    uLongf comp_len = (uLongf)sizeof(compressed);
    if (compress2(compressed, &comp_len, raw_scanlines, (uLong)raw_size, Z_BEST_COMPRESSION) != Z_OK) {
        return -2;
    }

    FILE *fp = fopen(filepath, "wb");
    if (!fp) return -3;

    static const uint8_t png_sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (fwrite(png_sig, 1, 8, fp) != 8) {
        fclose(fp);
        return -4;
    }

    uint8_t ihdr[13];
    write_be32(&ihdr[0], w);
    write_be32(&ihdr[4], h);
    ihdr[8]  = 8; /* Bit depth: 8 */
    ihdr[9]  = 6; /* Color type: 6 (RGBA) */
    ihdr[10] = 0; /* Compression: Deflate */
    ihdr[11] = 0; /* Filter: Adaptive */
    ihdr[12] = 0; /* Interlace: None */

    bool ok = write_png_chunk(fp, "IHDR", ihdr, 13) &&
              write_png_chunk(fp, "IDAT", compressed, (uint32_t)comp_len) &&
              write_png_chunk(fp, "IEND", NULL, 0);

    fclose(fp);
    return ok ? 0 : -5;
}

uint32_t dial_hal_ticks_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL);
}

bool dial_hal_poll_events(dial_touch_event_t *out_touch,
                          bool *has_touch,
                          dial_key_event_t *out_key,
                          bool *has_key) {
    if (has_touch) *has_touch = false;
    if (has_key)   *has_key   = false;

#ifdef DIAL_HAS_SDL2
    if (!g_headless) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                return false;
            }
            if ((ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) &&
                ev.button.button == SDL_BUTTON_LEFT) {
                if (out_touch && has_touch) {
                    out_touch->x            = (float)ev.button.x;
                    out_touch->y            = (float)ev.button.y;
                    out_touch->pressed      = (ev.type == SDL_MOUSEBUTTONDOWN);
                    out_touch->timestamp_ms = ev.button.timestamp;
                    *has_touch              = true;
                }
                return true;
            }
            if (ev.type == SDL_MOUSEMOTION && (ev.motion.state & SDL_BUTTON_LMASK)) {
                if (out_touch && has_touch) {
                    out_touch->x            = (float)ev.motion.x;
                    out_touch->y            = (float)ev.motion.y;
                    out_touch->pressed      = true;
                    out_touch->timestamp_ms = ev.motion.timestamp;
                    *has_touch              = true;
                }
                return true;
            }
            if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
                if (out_key && has_key) {
                    dial_key_id_t mapped = DIAL_KEY_NONE;
                    if (ev.key.keysym.sym == SDLK_SPACE)  mapped = DIAL_KEY_PLAYPAUSE;
                    if (ev.key.keysym.sym == SDLK_UP)     mapped = DIAL_KEY_VOLUP;
                    if (ev.key.keysym.sym == SDLK_DOWN)   mapped = DIAL_KEY_VOLDOWN;
                    if (ev.key.keysym.sym == SDLK_ESCAPE) mapped = DIAL_KEY_POWER;
                    if (mapped != DIAL_KEY_NONE) {
                        out_key->key          = mapped;
                        out_key->pressed      = (ev.type == SDL_KEYDOWN);
                        out_key->timestamp_ms = ev.key.timestamp;
                        *has_key              = true;
                        return true;
                    }
                }
            }
        }
        return true;
    }
#else
    (void)out_touch;
    (void)out_key;
#endif
    return false;
}

void dial_hal_haptic_tick(uint16_t duration_ms) {
    g_haptic_count++;
    g_last_haptic_ms = duration_ms;
}

uint32_t dial_hal_get_haptic_count(void) {
    return g_haptic_count;
}

uint16_t dial_hal_get_last_haptic_ms(void) {
    return g_last_haptic_ms;
}

void dial_hal_reset_stats(void) {
    g_haptic_count   = 0;
    g_last_haptic_ms = 0;
    g_ipc_head       = 0;
    g_ipc_tail       = 0;
    g_ipc_count      = 0;
}

bool dial_hal_send_ipc(const char *opcode, const char *payload) {
    if (!opcode) return false;
    if (g_ipc_count >= DIAL_IPC_RING_CAP) {
        /* Drop oldest when ring is full */
        g_ipc_tail = (g_ipc_tail + 1) % DIAL_IPC_RING_CAP;
        g_ipc_count--;
    }
    dial_ipc_msg_t *slot = &g_ipc_ring[g_ipc_head];
    memset(slot, 0, sizeof(*slot));
    strncpy(slot->opcode, opcode, sizeof(slot->opcode) - 1);
    if (payload) {
        strncpy(slot->payload, payload, sizeof(slot->payload) - 1);
    }
    slot->timestamp_ms = dial_hal_ticks_ms();
    g_ipc_head = (g_ipc_head + 1) % DIAL_IPC_RING_CAP;
    g_ipc_count++;
    return true;
}

bool dial_hal_pop_ipc(dial_ipc_msg_t *out_msg) {
    if (g_ipc_count == 0) return false;
    if (out_msg) {
        *out_msg = g_ipc_ring[g_ipc_tail];
    }
    g_ipc_tail = (g_ipc_tail + 1) % DIAL_IPC_RING_CAP;
    g_ipc_count--;
    return true;
}

size_t dial_hal_get_ipc_count(void) {
    return g_ipc_count;
}

size_t dial_hal_get_ram_usage_bytes(void) {
    /* Static arena footprint: framebuffer + IPC ring + PNG scanline buffers */
    return sizeof(g_fb) + sizeof(g_ipc_ring) +
           (size_t)(DIAL_SCREEN_HEIGHT * (DIAL_SCREEN_WIDTH * 4 + 1)) * 2u;
}

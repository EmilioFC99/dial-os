#ifndef DIAL_HAL_H
#define DIAL_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dial_config.h"
#include "dial_theme.h"
#include "dial_ipc.h"

/* 360x360 Circular Framebuffer (0xAARRGGBB in software, RGB565 on /dev/fb0) */
typedef struct {
    uint32_t pixels[DIAL_SCREEN_WIDTH * DIAL_SCREEN_HEIGHT];
    uint16_t rgb565[DIAL_SCREEN_WIDTH * DIAL_SCREEN_HEIGHT];
    uint32_t width;
    uint32_t height;
} dial_fb_t;

/* Touch Event from /dev/input/event0 or Simulator Mouse */
typedef struct {
    float    x;
    float    y;
    bool     pressed;
    uint32_t timestamp_ms;
} dial_touch_event_t;

/* Physical Tactile Hardware Buttons (/dev/input/event1) */
typedef enum {
    DIAL_KEY_NONE      = 0,
    DIAL_KEY_POWER     = 1, /* GPIO_PA02: Short=Sleep/Wake, Long=Power Sheet */
    DIAL_KEY_PLAYPAUSE = 2, /* GPIO_PA03: 1x=Play/Pause, 2x=Next, 3x=Prev */
    DIAL_KEY_VOLUP     = 3, /* GPIO_PA04: +1 step / 10 steps/s hold */
    DIAL_KEY_VOLDOWN   = 4  /* GPIO_PA05: -1 step / 10 steps/s hold */
} dial_key_id_t;

typedef struct {
    dial_key_id_t key;
    bool          pressed;
    uint32_t      timestamp_ms;
} dial_key_event_t;

/* IPC Command Record (In-Memory Queue for Simulator & Unit Tests) */
typedef struct {
    char     opcode[8];
    char     payload[128];
    uint32_t timestamp_ms;
} dial_ipc_msg_t;

/* Initialize and tear down HAL backend */
bool dial_hal_init(bool headless);
void dial_hal_shutdown(void);

/* Acquire singleton static 360x360 framebuffer (zero per-frame malloc) */
dial_fb_t *dial_hal_get_framebuffer(void);

/* Present framebuffer to SDL2 window or sync RGB565 buffer */
void dial_hal_present(dial_fb_t *fb);

/* Export 360x360 framebuffer to 32-bit RGBA PNG file */
int dial_hal_save_png(const dial_fb_t *fb, const char *filepath);

/* Convert 24-bit 0xRRGGBB to 16-bit RGB565 (matches dial_theme.h comments) */
static inline uint16_t dial_rgb888_to_rgb565(uint32_t rgb) {
    uint32_t r = (rgb >> 16) & 0xFFu;
    uint32_t g = (rgb >> 8)  & 0xFFu;
    uint32_t b =  rgb        & 0xFFu;
    return (uint16_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
}

/* Poll desktop window events (returns false when quit is requested) */
bool dial_hal_poll_events(dial_touch_event_t *out_touch,
                          bool *has_touch,
                          dial_key_event_t *out_key,
                          bool *has_key);

/* Monotonic clock in milliseconds */
uint32_t dial_hal_ticks_ms(void);

/* Haptic LRA feedback (DRV2605L / simulator log) */
void     dial_hal_haptic_tick(uint16_t duration_ms);
uint32_t dial_hal_get_haptic_count(void);
uint16_t dial_hal_get_last_haptic_ms(void);
void     dial_hal_reset_stats(void);

/* POSIX /player & /ui IPC message dispatch */
bool dial_hal_send_ipc(const char *opcode, const char *payload);
bool dial_hal_pop_ipc(dial_ipc_msg_t *out_msg);
size_t dial_hal_get_ipc_count(void);

/* Memory budget monitor (must remain <= DIAL_MAX_RAM_BYTES = 24.0 MB) */
size_t dial_hal_get_ram_usage_bytes(void);

#endif /* DIAL_HAL_H */

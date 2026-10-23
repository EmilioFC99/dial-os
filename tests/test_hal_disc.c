/**
 * @file test_hal_disc.c
 * @brief Hardware Abstraction Layer Verification for FiiO Snowsky DISC (hal_disc.c)
 *
 * Links directly against `src/hal/hal_disc.c` (`-DDIAL_TARGET_DISC`) with zero
 * macOS frameworks to verify:
 *   1. Portable compilation & strict HAL boundary isolation
 *   2. 360x360 RGB888 -> RGB565 framebuffer conversion (`dial_hal_present`)
 *   3. Raw RGB565 framebuffer snapshot output (`259,200` bytes)
 *   4. Haptic LRA tick telemetry (`15 ms` detent, `25 ms` boundary)
 *   5. 32-slot `/player` IPC ring buffer & overflow wrap-around
 *   6. All 38 screens rendered cleanly on the Snowsky DISC HAL within 24.0 MB RAM
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "dial_app.h"
#include "dial_config.h"
#include "dial_hal.h"
#include "dial_ipc.h"
#include "dial_screens.h"

int main(void) {
    assert(dial_hal_init(true));
    dial_hal_reset_stats();

    dial_fb_t *fb = dial_hal_get_framebuffer();
    assert(fb != NULL);
    assert(fb->width == DIAL_SCREEN_WIDTH);
    assert(fb->height == DIAL_SCREEN_HEIGHT);

    /* 1. Render all 38 master screens on the Snowsky DISC backend & verify RGB565 output */
    dial_app_t *app = dial_app_get_state();
    size_t screen_count = 0;
    const dial_screen_desc_t *all_screens = dial_screen_get_all(&screen_count);
    assert(all_screens != NULL);
    assert(screen_count >= 38);

    for (size_t i = 0; i < screen_count; ++i) {
        dial_app_init(app, all_screens[i].id, (i % 2 == 0) ? DIAL_MODE_NIGHT : DIAL_MODE_DAY);
        dial_app_render(app, fb, 1200);
        dial_hal_present(fb);

        /* Verify center pixel RGB888 -> RGB565 conversion matches exactly */
        size_t center_idx = 180u * DIAL_SCREEN_WIDTH + 180u;
        uint16_t expected_565 = dial_rgb888_to_rgb565(fb->pixels[center_idx]);
        assert(fb->rgb565[center_idx] == expected_565);
    }

    /* 2. Verify raw RGB565 framebuffer dump is exactly 360 * 360 * 2 = 259,200 bytes */
    const char *raw_path = "/tmp/dialos_test_disc_fb.raw";
    assert(dial_hal_save_png(fb, raw_path) == 0);
    struct stat st;
    assert(stat(raw_path, &st) == 0);
    assert(st.st_size == (off_t)(DIAL_SCREEN_WIDTH * DIAL_SCREEN_HEIGHT * sizeof(uint16_t)));

    /* 3. Verify haptic LRA tick counter & duration tracking */
    dial_hal_reset_stats();
    assert(dial_hal_get_haptic_count() == 0);
    dial_hal_haptic_tick(15);
    assert(dial_hal_get_haptic_count() == 1);
    assert(dial_hal_get_last_haptic_ms() == 15);
    dial_hal_haptic_tick(25);
    assert(dial_hal_get_haptic_count() == 2);
    assert(dial_hal_get_last_haptic_ms() == 25);

    /* 4. Verify 32-slot /player IPC ring buffer and wrap-around behavior */
    dial_hal_reset_stats();
    for (int i = 0; i < 40; ++i) {
        char payload[32];
        snprintf(payload, sizeof(payload), "%d", i);
        assert(dial_hal_send_ipc(DIAL_CMD_SEEK, payload));
    }
    assert(dial_hal_get_ipc_count() == 32);

    dial_ipc_msg_t msg;
    assert(dial_hal_pop_ipc(&msg));
    assert(strcmp(msg.opcode, DIAL_CMD_SEEK) == 0);
    /* First 8 messages (0..7) were overwritten by wrap-around; oldest is now "8" */
    assert(strcmp(msg.payload, "8") == 0);

    /* 5. Verify Real-Hardware Patch #1: 180-degree rotated 32 bpp XRGB8888 ingenicfb blit */
    static uint32_t hw_xrgb[DIAL_SCREEN_WIDTH * DIAL_SCREEN_HEIGHT];
    fb->pixels[0] = 0xFF112233u; /* Top-left logical pixel */
    fb->pixels[DIAL_SCREEN_WIDTH * DIAL_SCREEN_HEIGHT - 1] = 0xFFAABBCCu; /* Bottom-right logical pixel */
    dial_disc_blit_rotated_xrgb8888(hw_xrgb, DIAL_SCREEN_WIDTH, fb->pixels, DIAL_SCREEN_WIDTH, DIAL_SCREEN_HEIGHT);
    assert(hw_xrgb[DIAL_SCREEN_WIDTH * DIAL_SCREEN_HEIGHT - 1] == 0xFF112233u);
    assert(hw_xrgb[0] == 0xFFAABBCCu);

    /* 6. Verify Real-Hardware Patch #2: event0/event1 node mapping & 180-deg cst816t touch inversion */
    assert(strcmp(DIAL_DEV_KEYS,  "/dev/input/event0") == 0);
    assert(strcmp(DIAL_DEV_TOUCH, "/dev/input/event1") == 0);
    float tx = 0.0f, ty = 0.0f;
    dial_disc_map_touch_180(10, 25, &tx, &ty);
    assert(tx == 349.0f && ty == 334.0f);
    dial_disc_map_touch_180(359, 359, &tx, &ty);
    assert(tx == 0.0f && ty == 0.0f);

    /* 7. Verify Real-Hardware Patch #4: FiiO /player POSIX mqueue wire-frame encoding */
    char wire[64];
    assert(dial_ipc_format_frame(wire, sizeof(wire), DIAL_CMD_PLAY_PAUSE, "pause") == 12);
    assert(strcmp(wire, "0201000C0000") == 0);
    assert(dial_ipc_format_frame(wire, sizeof(wire), DIAL_CMD_NEXT_TRACK, "next") == 12);
    assert(strcmp(wire, "0647000C0000") == 0);
    assert(dial_ipc_format_frame(wire, sizeof(wire), DIAL_CMD_PREV_TRACK, "prev") == 12);
    assert(strcmp(wire, "0649000C0000") == 0);
    assert(dial_ipc_format_frame(wire, sizeof(wire), DIAL_CMD_SEEK, "45000") == 16);
    assert(strcmp(wire, "010300100000AFC8") == 0);
    assert(dial_ipc_format_frame(wire, sizeof(wire), DIAL_CMD_SET_VOLUME, "100") == 12);
    assert(strcmp(wire, "0715000C0078") == 0);

    /* 8. Verify non-blocking event poll and 24.0 MB RAM ceiling */
    dial_touch_event_t touch;
    dial_key_event_t   key;
    bool has_t = true, has_k = true;
    assert(dial_hal_poll_events(&touch, &has_t, &key, &has_k));
    assert(!has_t && !has_k);

    size_t ram_bytes = dial_hal_get_ram_usage_bytes();
    assert(ram_bytes > 0 && ram_bytes <= DIAL_MAX_RAM_BYTES);

    dial_hal_shutdown();
    printf("[PASS] test_hal_disc: Snowsky DISC HAL, 180-deg XRGB8888 blit, cst816t touch, mq_player wire frames & RAM (%.2f KB) verified.\n",
           (double)ram_bytes / 1024.0);
    return 0;
}

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "dial_app.h"

/* Write a valid 44.1kHz 16-bit stereo RIFF WAVE file of `dur_sec` seconds */
static void write_test_wav(const char *filepath, uint32_t dur_sec) {
    FILE *fp = fopen(filepath, "wb");
    assert(fp != NULL);

    uint32_t sample_rate = 44100u;
    uint16_t channels    = 2u;
    uint16_t bits        = 16u;
    uint32_t byte_rate   = sample_rate * channels * (bits / 8u);
    uint32_t data_bytes  = byte_rate * dur_sec;
    uint32_t riff_size   = 36u + data_bytes;
    uint16_t block_align = channels * (bits / 8u);

    uint8_t hdr[44];
    memcpy(&hdr[0],  "RIFF", 4);
    hdr[4]  = (uint8_t)(riff_size & 0xFFu);
    hdr[5]  = (uint8_t)((riff_size >> 8) & 0xFFu);
    hdr[6]  = (uint8_t)((riff_size >> 16) & 0xFFu);
    hdr[7]  = (uint8_t)((riff_size >> 24) & 0xFFu);
    memcpy(&hdr[8],  "WAVEfmt ", 8);
    hdr[16] = 16; hdr[17] = 0; hdr[18] = 0; hdr[19] = 0; /* PCM chunk size 16 */
    hdr[20] = 1;  hdr[21] = 0;                           /* AudioFormat = 1 (PCM) */
    hdr[22] = (uint8_t)channels; hdr[23] = 0;
    hdr[24] = (uint8_t)(sample_rate & 0xFFu);
    hdr[25] = (uint8_t)((sample_rate >> 8) & 0xFFu);
    hdr[26] = (uint8_t)((sample_rate >> 16) & 0xFFu);
    hdr[27] = (uint8_t)((sample_rate >> 24) & 0xFFu);
    hdr[28] = (uint8_t)(byte_rate & 0xFFu);
    hdr[29] = (uint8_t)((byte_rate >> 8) & 0xFFu);
    hdr[30] = (uint8_t)((byte_rate >> 16) & 0xFFu);
    hdr[31] = (uint8_t)((byte_rate >> 24) & 0xFFu);
    hdr[32] = (uint8_t)block_align; hdr[33] = 0;
    hdr[34] = (uint8_t)bits;        hdr[35] = 0;
    memcpy(&hdr[36], "data", 4);
    hdr[40] = (uint8_t)(data_bytes & 0xFFu);
    hdr[41] = (uint8_t)((data_bytes >> 8) & 0xFFu);
    hdr[42] = (uint8_t)((data_bytes >> 16) & 0xFFu);
    hdr[43] = (uint8_t)((data_bytes >> 24) & 0xFFu);

    fwrite(hdr, 1, sizeof(hdr), fp);
    uint8_t silence[4096];
    memset(silence, 0, sizeof(silence));
    uint32_t rem = data_bytes;
    while (rem > 0) {
        uint32_t chunk = (rem > sizeof(silence)) ? (uint32_t)sizeof(silence) : rem;
        fwrite(silence, 1, chunk, fp);
        rem -= chunk;
    }
    fclose(fp);
}

static void sim_tap(dial_app_t *app, float x, float y, uint32_t *io_ts) {
    dial_touch_event_t dn = { .x = x, .y = y, .pressed = true,  .timestamp_ms = *io_ts };
    dial_touch_event_t up = { .x = x, .y = y, .pressed = false, .timestamp_ms = *io_ts + 30u };
    dial_app_handle_touch(app, &dn);
    dial_app_handle_touch(app, &up);
    *io_ts += 300u;
    dial_app_tick(app, *io_ts);
}

static void sim_swipe(dial_app_t *app, float x0, float y0, float x1, float y1, uint32_t *io_ts) {
    dial_touch_event_t dn = { .x = x0, .y = y0, .pressed = true, .timestamp_ms = *io_ts };
    dial_app_handle_touch(app, &dn);
    for (int s = 1; s <= 4; ++s) {
        float t = (float)s / 4.0f;
        dial_touch_event_t mv = {
            .x = x0 + (x1 - x0) * t,
            .y = y0 + (y1 - y0) * t,
            .pressed = true,
            .timestamp_ms = *io_ts + (uint32_t)(s * 15)
        };
        dial_app_handle_touch(app, &mv);
    }
    dial_touch_event_t up = { .x = x1, .y = y1, .pressed = false, .timestamp_ms = *io_ts + 80u };
    dial_app_handle_touch(app, &up);
    *io_ts += 100u;
    dial_app_tick(app, *io_ts);
}

int main(void) {
    dial_hal_init(true);
    dial_fb_t *fb = dial_hal_get_framebuffer();

    /* 1. Build a real temporary directory hierarchy with real WAV files */
    (void)system("rm -rf /tmp/dialos_test_music_p1 && mkdir -p '/tmp/dialos_test_music_p1/Ambient/Nordic Fell' '/tmp/dialos_test_music_p1/EmptyFolder'");
    write_test_wav("/tmp/dialos_test_music_p1/Ambient/Nordic Fell/01 - Morning Moss.wav", 2);
    write_test_wav("/tmp/dialos_test_music_p1/Ambient/Nordic Fell/02 - Fjord Wind.wav", 2);
    write_test_wav("/tmp/dialos_test_music_p1/Ambient/Nordic Fell/03 - Cloudberry Sun.wav", 2);

    assert(dial_media_scan_root("/tmp/dialos_test_music_p1") == true);
    assert(dial_media_get_album_count() == 1);
    assert(dial_media_get_artist_count() == 1);
    assert(dial_media_get_total_track_count() == 3);

    /* Starts cleanly in "Not Playing" state (zero auto-started fake songs) */
    assert(dial_media_has_active_track() == false);
    assert(strcmp(dial_media_get_now_title(), "Not Playing") == 0);

    dial_app_t app;
    dial_app_init(&app, DIAL_MODE_NIGHT);
    uint32_t ts = 1000u;
    dial_app_render(&app, fb);

    /* 2. Home Launcher: swipe left twice to focus Files (idx = 2) and tap to open */
    assert(app.current_screen == DIAL_SCREEN_HOME);
    sim_swipe(&app, 240.0f, 180.0f, 120.0f, 180.0f, &ts);
    assert(app.home_focus_idx == 1); /* Library */
    sim_swipe(&app, 240.0f, 180.0f, 120.0f, 180.0f, &ts);
    assert(app.home_focus_idx == 2); /* Files */
    dial_app_render(&app, fb);
    dial_hal_save_png(fb, "/tmp/dialos_p1_home_files.png");

    sim_tap(&app, 180.0f, 118.0f, &ts);
    assert(app.current_screen == DIAL_SCREEN_FILES);
    assert(app.nav_depth == 1);
    assert(dial_media_is_at_root_dir() == true);
    assert(dial_media_get_fs_entry_count() == 2); /* Ambient, EmptyFolder */
    dial_app_render(&app, fb);

    /* 3. Files Browser: Enter EmptyFolder (row 1), verify FIL-04, Left-Edge Swipe-Right back */
    sim_tap(&app, 180.0f, 140.0f, &ts);
    assert(dial_media_is_at_root_dir() == false);
    dial_app_render(&app, fb);
    dial_hal_save_png(fb, "/tmp/dialos_p1_files_empty.png");

    /* Universal Left-Edge Swipe-Right (start_x = 18 < 48px -> dx = +62 > 45px) */
    sim_swipe(&app, 18.0f, 180.0f, 80.0f, 180.0f, &ts);
    assert(app.current_screen == DIAL_SCREEN_FILES);
    assert(dial_media_is_at_root_dir() == true);

    /* 4. Drill down into Ambient -> Nordic Fell -> Tap 01 - Morning Moss.wav */
    sim_tap(&app, 180.0f, 90.0f, &ts); /* Enter Ambient */
    assert(dial_media_is_at_root_dir() == false);
    /* Row 0 is "..", Row 1 is "Nordic Fell" */
    sim_tap(&app, 180.0f, 140.0f, &ts); /* Enter Nordic Fell */
    assert(dial_media_get_fs_entry_count() == 4); /* "..", 01, 02, 03 */
    dial_app_render(&app, fb);
    dial_hal_save_png(fb, "/tmp/dialos_p1_files_list.png");

    /* Tap Row 1 ("01 - Morning Moss.wav") -> plays real file and opens Now Playing */
    sim_tap(&app, 180.0f, 140.0f, &ts);
    assert(app.current_screen == DIAL_SCREEN_NOW_PLAYING);
    assert(app.nav_depth == 2);
    assert(dial_media_has_active_track() == true);
    assert(strcmp(dial_media_get_now_title(), "Morning Moss") == 0);
    assert(dial_media_get_now_sample_rate() == 44100u);
    assert(dial_media_get_now_bit_depth() == 16u);
    dial_media_seek_progress(0.38f);
    dial_app_render(&app, fb);
    dial_hal_save_png(fb, "/tmp/dialos_p1_np01.png");

    /* 5. Now Playing: Single tap opens NP-02 Quick Actions (Fav, Mode cycle, Playlist, Lyrics) */
    sim_tap(&app, 180.0f, 180.0f, &ts);
    assert(app.np_qa_open == true);
    assert(dial_media_get_play_mode() == DIAL_PLAY_MODE_SEQUENTIAL);
    /* Tap Mode puck at (142, 244) to cycle: Sequential -> Random -> Repeat All -> Repeat One -> Sequential */
    sim_tap(&app, 142.0f, 244.0f, &ts);
    assert(dial_media_get_play_mode() == DIAL_PLAY_MODE_RANDOM);
    dial_app_render(&app, fb);
    sim_tap(&app, 142.0f, 244.0f, &ts);
    assert(dial_media_get_play_mode() == DIAL_PLAY_MODE_REPEAT_ALL);
    sim_tap(&app, 142.0f, 244.0f, &ts);
    assert(dial_media_get_play_mode() == DIAL_PLAY_MODE_REPEAT_ONE);
    sim_tap(&app, 142.0f, 244.0f, &ts);
    assert(dial_media_get_play_mode() == DIAL_PLAY_MODE_SEQUENTIAL);

    /* Tap Add to Playlist puck at (218, 244), then tap first playlist row */
    sim_tap(&app, 218.0f, 244.0f, &ts);
    assert(app.np_playlist_open == true);
    dial_app_render(&app, fb);
    sim_tap(&app, 180.0f, 95.0f, &ts);
    assert(app.np_playlist_open == false);
    assert(app.np_added_to_playlist == true);

    /* Tap Lyrics puck at (294, 216), then close via Left-Edge Swipe-Right */
    sim_tap(&app, 294.0f, 216.0f, &ts);
    assert(app.np_lyrics_open == true);
    dial_app_render(&app, fb);
    sim_swipe(&app, 18.0f, 180.0f, 85.0f, 180.0f, &ts);
    assert(app.np_lyrics_open == false);

    /* Close NP-02 back to NP-01 */
    sim_tap(&app, 180.0f, 80.0f, &ts);
    assert(app.np_qa_open == false);

    /* Verify Right-Rim Volume Slider: moving finger UP increases volume, DOWN decreases volume */
    sim_swipe(&app, 336.0f, 240.0f, 310.0f, 85.0f, &ts);
    int vol_top = dial_media_get_volume();
    sim_swipe(&app, 310.0f, 85.0f, 336.0f, 240.0f, &ts);
    int vol_bot = dial_media_get_volume();
    assert(vol_top > vol_bot);

    /* Verify Seeking grabs current disc rotation and rotates 1:1 with finger position */
    app.vinyl_spin_deg = 120.0f;
    dial_touch_event_t sk0 = { .x = 180.0f, .y = 142.0f, .pressed = true,  .timestamp_ms = ts };      /* 0 deg */
    dial_touch_event_t sk1 = { .x = 218.0f, .y = 180.0f, .pressed = true,  .timestamp_ms = ts + 40 }; /* 90 deg (+90) */
    dial_touch_event_t sk2 = { .x = 180.0f, .y = 218.0f, .pressed = true,  .timestamp_ms = ts + 80 }; /* 180 deg (+90) */
    dial_touch_event_t sk3 = { .x = 180.0f, .y = 218.0f, .pressed = false, .timestamp_ms = ts + 90 };
    ts += 100;
    dial_app_handle_touch(&app, &sk0);
    dial_app_handle_touch(&app, &sk1);
    assert(fabsf(app.vinyl_spin_deg - 210.0f) < 1.0f);
    dial_app_handle_touch(&app, &sk2);
    assert(fabsf(app.vinyl_spin_deg - 300.0f) < 1.0f);
    dial_app_handle_touch(&app, &sk3);
    assert(app.vinyl_grabbed == false);

    /* 6. Now Playing: Swipe Left skips to next track in folder ("02 - Fjord Wind.wav") */
    sim_swipe(&app, 240.0f, 180.0f, 120.0f, 180.0f, &ts);
    assert(strcmp(dial_media_get_now_title(), "Fjord Wind") == 0);

    /* Swipe Right inside vinyl stage (start_x = 100 >= 48px) skips back to previous track */
    sim_swipe(&app, 100.0f, 180.0f, 200.0f, 180.0f, &ts);
    assert(strcmp(dial_media_get_now_title(), "Morning Moss") == 0);

    /* 7. Bottom Swipe-Up opens NP-04 Track Details; Left-Edge Swipe-Right closes it */
    sim_swipe(&app, 180.0f, 340.0f, 180.0f, 280.0f, &ts);
    assert(app.np_details_open == true);
    dial_app_render(&app, fb);
    dial_hal_save_png(fb, "/tmp/dialos_p1_np04.png");
    sim_swipe(&app, 18.0f, 180.0f, 85.0f, 180.0f, &ts);
    assert(app.np_details_open == false);
    assert(app.current_screen == DIAL_SCREEN_NOW_PLAYING);

    /* 8. Universal Left-Edge Swipe-Right pops Now Playing -> Files (Nordic Fell) -> Ambient -> Root -> Home */
    sim_swipe(&app, 18.0f, 180.0f, 85.0f, 180.0f, &ts);
    assert(app.current_screen == DIAL_SCREEN_FILES);
    sim_swipe(&app, 18.0f, 180.0f, 85.0f, 180.0f, &ts); /* Nordic Fell -> Ambient */
    sim_swipe(&app, 18.0f, 180.0f, 85.0f, 180.0f, &ts); /* Ambient -> Root */
    assert(dial_media_is_at_root_dir() == true);
    sim_swipe(&app, 18.0f, 180.0f, 85.0f, 180.0f, &ts); /* Root -> Home Launcher */
    assert(app.current_screen == DIAL_SCREEN_HOME);
    assert(app.nav_depth == 0);

    /* 9. Top-Rim Pull-Down opens Control Center (CC-01), Bottom Swipe-Up closes it */
    sim_swipe(&app, 180.0f, 16.0f, 180.0f, 70.0f, &ts);
    assert(app.cc_open == true);
    dial_app_render(&app, fb);
    dial_hal_save_png(fb, "/tmp/dialos_p1_cc01.png");
    sim_swipe(&app, 180.0f, 340.0f, 180.0f, 280.0f, &ts);
    assert(app.cc_open == false);

    dial_media_set_playing(false);
    dial_hal_shutdown();
    (void)system("rm -rf /tmp/dialos_test_music_p1 /tmp/dialos_p1_*.png");
    printf("[PASS] test_phase1_app: nav_stack, Left-Edge Swipe-Right, Home, Files & Now Playing verified.\n");
    return 0;
}

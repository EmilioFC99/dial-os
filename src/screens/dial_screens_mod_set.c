#include "dial_screens.h"
#include "dial_app.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *icon;
    const char *picker_sub;
    const char *state_label;
    int         state_tone; /* 0=moss, 1=fjord, 2=cloudberry */
    bool        live;
    bool        waiting_breathe;
    const char *hero;
    const char *unit;
    bool        hero_small;
    const char *sub;
    const char *row_icon;
    const char *row_title;
    bool        row_marquee;
    bool        row_has_chev;
    bool        row_has_toggle;
    const char *row_chip;
    int         row_chip_tone; /* 0=neutral, 1=fjord */
    const char *out_pill;
} mode_spec_t;

static const mode_spec_t G_MODES[6] = {
    {
        "Player", "mode_player", "Local playback · microSD",
        "Active", 0, true, false,
        "Ready", NULL, true, "microSD · 1,428 tracks",
        "play", "Resume “Teardrop”", false, true, false, NULL, 0,
        "4.4 mm BAL · High gain"
    },
    {
        "USB DAC", "mode_usbdac", "UAC 2.0 · 384 kHz / 32-bit",
        "Locked · bit-perfect", 0, true, false,
        "384", "kHz", false, "32-bit · iPhone 16 Pro · UAC 2.0",
        "usb_charge", "Draw power from host", true, false, true, NULL, 0,
        "4.4 mm BAL · Coax on"
    },
    {
        "Transport", "mode_transport", "USB-C / Coaxial digital out",
        "Streaming", 0, true, false,
        "DSD128", NULL, false, "Native · USB-C -> external DAC",
        "chip", "Topping D90 SE", false, false, false, "USB", 0,
        "Digital out only"
    },
    {
        "Bluetooth RX", "mode_btrx", "LDAC / aptX HD receiver",
        "Connected", 1, true, false,
        "LDAC", NULL, false, "990 kbps · 24-bit / 96 kHz",
        "bluetooth", "Emilio's iPhone", false, false, false, "-52 dBm", 1,
        "4.4 mm BAL · Low gain"
    },
    {
        "AirPlay", "mode_airplay", "Lossless Wi-Fi receiver",
        "Waiting for host", 0, false, true,
        "Visible as “DISC”", NULL, true, "Wi-Fi · Studio 5G · 2.4 GHz",
        "wifi", "Studio 5G", false, false, false, "Strong", 1,
        "4.4 mm BAL · Coax on"
    },
    {
        "File Transfer", "mode_transfer", "USB mass storage",
        "Do not unplug", 2, true, false,
        "48", "MB/s", false, "Card reader · exFAT · Mac Studio",
        "sdcard", "482 GB free of 1 TB", false, false, false, "52%", 0,
        "Audio paused"
    }
};

/* ── 1. MOD-00: 6-Mode Ring Picker (Also Opened by Holding Mode Puck in CC-01) ── */
void dial_render_mod_00(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    const dial_app_t *app = dial_app_get_state();
    int focus = (app->mode_picker_focus >= 0) ? (app->mode_picker_focus % 6) : 0;

    dial_draw_clear(fb, pal->bg);

    for (int i = 0; i < 6; ++i) {
        float px = 0.0f, py = 0.0f;
        dial_polar_to_xy((float)(i * 60), 116.0f, &px, &py);
        bool on = (i == focus);
        uint32_t fill = on ? dial_color_lerp(pal->surface_1, pal->moss, 46) : pal->surface_1;
        uint32_t fg   = on ? pal->moss : pal->text_2;

        dial_draw_circle_fill(fb, px, py, 26.0f, fill, 255);
        dial_draw_circle_stroke(fb, px, py, 25.5f, 1.0f, on ? pal->moss : pal->line, 255);
        if (on) {
            dial_draw_circle_stroke(fb, px, py, 30.0f, 1.5f, pal->moss, 255);
        }
        dial_draw_icon_by_name(fb, G_MODES[i].icon, px - 11.0f, py - 11.0f, 22.0f, 1.75f, fg, 255);
    }

    const mode_spec_t *m = &G_MODES[focus];
    dial_draw_text(fb, m->name,       DIAL_CENTER_X, 140, 20, pal->text_1, 255, true);
    dial_draw_text(fb, m->picker_sub, DIAL_CENTER_X, 166, 12, pal->text_2, 255, true);

    const char *chip_txt = (focus == app->active_mode_idx) ? "• Active" : "Tap to switch";
    int cw = dial_measure_text(chip_txt, 10, 0) + 14;
    dial_draw_chip(fb, pal, DIAL_CENTER_X - cw / 2, 192, chip_txt, pal->moss, NULL);

    dial_draw_circle_mask(fb);
}

/* ── Shared Renderer for MOD-01..MOD-06 ── */
static void render_mode_template(dial_fb_t *fb,
                                 const dial_palette_t *pal,
                                 int mode_idx,
                                 uint32_t tick_ms) {
    const mode_spec_t *m = &G_MODES[mode_idx % 6];
    const dial_app_t *app = dial_app_get_state();

    dial_draw_clear(fb, pal->bg);

    char overline[48];
    snprintf(overline, sizeof(overline), "Mode · %s", m->name);
    dial_draw_header(fb, pal, overline, "", 36);

    /* Center 68px mode pebble at (180, 96) */
    if (m->waiting_breathe) {
        dial_draw_circle_stroke(fb, 180.0f, 96.0f, 48.0f, 1.5f, pal->moss, 140);
    }
    uint32_t p_fill = dial_color_lerp(pal->surface_1, pal->moss, 46);
    dial_draw_circle_fill(fb, 180.0f, 96.0f, 34.0f, p_fill, 255);
    dial_draw_circle_stroke(fb, 180.0f, 96.0f, 33.5f, 1.2f, pal->moss, 120);
    if (m->live) {
        dial_draw_circle_stroke(fb, 180.0f, 96.0f, 38.5f, 1.5f, pal->moss, 255);
    }
    dial_draw_icon_by_name(fb, m->icon, 165.0f, 81.0f, 30.0f, 1.75f, pal->moss, 255);

    /* State chip at y = 146 */
    uint32_t st_col = (m->state_tone == 1) ? pal->fjord :
                      (m->state_tone == 2) ? pal->cloudberry : pal->moss;
    char st_buf[48];
    snprintf(st_buf, sizeof(st_buf), "• %s", m->state_label);
    int sw = dial_measure_text(st_buf, 10, 0) + 14;
    dial_draw_chip(fb, pal, DIAL_CENTER_X - sw / 2, 146, st_buf, st_col, NULL);

    /* Hero value + unit at y = 174 */
    int hero_px = m->hero_small ? 20 : 36;
    if (m->unit) {
        int hw = dial_measure_text(m->hero, hero_px, 0);
        int uw = dial_measure_text(m->unit, 16, 0);
        int sx = DIAL_CENTER_X - (hw + uw + 6) / 2;
        dial_draw_text(fb, m->hero, sx,          172, hero_px, pal->text_1, 255, false);
        dial_draw_text(fb, m->unit, sx + hw + 6, 186, 16,      pal->text_2, 255, false);
    } else {
        dial_draw_text(fb, m->hero, DIAL_CENTER_X, m->hero_small ? 180 : 172, hero_px, pal->text_1, 255, true);
    }

    dial_draw_text(fb, m->sub, DIAL_CENTER_X, 214, 12, pal->text_2, 255, true);

    /* Single compact row at y = 246 (Locked Rule #8: MOD-02 uses .train marquee for "Draw power from host") */
    dial_rim_row_t row;
    memset(&row, 0, sizeof(row));
    row.icon_name     = m->row_icon;
    row.title         = m->row_title;
    row.marquee       = m->row_marquee;
    row.has_chev      = m->row_has_chev;
    row.has_toggle    = m->row_has_toggle;
    row.toggle_on     = app->usb_charge;
    row.value_text    = m->row_chip;
    row.value_is_chip = (m->row_chip != NULL);
    row.chip_color    = (m->row_chip_tone == 1) ? pal->fjord : 0;

    dial_draw_rim_list(fb, pal, &row, 1, 0, 242, tick_ms);

    /* Bottom output pill at y = 302 */
    dial_draw_pill(fb, pal, DIAL_CENTER_X, 304, "headphones", m->out_pill, false);
    dial_draw_circle_mask(fb);
}

void dial_render_mod_01(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) { render_mode_template(fb, pal, 0, tick_ms); }
void dial_render_mod_02(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) { render_mode_template(fb, pal, 1, tick_ms); }
void dial_render_mod_03(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) { render_mode_template(fb, pal, 2, tick_ms); }
void dial_render_mod_04(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) { render_mode_template(fb, pal, 3, tick_ms); }
void dial_render_mod_05(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) { render_mode_template(fb, pal, 4, tick_ms); }
void dial_render_mod_06(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) { render_mode_template(fb, pal, 5, tick_ms); }

/* ── 8. SET-01: Settings Hub ── */
void dial_render_set_01(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_app_t *app = dial_app_get_state();
    size_t focus = (size_t)(app->settings_focus_idx >= 0 && app->settings_focus_idx < 5 ? app->settings_focus_idx : 2);

    dial_draw_clear(fb, pal->bg);
    dial_draw_header(fb, pal, "Settings", "", 42);

    const dial_rim_row_t rows[5] = {
        { .icon_name = "headphones", .title = "Audio" },
        { .icon_name = "contrast",   .title = "Display" },
        { .icon_name = "bluetooth",  .title = "Bluetooth", .sub = "On · Sony WH-1000XM5 · LDAC", .has_chev = true },
        { .icon_name = "wifi",       .title = "Wi-Fi" },
        { .icon_name = "power",      .title = "Power" }
    };

    dial_draw_rim_list(fb, pal, rows, 5, focus, 64, tick_ms);
    dial_draw_scroll_rim(fb, pal, 0.25f, 0.28f);
    dial_draw_circle_mask(fb);
}

/* ── 9. SET-02: Audio Settings (High Gain in Cloudberry) ── */
void dial_render_set_02(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_app_t *app = dial_app_get_state();
    dial_draw_clear(fb, pal->bg);
    dial_draw_header(fb, pal, "Audio", "", 42);

    const dial_rim_row_t rows[5] = {
        { .icon_name = "filter", .title = "DAC filter",    .has_chev = true },
        { .icon_name = "gain",   .title = "Output gain",   .sub = "+6 dB analogue relay",
          .value_text = app->high_gain ? "High" : "Low",   .value_is_chip = true,
          .chip_color = app->high_gain ? pal->cloudberry : 0 },
        { .icon_name = "eq",     .title = "Parametric EQ", .value_text = "Acoustic", .value_is_chip = true, .chip_color = pal->moss },
        { .icon_name = "rings",  .title = "Gapless",       .has_toggle = true, .toggle_on = true },
        { .icon_name = "volume", .title = "Volume limit",  .value_text = "85" }
    };

    dial_draw_rim_list(fb, pal, rows, 5, 1, 64, tick_ms);
    dial_draw_scroll_rim(fb, pal, 0.05f, 0.35f);
    dial_draw_circle_mask(fb);
}

/* ── 10. SET-03: CS43131 DAC Filter Picker ── */
void dial_render_set_03(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_app_t *app = dial_app_get_state();
    int sel = (app->dac_filter_idx >= 0 && app->dac_filter_idx < 5) ? app->dac_filter_idx : 0;

    dial_draw_clear(fb, pal->bg);
    dial_draw_header(fb, pal, "DAC filter · CS43131", "", 42);

    dial_rim_row_t rows[5] = {
        { .icon_name = "filter", .title = "Fast roll-off · linear",  .has_check = (sel == 0) },
        { .icon_name = "filter", .title = "Slow roll-off · linear",  .sub = "Softer top end, less pre-ring", .has_check = (sel == 1) },
        { .icon_name = "filter", .title = "Fast · minimum phase",    .has_check = (sel == 2) },
        { .icon_name = "filter", .title = "Slow · minimum phase",    .has_check = (sel == 3) },
        { .icon_name = "filter", .title = "NOS",                     .has_check = (sel == 4) }
    };

    dial_draw_rim_list(fb, pal, rows, 5, 1, 64, tick_ms);
    dial_draw_scroll_rim(fb, pal, 0.10f, 0.30f);
    dial_draw_circle_mask(fb);
}

/* ── 11. SET-04: 10-Band Parametric EQ ── */
void dial_render_set_04(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    const dial_app_t *app = dial_app_get_state();
    int focus_band = (app->eq_focus_band >= 0 && app->eq_focus_band < 10) ? app->eq_focus_band : 2;

    dial_draw_clear(fb, pal->bg);
    dial_draw_header(fb, pal, "Parametric EQ", "", 42);

    /* Segmented preset bar at y = 62 */
    dial_draw_round_rect(fb, 68.0f, 60.0f, 224.0f, 26.0f, 13.0f, pal->surface_1, 255, 1.0f, pal->line, 255);
    dial_draw_round_rect(fb, 70.0f, 62.0f, 62.0f,  22.0f, 11.0f,
                         dial_color_lerp(pal->surface_2, pal->moss, 46), 255, 1.0f, pal->moss, 180);
    dial_draw_text(fb, "Acoustic", 101, 67, 10, pal->moss,   255, true);
    dial_draw_text(fb, "Vocal",    154, 67, 10, pal->text_2, 255, true);
    dial_draw_text(fb, "Flat",     202, 67, 10, pal->text_2, 255, true);
    dial_draw_text(fb, "Custom",   256, 67, 10, pal->text_2, 255, true);

    /* Response curve card (x=52, y=102, w=256, h=76, r=16) */
    dial_draw_round_rect(fb, 52.0f, 102.0f, 256.0f, 76.0f, 16.0f, pal->surface_1, 255, 1.0f, pal->line, 255);
    dial_draw_line_aa(fb, 60.0f, 140.0f, 300.0f, 140.0f, 1.0f, pal->line, 180);
    dial_draw_text(fb, "+6", 60, 108, 10, pal->text_3, 255, false);
    dial_draw_text(fb, "–6", 60, 162, 10, pal->text_3, 255, false);

    static const char  *kBandLabels[10] = { "31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };
    static const float  kBandNorm[10]   = { 0.52f, 0.40f, 0.32f, 0.55f, 0.62f, 0.58f, 0.44f, 0.38f, 0.46f, 0.54f };

    float pts_x[10], pts_y[10];
    for (int i = 0; i < 10; ++i) {
        pts_x[i] = 64.0f + (float)i * 25.8f;
        pts_y[i] = 104.0f + kBandNorm[i] * 70.0f;
    }

    /* Soft 12% Moss fill under EQ response curve */
    for (int s = 0; s < 9; ++s) {
        int x0 = (int)pts_x[s];
        int x1 = (int)pts_x[s + 1];
        for (int x = x0; x <= x1; ++x) {
            float t = (float)(x - x0) / (float)(x1 - x0);
            int y_top = (int)(pts_y[s] + t * (pts_y[s + 1] - pts_y[s]));
            for (int y = y_top; y <= 176; ++y) {
                dial_draw_pixel_aa(fb, x, y, pal->moss, 32);
            }
        }
        dial_draw_line_aa(fb, pts_x[s], pts_y[s], pts_x[s + 1], pts_y[s + 1], 2.0f, pal->moss, 255);
    }

    /* Active node on response curve */
    dial_draw_circle_fill(fb, pts_x[focus_band], pts_y[focus_band], 5.0f, pal->bg, 255);
    dial_draw_circle_stroke(fb, pts_x[focus_band], pts_y[focus_band], 5.0f, 2.0f, pal->moss, 255);

    /* 10 vertical band sliders (y = 196..260) */
    for (int i = 0; i < 10; ++i) {
        float x = pts_x[i];
        float y = 196.0f + kBandNorm[i] * 64.0f;
        bool on = (i == focus_band);
        dial_draw_line_aa(fb, x, 196.0f, x, 260.0f, 3.0f, pal->surface_3, 255);
        if (on) {
            dial_draw_line_aa(fb, x, y, x, 260.0f, 3.0f, pal->moss, 255);
        }
        dial_draw_circle_fill(fb, x, y, on ? 7.0f : 5.0f, on ? pal->moss : pal->text_2, 255);
        dial_draw_circle_stroke(fb, x, y, on ? 7.0f : 5.0f, 2.0f, pal->bg, 255);
        dial_draw_text(fb, kBandLabels[i], (int)x, 268, 10, on ? pal->moss : pal->text_3, 255, true);
    }

    dial_draw_pill(fb, pal, DIAL_CENTER_X, 294, NULL, "125 Hz · +2.5 dB · Q 1.41", false);
    dial_draw_circle_mask(fb);
}

/* ── 12. SET-05: Display Settings (Brightness, Night/Day/Auto, Nature Swatches) ── */
void dial_render_set_05(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    const dial_app_t *app = dial_app_get_state();

    dial_draw_clear(fb, pal->bg);
    dial_draw_header(fb, pal, "Display", "", 42);

    /* Row 0: Brightness slider card (276x64 at y=66) */
    dial_draw_round_rect(fb, 42.0f, 66.0f, 276.0f, 64.0f, 18.0f, pal->surface_2, 255, 1.5f, pal->moss, 255);
    dial_draw_icon(fb, DIAL_ICON_SUN, 58.0f, 77.0f, 16.0f, 1.75f, pal->moss, 255);
    dial_draw_text(fb, "Brightness", 80, 78, 14, pal->text_1, 255, false);

    char br_buf[16];
    snprintf(br_buf, sizeof(br_buf), "%d%%", app->brightness);
    int bw = dial_measure_text(br_buf, 13, 0);
    dial_draw_text(fb, br_buf, 302 - bw, 78, 13, pal->moss, 255, false);

    /* Slider track & thumb */
    dial_draw_round_rect(fb, 58.0f, 106.0f, 244.0f, 6.0f, 3.0f, pal->surface_3, 255, 0.0f, 0, 0);
    float fill_w = 244.0f * ((float)app->brightness / 100.0f);
    dial_draw_round_rect(fb, 58.0f, 106.0f, fill_w, 6.0f, 3.0f, pal->moss, 255, 0.0f, 0, 0);
    dial_draw_circle_fill(fb, 58.0f + fill_w, 109.0f, 7.0f, pal->text_1, 255);
    dial_draw_circle_stroke(fb, 58.0f + fill_w, 109.0f, 7.0f, 2.0f, pal->surface_2, 255);

    /* Row 1: Mode (Night / Day / Auto) at y = 138 */
    dial_draw_round_rect(fb, 46.0f, 138.0f, 268.0f, 44.0f, 15.0f, pal->surface_1, 255, 1.0f, pal->line, 255);
    dial_draw_icon(fb, DIAL_ICON_CONTRAST, 62.0f, 152.0f, 16.0f, 1.75f, pal->text_2, 255);
    dial_draw_text(fb, "Mode", 88, 153, 14, pal->text_1, 255, false);
    bool is_night = (app->mode == DIAL_MODE_NIGHT);
    dial_draw_chip(fb, pal, 178, 150, "Night", is_night ? pal->moss : 0, NULL);
    dial_draw_chip(fb, pal, 226, 150, "Day",   !is_night ? pal->moss : 0, NULL);
    dial_draw_chip(fb, pal, 266, 150, "Auto",  0, NULL);

    /* Row 2: Accent swatches (moss, fjord, cloudberry, birch) at y = 190 */
    dial_draw_round_rect(fb, 52.0f, 190.0f, 256.0f, 44.0f, 15.0f, pal->surface_1, 255, 1.0f, pal->line, 255);
    dial_draw_icon(fb, DIAL_ICON_LEAF, 68.0f, 204.0f, 16.0f, 1.75f, pal->text_2, 255);
    dial_draw_text(fb, "Accent", 94, 205, 14, pal->text_1, 255, false);
    const uint32_t swatches[4] = { pal->moss, pal->fjord, pal->cloudberry, pal->birch };
    for (int i = 0; i < 4; ++i) {
        float sx = 212.0f + (float)i * 24.0f;
        dial_draw_circle_fill(fb, sx, 212.0f, 9.0f, swatches[i], 255);
        if (i == 0) {
            dial_draw_circle_stroke(fb, sx, 212.0f, 11.5f, 1.5f, pal->text_1, 255);
        }
    }

    /* Row 3: Spinning disc toggle at y = 242 */
    dial_draw_round_rect(fb, 62.0f, 242.0f, 236.0f, 44.0f, 15.0f, pal->surface_1, 235, 1.0f, pal->line, 235);
    dial_draw_icon(fb, DIAL_ICON_RINGS, 78.0f, 256.0f, 16.0f, 1.75f, pal->text_2, 235);
    dial_draw_text(fb, "Spinning disc", 104, 257, 14, pal->text_1, 235, false);
    dial_draw_toggle(fb, pal, 248, 254, true);

    /* Row 4: Timeout at y = 294 */
    dial_draw_round_rect(fb, 80.0f, 294.0f, 200.0f, 44.0f, 15.0f, pal->surface_1, 115, 1.0f, pal->line, 115);
    dial_draw_icon(fb, DIAL_ICON_TIMER, 96.0f, 308.0f, 16.0f, 1.75f, pal->text_2, 115);
    dial_draw_text(fb, "Timeout", 122, 309, 14, pal->text_1, 115, false);
    dial_draw_text(fb, "30 s",    244, 310, 12, pal->text_2, 115, false);

    dial_draw_scroll_rim(fb, pal, 0.15f, 0.30f);
    dial_draw_circle_mask(fb);
}

/* ── 13. SET-06: Wi-Fi Settings ── */
void dial_render_set_06(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_app_t *app = dial_app_get_state();
    dial_draw_clear(fb, pal->bg);
    dial_draw_header(fb, pal, "Wi-Fi", "", 42);

    const dial_rim_row_t rows[5] = {
        { .icon_name = "wifi", .title = "Wi-Fi",        .has_toggle = true, .toggle_on = app->wifi_on },
        { .icon_name = "wifi", .title = "Studio 5G",    .sub = "Connected · strong", .has_check = true },
        { .icon_name = "lock", .title = "Fjällstuga",   .value_text = "Fair", .value_is_chip = true },
        { .icon_name = "lock", .title = "Kontor-Guest" },
        { .icon_name = "plus", .title = "Other network" }
    };

    dial_draw_rim_list(fb, pal, rows, 5, 1, 64, tick_ms);
    dial_draw_scroll_rim(fb, pal, 0.00f, 0.40f);
    dial_draw_circle_mask(fb);
}

/* ── 14. SET-07: Fast-Cache Bluetooth & Codec Ladder (Locked Rule #8) ── */
void dial_render_set_07(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_app_t *app = dial_app_get_state();
    dial_draw_clear(fb, pal->bg);
    dial_draw_header(fb, pal, "Bluetooth", "", 42);

    const dial_rim_row_t rows[5] = {
        { .icon_name = "bluetooth",  .title = "Bluetooth",     .has_toggle = true, .toggle_on = app->bt_on },
        { .icon_name = "headphones", .title = "WH-1000XM5",    .has_check  = true },
        { .icon_name = "headphones", .title = "Sennheiser M4", .value_text = "aptX · Vol 72", .value_is_chip = true },
        { .icon_name = "headphones", .title = "AirPods Pro",   .value_text = "AAC · Vol 66",  .value_is_chip = true },
        { .icon_name = "plus",       .title = "Scan new device" }
    };

    dial_draw_rim_list(fb, pal, rows, 5, 1, 64, tick_ms);

    /* Render the 3 inline Fast-Cache chips on focused row 1 (WH-1000XM5):
     * [LDAC 990 (moss)] [Vol 78 (neutral)] [Default (fjord)] */
    int cx = 86;
    int cy = 140;
    int w_ldac = 0, w_vol = 0;
    dial_draw_chip(fb, pal, cx, cy, "LDAC 990", pal->moss, &w_ldac);
    cx += w_ldac + 4;
    dial_draw_chip(fb, pal, cx, cy, "Vol 78",   0,         &w_vol);
    cx += w_vol + 4;
    dial_draw_chip(fb, pal, cx, cy, "Default",  pal->fjord, NULL);

    dial_draw_circle_mask(fb);
}

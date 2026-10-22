#include "dial_screens.h"
#include "dial_app.h"
#include "dial_media.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ── Shared App Pebble Registry (matching APPS in fjall-design-system.html) ── */
typedef struct {
    const char *id;
    const char *name;
    const char *icon;
    const char *sub;
    int         tint_role; /* 0=moss, 1=fjord, 2=birch, 3=heather, 4=text_2, 5=cloudberry */
} app_pebble_spec_t;

static const app_pebble_spec_t G_APPS[6] = {
    { "np",    "Now Playing", "rings",  "Teardrop · Massive Attack",   0 },
    { "lib",   "Library",     "note",   "1,428 tracks · 118 artists",  1 },
    { "files", "Files",       "folder", "microSD · 482 GB free",       2 },
    { "modes", "Modes",       "modes",  "Player · USB DAC · BT RX",    3 },
    { "set",   "Settings",    "tune",   "Audio · Display · Power",     4 },
    { "games", "Arcade",      "games",  "3 mini-games",                5 }
};

static const char *G_MODE_ICONS[6] = {
    "mode_player",
    "mode_usbdac",
    "mode_transport",
    "mode_btrx",
    "mode_airplay",
    "mode_transfer"
};

static uint32_t resolve_app_tint(const dial_palette_t *pal, int tint_role) {
    switch (tint_role) {
        case 0: return pal->moss;
        case 1: return pal->fjord;
        case 2: return pal->birch;
        case 3: return pal->heather;
        case 4: return pal->text_2;
        case 5: return pal->cloudberry;
        default: return pal->moss;
    }
}

static void draw_app_pebble(dial_fb_t *fb,
                            const dial_palette_t *pal,
                            const app_pebble_spec_t *app,
                            float cx,
                            float cy,
                            float diameter,
                            bool focus_ring,
                            uint8_t alpha) {
    const float r = diameter * 0.5f;
    const uint32_t tint = resolve_app_tint(pal, app->tint_role);
    const uint32_t fill = dial_color_lerp(pal->surface_1, tint, 48);
    const uint32_t rim  = dial_color_lerp(pal->surface_1, tint, 96);

    dial_draw_circle_fill(fb, cx, cy, r, fill, alpha);
    dial_draw_circle_stroke(fb, cx, cy, r - 0.5f, 1.2f, rim, alpha);

    if (focus_ring) {
        dial_draw_circle_stroke(fb, cx, cy, r + 4.5f, 1.8f, pal->moss, alpha);
    }

    float ic_sz = floorf(diameter * 0.42f);
    dial_draw_icon_by_name(fb, app->icon, cx - ic_sz * 0.5f, cy - ic_sz * 0.5f,
                           ic_sz, 1.75f, tint, alpha);
}

/* Shared Now Playing Full-Bleed Base (NP-01..NP-05) */
static void draw_np_base(dial_fb_t *fb,
                         const dial_palette_t *pal,
                         bool dim,
                         bool show_volume_arc,
                         uint32_t tick_ms) {
    const dial_app_t *app = dial_app_get_state();
    float spin_deg = (!dim && app->is_playing) ? (float)(tick_ms % 1800) * 0.2f : 0.0f;
    dial_draw_art_tex(fb, pal, dial_media_get_now_cover_rgb565(),
                      DIAL_ART_FJORD, DIAL_CENTER_X, DIAL_CENTER_Y, 360, true, spin_deg);

    if (!dim) {
        /* Locked Rule #2: Inner vinyl circle progress & seek ring (r = 38px, 0°=0% -> 359°=99%) */
        dial_draw_vinyl_seek_ring(fb, pal, app->seek_progress);
    }

    if (show_volume_arc) {
        float vol_01 = (float)app->volume / 100.0f;
        dial_draw_rim_arc(fb, pal, 145.0f, 35.0f, vol_01, pal->moss,
                          (float)DIAL_RIM_ARC_R, 4.0f, true, false, true);
        if (!dim) {
            char vol_buf[16];
            snprintf(vol_buf, sizeof(vol_buf), "%d", app->volume);
            dial_draw_text(fb, vol_buf, 316, 172, 11, pal->moss, 255, false);
        }
    }

    dial_draw_top_grab(fb, pal);

    if (!dim) {
        /* Bottom handle: only the '^' (up) chevron at bottom: 16px (zero text label) */
        dial_draw_icon(fb, DIAL_ICON_UP, 170.0f, 324.0f, 20.0f, 2.2f, 0xECEEE8u, 216);
    }
}

/* ── 1. SYS-00: Boot Breathing Mark ── */
static void render_sys_00(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    dial_draw_clear(fb, pal->bg);
    dial_draw_horizon(fb, pal);
    dial_draw_icon(fb, DIAL_ICON_FJALL, 144.0f, 122.0f, 72.0f, 1.4f, pal->moss, 255);
    dial_draw_text(fb, "DIALOS", DIAL_CENTER_X, 212, 12, pal->text_3, 255, true);
    dial_draw_circle_mask(fb);
}

/* ── 2. LCK-01: Lock · Idle Mountain Horizon Clock ── */
static void render_lck_01(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    dial_draw_clear(fb, pal->bg);
    dial_draw_horizon(fb, pal);

    dial_draw_icon(fb, DIAL_ICON_BATTERY, 152.0f, 34.0f, 16.0f, 1.75f, pal->text_2, 240);
    dial_draw_text(fb, "82%", 174, 36, 12, pal->text_2, 240, false);

    dial_draw_text(fb, "TUESDAY · 22 OCT", DIAL_CENTER_X, 86, 11, pal->text_3, 255, true);
    dial_draw_text(fb, "21:45", DIAL_CENTER_X, 112, 52, pal->text_1, 255, true);
    dial_draw_text(fb, "Nothing playing", DIAL_CENTER_X, 184, 13, pal->text_2, 255, true);

    dial_draw_hint(fb, pal, "up", "Swipe up to unlock", 0);
    dial_draw_circle_mask(fb);
}

/* ── 3. LCK-02: Lock · Playing (Full-Bleed Spinning Cover) ── */
static void render_lck_02(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_app_t *app = dial_app_get_state();
    float spin_deg = app->is_playing ? (float)(tick_ms % 1800) * 0.2f : 0.0f;

    dial_draw_clear(fb, pal->bg);
    dial_draw_art_tex(fb, pal, dial_media_get_now_cover_rgb565(),
                      DIAL_ART_FJORD, DIAL_CENTER_X, DIAL_CENTER_Y, 360, true, spin_deg);

    for (int y = 180; y < DIAL_SCREEN_HEIGHT; ++y) {
        uint8_t a = (uint8_t)(((y - 180) * 210) / 180);
        for (int x = 0; x < DIAL_SCREEN_WIDTH; ++x) {
            dial_draw_pixel_aa(fb, x, y, 0x121615u, a);
        }
    }

    dial_draw_rim_arc(fb, pal, -140.0f, 140.0f, app->seek_progress, pal->moss, 168.0f, 3.5f, true, false, true);
    dial_draw_status_bar(fb, pal, "21:45", 82, 0xECEEE8u);

    dial_draw_text(fb, dial_media_get_now_title(),  DIAL_CENTER_X, 212, 17, 0xECEEE8u, 255, true);
    dial_draw_text(fb, dial_media_get_now_artist(), DIAL_CENTER_X, 234, 13, 0xECEEE8u, 200, true);

    dial_draw_icon(fb, DIAL_ICON_PREV, 112.0f, 275.0f, 22.0f, 1.75f, 0xECEEE8u, 220);
    dial_draw_circle_fill(fb, 180.0f, 286.0f, 24.0f, pal->moss, 255);
    dial_draw_icon(fb, app->is_playing ? DIAL_ICON_PAUSE : DIAL_ICON_PLAY,
                   170.0f, 276.0f, 20.0f, 1.75f, pal->on_accent, 255);
    dial_draw_icon(fb, DIAL_ICON_NEXT, 226.0f, 275.0f, 22.0f, 1.75f, 0xECEEE8u, 220);

    dial_draw_hint(fb, pal, "up", "Swipe up to unlock", 0xECEEE8u);
    dial_draw_circle_mask(fb);
}

/* ── Shared Home Pebble Carousel Renderer (HOM-01 & HOM-02) ── */
static void render_home_carousel(dial_fb_t *fb, const dial_palette_t *pal, int focus_idx) {
    dial_draw_clear(fb, pal->bg);
    dial_draw_status_bar(fb, pal, "21:45", 82, 0);

    const app_pebble_spec_t *focus_app = &G_APPS[focus_idx % 6];
    const app_pebble_spec_t *prev1     = &G_APPS[(focus_idx + 5) % 6];
    const app_pebble_spec_t *next1     = &G_APPS[(focus_idx + 1) % 6];
    const app_pebble_spec_t *prev2     = &G_APPS[(focus_idx + 4) % 6];
    const app_pebble_spec_t *next2     = &G_APPS[(focus_idx + 2) % 6];

    float x = 0.0f, y = 0.0f;
    dial_polar_to_xy(-98.0f, 140.0f, &x, &y);
    draw_app_pebble(fb, pal, prev2, x, y - 40.0f, 32.0f, false, 115);

    dial_polar_to_xy(98.0f, 140.0f, &x, &y);
    draw_app_pebble(fb, pal, next2, x, y - 40.0f, 32.0f, false, 115);

    dial_polar_to_xy(-62.0f, 136.0f, &x, &y);
    draw_app_pebble(fb, pal, prev1, x, y - 40.0f, 44.0f, false, 230);

    dial_polar_to_xy(62.0f, 136.0f, &x, &y);
    draw_app_pebble(fb, pal, next1, x, y - 40.0f, 44.0f, false, 230);

    draw_app_pebble(fb, pal, focus_app, 180.0f, 118.0f, 104.0f, true, 255);

    dial_draw_text(fb, focus_app->name, DIAL_CENTER_X, 192, 20, pal->text_1, 255, true);
    dial_draw_text(fb, focus_app->sub,  DIAL_CENTER_X, 218, 12, pal->text_2, 255, true);

    if (focus_idx == 0) {
        dial_draw_pill(fb, pal, 136, 248, "pause", "Pause", true);
        dial_draw_pill(fb, pal, 224, 248, "next",  "Next",  false);
    } else if (focus_idx == 1) {
        dial_draw_pill(fb, pal, 102, 248, NULL, "Artists", false);
        dial_draw_pill(fb, pal, 180, 248, NULL, "Albums",  true);
        dial_draw_pill(fb, pal, 254, 248, NULL, "Songs",   false);
    } else {
        dial_draw_pill(fb, pal, 180, 248, NULL, "Open", true);
    }

    int dot_x = 152;
    for (int i = 0; i < 6; ++i) {
        float w = (i == focus_idx) ? 16.0f : 5.0f;
        uint32_t c = (i == focus_idx) ? pal->moss : pal->surface_3;
        dial_draw_round_rect(fb, (float)dot_x, 300.0f, w, 5.0f, 2.5f, c, 255, 0.0f, 0, 0);
        dot_x += (int)w + 5;
    }

    dial_draw_hint(fb, pal, NULL, "Trace rim · tap to open", 0);
    dial_draw_circle_mask(fb);
}

/* ── 4. HOM-01: Home · Carousel ── */
static void render_hom_01(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    const dial_app_t *app = dial_app_get_state();
    render_home_carousel(fb, pal, app->home_focus_idx % 6);
}

/* ── 5. HOM-02: Home · Carousel (Library Focused) ── */
static void render_hom_02(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    render_home_carousel(fb, pal, 1);
}

/* ── 6. HOM-03: Home · 6-App Radial Wheel ── */
static void render_hom_03(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    const dial_app_t *app = dial_app_get_state();
    int focus = app->home_focus_idx % 6;

    dial_draw_clear(fb, pal->bg);

    for (int i = 0; i < 6; ++i) {
        float px = 0.0f, py = 0.0f;
        dial_polar_to_xy((float)(i * 60), 110.0f, &px, &py);
        draw_app_pebble(fb, pal, &G_APPS[i], px, py, 60.0f, (i == focus), 255);
    }

    float center_deg = (float)(focus * 60);
    dial_draw_arc_aa(fb, 180.0f, 180.0f, 170.0f,
                     center_deg - 16.0f, center_deg + 16.0f,
                     4.0f, pal->moss, 255, true, false);

    dial_draw_text(fb, G_APPS[focus].name, DIAL_CENTER_X, 162, 17, pal->text_1, 255, true);
    dial_draw_text(fb, "TAP TO OPEN", DIAL_CENTER_X, 186, 11, pal->text_3, 255, true);
    dial_draw_circle_mask(fb);
}

/* ── 7. CC-01: Control Center · Icon-Only 3x2 Pucks ── */
static void draw_cc_puck(dial_fb_t *fb,
                         const dial_palette_t *pal,
                         float cx,
                         float cy,
                         const char *icon_name,
                         uint32_t active_tone_rgb) {
    const float r = (float)DIAL_CC_PUCK_DIAMETER * 0.5f;
    bool lit = (active_tone_rgb != 0);
    uint32_t bg   = lit ? dial_color_lerp(pal->surface_1, active_tone_rgb, 46) : pal->surface_1;
    uint32_t ring = lit ? dial_color_lerp(pal->surface_1, active_tone_rgb, 192) : pal->line;
    uint32_t fg   = lit ? active_tone_rgb : pal->text_3;
    float    rw   = lit ? 1.5f : 1.0f;

    dial_draw_circle_fill(fb, cx, cy, r, bg, 255);
    dial_draw_circle_stroke(fb, cx, cy, r - 0.5f, rw, ring, 255);
    dial_draw_icon_by_name(fb, icon_name, cx - 13.0f, cy - 13.0f, 26.0f, 1.75f, fg, 255);
}

static void render_cc_01(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    const dial_app_t *app = dial_app_get_state();

    dial_draw_clear(fb, pal->bg);
    dial_draw_status_bar(fb, pal, "21:45", 82, 0);

    dial_draw_rim_arc(fb, pal, 325.0f, 215.0f, (float)app->brightness / 100.0f,
                      pal->birch, (float)DIAL_RIM_ARC_R, 4.0f, true, false, true);
    dial_draw_rim_arc(fb, pal, 145.0f, 35.0f,  (float)app->volume / 100.0f,
                      pal->moss,  (float)DIAL_RIM_ARC_R, 4.0f, true, false, true);
    dial_draw_icon(fb, DIAL_ICON_SUN,    22.0f,  171.0f, 18.0f, 1.75f, pal->birch, 255);
    dial_draw_icon(fb, DIAL_ICON_VOLUME, 320.0f, 171.0f, 18.0f, 1.75f, pal->moss,  255);

    const float col_cx[3] = { 93.0f, 180.0f, 267.0f };
    const float row_cy[2] = { 131.0f, 219.0f };

    /* Locked Rule #5: 3x2 icon-only pucks reflecting live toggle states */
    const char *mode_ic = G_MODE_ICONS[app->active_mode_idx % 6];
    const char *gain_ic = app->high_gain ? "gain_high" : "gain_low";

    draw_cc_puck(fb, pal, col_cx[0], row_cy[0], "wifi",       app->wifi_on    ? pal->fjord      : 0);
    draw_cc_puck(fb, pal, col_cx[1], row_cy[0], "bluetooth",  app->bt_on      ? pal->fjord      : 0);
    draw_cc_puck(fb, pal, col_cx[2], row_cy[0], mode_ic,      pal->moss);
    draw_cc_puck(fb, pal, col_cx[0], row_cy[1], gain_ic,      app->high_gain  ? pal->cloudberry : 0);
    draw_cc_puck(fb, pal, col_cx[1], row_cy[1], "usb_charge", app->usb_charge ? pal->moss       : 0);
    draw_cc_puck(fb, pal, col_cx[2], row_cy[1], "lock",       app->touch_locked ? pal->moss     : 0);

    dial_draw_hint(fb, pal, NULL, "Tap · toggle   Hold · menu", 0);
    dial_draw_circle_mask(fb);
}

/* ── 8. CC-02: Touch Lock Shield (600ms Hold Unlock) ── */
static void render_cc_02(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    const dial_app_t *app = dial_app_get_state();
    dial_draw_clear(fb, pal->bg);
    dial_draw_status_bar(fb, pal, "21:45", 82, 0);

    dial_draw_circle_fill(fb, 180.0f, 152.0f, 52.0f, pal->surface_1, 255);
    dial_draw_circle_stroke(fb, 180.0f, 152.0f, 52.0f, 3.0f, pal->surface_3, 255);
    float arc_deg = 360.0f * app->unlock_hold_progress;
    if (arc_deg > 1.0f) {
        dial_draw_arc_aa(fb, 180.0f, 152.0f, 52.0f, 0.0f, arc_deg, 3.0f, pal->moss, 255, true, false);
    }
    dial_draw_icon(fb, DIAL_ICON_LOCK, 160.0f, 132.0f, 40.0f, 1.75f, pal->moss, 255);

    dial_draw_text(fb, "Screen locked", DIAL_CENTER_X, 236, 17, pal->text_1, 255, true);
    dial_draw_text(fb, "Hold the centre to unlock", DIAL_CENTER_X, 260, 12, pal->text_2, 255, true);
    dial_draw_circle_mask(fb);
}

/* ── 9. NP-01: Now Playing · Pure Vinyl & Inner Circle Seek Ring ── */
static void render_np_01(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    dial_draw_clear(fb, pal->bg);
    draw_np_base(fb, pal, false, true, tick_ms);
    dial_draw_circle_mask(fb);
}

/* ── 10. NP-02: Quick Actions · Floating Metadata & Bottom Arc ── */
static void render_np_02(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_app_t *app = dial_app_get_state();
    dial_draw_clear(fb, pal->bg);
    draw_np_base(fb, pal, true, true, tick_ms);
    dial_draw_scrim(fb, pal, DIAL_SCRIM_ALPHA);

    dial_draw_header(fb, pal, "Quick actions", "", 46);

    dial_draw_text(fb, dial_media_get_now_title(),  DIAL_CENTER_X, 108, 22, pal->text_1, 255, true);
    dial_draw_text(fb, dial_media_get_now_artist(), DIAL_CENTER_X, 138, 13, pal->moss,   255, true);
    dial_draw_text(fb, dial_media_get_now_album(),  DIAL_CENTER_X, 158, 12, pal->text_2, 255, true);

    const struct {
        float       deg;
        const char *icon;
        bool        active;
    } qa_pucks[4] = {
        { 234.0f, "heart",    app->is_favorited },
        { 198.0f, "shuffle",  app->is_shuffled  },
        { 162.0f, "playlist", false             },
        { 126.0f, "lyrics",   false             }
    };

    const float puck_r = (float)DIAL_QA_PUCK_DIAMETER * 0.5f;
    for (int i = 0; i < 4; ++i) {
        float px = 0.0f, py = 0.0f;
        dial_polar_to_xy(qa_pucks[i].deg, (float)DIAL_QA_ARC_RADIUS, &px, &py);
        bool on = qa_pucks[i].active;
        uint32_t fill = on ? dial_color_lerp(pal->surface_2, pal->moss, 52) : pal->surface_2;
        uint32_t brd  = on ? pal->moss : pal->line;
        uint32_t fg   = on ? pal->moss : pal->text_1;
        dial_draw_circle_fill(fb, px, py, puck_r, fill, 255);
        dial_draw_circle_stroke(fb, px, py, puck_r - 0.5f, on ? 1.5f : 1.0f, brd, 255);
        dial_draw_icon_by_name(fb, qa_pucks[i].icon, px - 11.0f, py - 11.0f, 22.0f, 1.75f, fg, 255);
    }

    dial_draw_circle_mask(fb);
}

/* ── 11. NP-03: Synced Lyrics (.lrc) ── */
static void render_np_03(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    const dial_app_t *app = dial_app_get_state();
    dial_draw_clear(fb, pal->bg);
    dial_draw_art(fb, pal, DIAL_ART_FJORD, DIAL_CENTER_X, DIAL_CENTER_Y, 360, true, 0.0f);
    dial_draw_scrim(fb, pal, DIAL_SCRIM_ALPHA);
    dial_draw_rim_arc(fb, pal, 145.0f, 35.0f, (float)app->volume / 100.0f,
                      pal->moss, (float)DIAL_RIM_ARC_R, 4.0f, true, false, true);

    dial_draw_art(fb, pal, DIAL_ART_FJORD, 118, 55, 34, false, 0.0f);
    dial_draw_text(fb, "Teardrop",       144, 42, 13, pal->text_1, 255, false);
    dial_draw_text(fb, "Massive Attack", 144, 58, 11, pal->text_2, 255, false);

    dial_draw_text(fb, "Love, love is a verb",       DIAL_CENTER_X, 108, 13, pal->text_3, 255, true);
    dial_draw_text(fb, "Love is a doing word",       DIAL_CENTER_X, 136, 13, pal->text_2, 255, true);

    const char *sung   = "Fearless on my";
    const char *unsung = " breath";
    int w_sung   = dial_measure_text(sung,   16, 0);
    int w_unsung = dial_measure_text(unsung, 16, 0);
    int start_x  = DIAL_CENTER_X - (w_sung + w_unsung) / 2;
    dial_draw_text(fb, sung,   start_x,          168, 16, pal->moss,   255, false);
    dial_draw_text(fb, unsung, start_x + w_sung, 168, 16, pal->text_1, 255, false);

    dial_draw_text(fb, "Gentle impulsion",           DIAL_CENTER_X, 206, 13, pal->text_2, 255, true);
    dial_draw_text(fb, "Shakes me, makes me lighter",DIAL_CENTER_X, 234, 13, pal->text_3, 255, true);

    dial_draw_hint(fb, pal, NULL, "Synced · .lrc sidecar", 0);
    dial_draw_circle_mask(fb);
}

/* ── 12. NP-04: Read-Only Track Details Sheet ── */
static void render_np_04(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    dial_draw_clear(fb, pal->bg);
    draw_np_base(fb, pal, true, true, tick_ms);
    dial_draw_scrim(fb, pal, DIAL_SCRIM_ALPHA);

    dial_draw_round_rect(fb, 34.0f, 58.0f, 292.0f, 286.0f, 28.0f,
                         pal->surface_1, 255, 1.0f, pal->line, 255);
    dial_draw_round_rect(fb, 166.0f, 68.0f, 28.0f, 4.0f, 2.0f,
                         pal->surface_3, 255, 0.0f, 0, 0);

    dial_draw_text(fb, "TRACK DETAILS",              DIAL_CENTER_X, 84,  11, pal->text_3, 255, true);
    dial_draw_text(fb, dial_media_get_now_title(),   DIAL_CENTER_X, 102, 20, pal->text_1, 255, true);
    char art_alb[128];
    snprintf(art_alb, sizeof(art_alb), "%s · %s", dial_media_get_now_artist(), dial_media_get_now_album());
    dial_draw_text(fb, art_alb,                      DIAL_CENTER_X, 126, 12, pal->text_2, 255, true);

    char fmt_chip[32], br_chip[32];
    snprintf(fmt_chip, sizeof(fmt_chip), "FLAC %u/%u", dial_media_get_now_bit_depth(), dial_media_get_now_sample_rate() / 1000u);
    snprintf(br_chip, sizeof(br_chip), "%u kbps", dial_media_get_now_bitrate_kbps());

    int w1 = dial_measure_text(fmt_chip, 10, 0) + 14;
    int w2 = dial_measure_text(br_chip,  10, 0) + 14;
    int w3 = dial_measure_text("DR 11",  10, 0) + 14;
    int cx_start = DIAL_CENTER_X - (w1 + w2 + w3 + 12) / 2;
    dial_draw_chip(fb, pal, cx_start,               146, fmt_chip, pal->moss, NULL);
    dial_draw_chip(fb, pal, cx_start + w1 + 6,      146, br_chip,  0,         NULL);
    dial_draw_chip(fb, pal, cx_start + w1 + w2 + 12,146, "DR 11",  0,         NULL);

    static const char *kv[4][2] = {
        { "Track",      "01 / 10" },
        { "Genre",      "Electronic" },
        { "ReplayGain", "-6.4 dB" },
        { "Size",       "48.2 MB" }
    };
    int ry = 178;
    for (int i = 0; i < 4; ++i) {
        dial_draw_text(fb, kv[i][0], 58, ry, 12, pal->text_2, 255, false);
        int vw = dial_measure_text(kv[i][1], 12, 0);
        dial_draw_text(fb, kv[i][1], 302 - vw, ry, 12, pal->text_1, 255, false);
        dial_draw_line_aa(fb, 58.0f, (float)(ry + 18), 302.0f, (float)(ry + 18), 1.0f, pal->line, 255);
        ry += 24;
    }

    dial_draw_text(fb, dial_media_get_now_filepath(), DIAL_CENTER_X, 278, 9, pal->text_3, 255, true);
    dial_draw_circle_mask(fb);
}

/* ── 13. NP-05: Volume HUD (Right-Side Outer Rim Arc Only, Zero Inner Circle) ── */
static void render_np_05(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_app_t *app = dial_app_get_state();
    dial_draw_clear(fb, pal->bg);
    draw_np_base(fb, pal, true, false, tick_ms);
    dial_draw_scrim(fb, pal, DIAL_SCRIM_ALPHA);

    dial_draw_rim_arc(fb, pal, 145.0f, 35.0f, (float)app->volume / 100.0f,
                      pal->moss, (float)DIAL_RIM_ARC_R, 6.0f, true, false, true);

    char vol_buf[16];
    snprintf(vol_buf, sizeof(vol_buf), "%d", app->volume);
    dial_draw_text(fb, vol_buf,  DIAL_CENTER_X, 132, 44, pal->text_1, 255, true);
    dial_draw_text(fb, "VOLUME", DIAL_CENTER_X, 188, 11, pal->text_3, 255, true);

    dial_draw_icon(fb, DIAL_ICON_HEADPHONES, 126.0f, 233.0f, 14.0f, 1.75f, pal->text_2, 255);
    dial_draw_text(fb, "4.4 mm BAL", 146, 234, 12, pal->text_2, 255, false);

    dial_draw_circle_mask(fb);
}

/* ── Diagnostic Screens ── */
static void render_test_icons(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    dial_draw_clear(fb, pal->bg);
    dial_draw_header(fb, pal, "Fjall 70 Icons", "", 22);
    const int cols = 10;
    for (int i = 0; i < (int)DIAL_ICON_COUNT; ++i) {
        int r = i / cols;
        int c = i % cols;
        float ix = (float)(46 + c * 27);
        float iy = (float)(48 + r * 36);
        uint32_t col = (i % 4 == 0) ? pal->moss :
                       (i % 4 == 1) ? pal->fjord :
                       (i % 4 == 2) ? pal->cloudberry : pal->text_1;
        dial_draw_icon(fb, (dial_icon_id_t)i, ix, iy, 18.0f, 1.75f, col, 255);
    }
    dial_draw_circle_mask(fb);
}

/* ── Master Screen Registry (38 Fjäll Master Screens + 2 Test Screens) ── */
static const dial_screen_desc_t G_SCREENS[] = {
    { "SYS-00", "core",     "Boot",                                   render_sys_00      },
    { "LCK-01", "core",     "Lock · idle",                            render_lck_01      },
    { "LCK-02", "core",     "Lock · playing (Full-Bleed)",            render_lck_02      },
    { "HOM-01", "core",     "Home · carousel",                        render_hom_01      },
    { "HOM-02", "core",     "Home · Library focused",                 render_hom_02      },
    { "HOM-03", "core",     "App wheel",                              render_hom_03      },
    { "CC-01",  "core",     "Control Center · Icon Pucks",            render_cc_01       },
    { "CC-02",  "core",     "Touch lock shield",                      render_cc_02       },
    { "NP-01",  "np",       "Now Playing · Pure Vinyl",               render_np_01       },
    { "NP-02",  "np",       "Quick actions · Bottom Arc",             render_np_02       },
    { "NP-03",  "np",       "Synced lyrics",                          render_np_03       },
    { "NP-04",  "np",       "Track details sheet",                    render_np_04       },
    { "NP-05",  "np",       "Volume HUD",                             render_np_05       },
    { "LIB-01", "lib",      "Library hub",                            dial_render_lib_01 },
    { "LIB-02", "lib",      "Albums · cover arc",                     dial_render_lib_02 },
    { "LIB-03", "lib",      "Track list",                             dial_render_lib_03 },
    { "LIB-04", "lib",      "Artists · letter jump",                  dial_render_lib_04 },
    { "FIL-01", "files",    "Browser",                                dial_render_fil_01 },
    { "FIL-02", "files",    "Action sheet (File & Folder)",           dial_render_fil_02 },
    { "FIL-03", "files",    "Delete confirm (600ms Hold)",            dial_render_fil_03 },
    { "FIL-04", "files",    "Empty state",                            dial_render_fil_04 },
    { "MOD-00", "modes",    "Mode picker",                            dial_render_mod_00 },
    { "MOD-01", "modes",    "Player",                                 dial_render_mod_01 },
    { "MOD-02", "modes",    "USB DAC",                                dial_render_mod_02 },
    { "MOD-03", "modes",    "USB Transport",                          dial_render_mod_03 },
    { "MOD-04", "modes",    "BT Receiver",                            dial_render_mod_04 },
    { "MOD-05", "modes",    "AirPlay",                                dial_render_mod_05 },
    { "MOD-06", "modes",    "File Transfer",                          dial_render_mod_06 },
    { "SET-01", "settings", "Settings",                               dial_render_set_01 },
    { "SET-02", "settings", "Audio",                                  dial_render_set_02 },
    { "SET-03", "settings", "DAC filter picker",                      dial_render_set_03 },
    { "SET-04", "settings", "Parametric EQ",                          dial_render_set_04 },
    { "SET-05", "settings", "Display",                                dial_render_set_05 },
    { "SET-06", "settings", "Wi-Fi",                                  dial_render_set_06 },
    { "SET-07", "settings", "Bluetooth · Fast-cache & Codec Ladder",  dial_render_set_07 },
    { "GAM-01", "games",    "Arcade hub",                             dial_render_gam_01 },
    { "GAM-02", "games",    "Dial Pong",                              dial_render_gam_02 },
    { "GAM-03", "games",    "2048 Wheel",                             dial_render_gam_03 },
    { "GAM-04", "games",    "Boomshine",                              dial_render_gam_04 },
    { "OVL-01", "overlays", "Toast",                                  dial_render_ovl_01 },
    { "OVL-02", "overlays", "Connection toast",                       dial_render_ovl_02 },
    { "OVL-03", "overlays", "Low battery dialog",                     dial_render_ovl_03 },
    { "OVL-04", "overlays", "Charging",                               dial_render_ovl_04 },
    { "TEST-PRIMITIVES", "test", "Test Primitives",                   dial_render_lib_01 },
    { "TEST-ICONS",      "test", "Test 70 Icons",                     render_test_icons  }
};

const dial_screen_desc_t *dial_screen_get_all(size_t *out_count) {
    if (out_count) {
        *out_count = sizeof(G_SCREENS) / sizeof(G_SCREENS[0]);
    }
    return G_SCREENS;
}

const dial_screen_desc_t *dial_screen_find(const char *screen_id) {
    if (!screen_id) return NULL;
    size_t count = 0;
    const dial_screen_desc_t *all = dial_screen_get_all(&count);
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(all[i].id, screen_id) == 0) {
            return &all[i];
        }
    }
    return NULL;
}

bool dial_screen_render_by_id(dial_fb_t *fb,
                              const char *screen_id,
                              dial_mode_t mode,
                              uint32_t tick_ms) {
    if (!fb) return false;
    const dial_palette_t *pal = dial_get_palette(mode);
    const dial_screen_desc_t *desc = dial_screen_find(screen_id);
    if (!desc) {
        render_np_01(fb, pal, tick_ms);
        return false;
    }
    desc->render(fb, pal, tick_ms);
    return true;
}

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "dial_screens.h"
#include "dial_app.h"
#include "dial_polar.h"

/* ════════════════════════════════════════════════════════════════════
   ROTARY ARCADE SCREENS (GAM-01 .. GAM-04)
   ════════════════════════════════════════════════════════════════════ */

/* ── 22. GAM-01: Arcade Hub ── */
void dial_render_gam_01(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    dial_draw_clear(fb, pal->bg);
    dial_draw_header(fb, pal, "Arcade", "", 22);

    /* Row 0 (Focused): Dial Pong (284x52 at y=104) */
    {
        float rx = (float)(DIAL_SCREEN_WIDTH - 284) * 0.5f;
        float ry = 104.0f;
        dial_draw_round_rect(fb, rx, ry, 284.0f, 52.0f, 18.0f,
                             pal->surface_2, 255, 1.5f, pal->moss, 255);
        float well_bg = (float)dial_color_lerp(pal->surface_1, pal->moss, 46);
        (void)well_bg;
        dial_draw_round_rect(fb, rx + 10.0f, ry + 10.0f, 32.0f, 32.0f, 10.0f,
                             dial_color_lerp(pal->surface_1, pal->moss, 46), 255, 0.0f, 0, 0);
        dial_draw_icon_by_name(fb, "rim", rx + 17.5f, ry + 17.5f, 17.0f, 1.75f, pal->moss, 255);
        dial_draw_text(fb, "Dial Pong", (int)(rx + 52.0f), (int)(ry + 9.0f), 15, pal->text_1, 255, false);
        dial_draw_text(fb, "Best 18,450 · Stage 07", (int)(rx + 52.0f), (int)(ry + 29.0f), 11, pal->cloudberry, 255, false);

        /* Filled Moss play puck (32x32) on the right */
        float px = rx + 284.0f - 28.0f;
        float py = ry + 26.0f;
        dial_draw_circle_fill(fb, px, py, 16.0f, pal->moss, 255);
        dial_draw_icon_by_name(fb, "play", px - 7.0f, py - 7.0f, 14.0f, 1.75f, pal->on_accent, 255);
    }

    /* Row 1 (Neighbour): 2048 Wheel (258x44 at y=162) */
    {
        float rx = (float)(DIAL_SCREEN_WIDTH - 258) * 0.5f;
        float ry = 162.0f;
        dial_draw_round_rect(fb, rx, ry, 258.0f, 44.0f, 16.0f,
                             pal->surface_1, 235, 1.0f, pal->line, 235);
        dial_draw_round_rect(fb, rx + 10.0f, ry + 6.0f, 32.0f, 32.0f, 10.0f,
                             pal->surface_2, 235, 0.0f, 0, 0);
        dial_draw_icon_by_name(fb, "apps", rx + 17.5f, ry + 13.5f, 17.0f, 1.75f, pal->text_2, 235);
        dial_draw_text(fb, "2048 Wheel", (int)(rx + 52.0f), (int)(ry + 14.0f), 15, pal->text_1, 235, false);

        const char *sc = "16,384";
        int sw = dial_measure_text(sc, 12, 0);
        dial_draw_text(fb, sc, (int)(rx + 258.0f - 14.0f - sw), (int)(ry + 15.0f), 12, pal->cloudberry, 235, false);
    }

    /* Row 2 (Outer): Boomshine (226x44 at y=212) */
    {
        float rx = (float)(DIAL_SCREEN_WIDTH - 226) * 0.5f;
        float ry = 212.0f;
        dial_draw_round_rect(fb, rx, ry, 226.0f, 44.0f, 16.0f,
                             pal->surface_1, 115, 1.0f, pal->line, 115);
        dial_draw_round_rect(fb, rx + 10.0f, ry + 6.0f, 32.0f, 32.0f, 10.0f,
                             pal->surface_2, 115, 0.0f, 0, 0);
        dial_draw_icon_by_name(fb, "pebble", rx + 17.5f, ry + 13.5f, 17.0f, 1.75f, pal->text_2, 115);
        dial_draw_text(fb, "Boomshine", (int)(rx + 52.0f), (int)(ry + 14.0f), 15, pal->text_1, 115, false);

        const char *sc = "L12";
        int sw = dial_measure_text(sc, 12, 0);
        dial_draw_text(fb, sc, (int)(rx + 226.0f - 14.0f - sw), (int)(ry + 15.0f), 12, pal->cloudberry, 115, false);
    }

    dial_draw_hint(fb, pal, NULL, "Trace rim · tap to play", 0);
    dial_draw_circle_mask(fb);
}

/* ── 23. GAM-02: Dial Pong ── */
void dial_render_gam_02(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    const dial_app_t *app = dial_app_get_state();
    float pad_center = app->pong_paddle_deg;

    dial_draw_clear(fb, pal->bg);

    /* Outer dashed arena ring (r=150) & inner core ring (r=34) */
    for (int a = 0; a < 360; a += 6) {
        dial_draw_arc_aa(fb, 180.0f, 180.0f, 150.0f, (float)a, (float)(a + 2), 1.5f, pal->surface_2, 255, true, false);
    }
    dial_draw_circle_stroke(fb, 180.0f, 180.0f, 34.0f, 1.5f, pal->surface_2, 255);

    /* Moss paddle arc (40° span around pad_center at r=160, w=10) */
    dial_draw_arc_aa(fb, 180.0f, 180.0f, 160.0f,
                     pad_center - 20.0f, pad_center + 20.0f,
                     10.0f, pal->moss, 255, true, true);

    /* Cloudberry dotted trajectory & ball (cx=206, cy=132, r=8) */
    const float traj_pts[7][2] = {
        { 150.0f, 220.0f }, { 159.0f, 208.0f }, { 168.0f, 197.0f },
        { 176.0f, 186.0f }, { 186.0f, 168.0f }, { 196.0f, 150.0f },
        { 206.0f, 132.0f }
    };
    for (int i = 0; i < 6; ++i) {
        dial_draw_circle_fill(fb, traj_pts[i][0], traj_pts[i][1], 1.5f, pal->cloudberry, 178);
    }
    dial_draw_circle_fill(fb, 206.0f, 132.0f, 8.0f, pal->cloudberry, 255);

    /* Stage overline, core score, and combo chip */
    dial_draw_text(fb, "STAGE 07", DIAL_CENTER_X, 54, 11, pal->text_3, 255, true);
    dial_draw_text(fb, "18,450",   DIAL_CENTER_X, 170, 17, pal->text_3, 255, true);
    int combo_w = dial_measure_text("× 3 combo", 11, 0) + 16;
    dial_draw_chip(fb, pal, DIAL_CENTER_X - combo_w / 2, 292, "× 3 combo", pal->cloudberry, NULL);

    dial_draw_circle_mask(fb);
}

/* ── 24. GAM-03: 2048 Wheel ── */
void dial_render_gam_03(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    dial_draw_clear(fb, pal->bg);

    dial_draw_text(fb, "SCORE",  DIAL_CENTER_X, 30, 11, pal->text_3,     255, true);
    dial_draw_text(fb, "16,384", DIAL_CENTER_X, 44, 17, pal->cloudberry, 255, true);

    const int vals[4][4] = {
        {  2,   0,    4,  8 },
        {  0,  16,   32,  4 },
        { 64, 128,  256,  2 },
        {  8, 512, 1024, 16 }
    };

    /* 4x4 grid at left=78, top=78, width=204, height=204, gap=6 -> cell=46.5px */
    const float gx = 78.0f, gy = 78.0f, cell = 46.5f, gap = 6.0f;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            int v = vals[r][c];
            float tx = gx + (float)c * (cell + gap);
            float ty = gy + (float)r * (cell + gap);

            uint32_t bg = pal->surface_1;
            uint32_t fg = pal->text_1;
            if (v == 2)         { bg = pal->surface_2; fg = pal->text_2; }
            else if (v == 4)    { bg = pal->surface_3; fg = pal->text_1; }
            else if (v == 8)    { bg = dial_color_lerp(pal->surface_1, pal->birch, 90); fg = pal->text_1; }
            else if (v == 16)   { bg = pal->birch;     fg = pal->on_accent; }
            else if (v == 32)   { bg = dial_color_lerp(pal->surface_1, pal->moss, 102); fg = pal->text_1; }
            else if (v == 64)   { bg = pal->moss;      fg = pal->on_accent; }
            else if (v == 128)  { bg = pal->fjord;     fg = pal->on_accent; }
            else if (v == 256)  { bg = dial_color_lerp(pal->surface_1, pal->cloudberry, 140); fg = pal->text_1; }
            else if (v == 512)  { bg = pal->cloudberry;fg = pal->on_accent; }
            else if (v == 1024) { bg = pal->heather;   fg = pal->on_accent; }

            dial_draw_round_rect(fb, tx, ty, cell, cell, 12.0f, bg, 255, 0.0f, 0, 0);
            if (v > 0) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%d", v);
                int fsize = (v > 512) ? 13 : 16;
                dial_draw_text(fb, buf, (int)(tx + cell * 0.5f), (int)(ty + 15.0f), fsize, fg, 255, true);
            }
        }
    }

    dial_draw_hint(fb, pal, "rim", "Trace rim to tilt", 0);
    dial_draw_circle_mask(fb);
}

/* ── 25. GAM-04: Boomshine ── */
void dial_render_gam_04(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    dial_draw_clear(fb, pal->bg);

    /* 5 translucent chain-reaction circles */
    const struct {
        float    x, y, r;
        uint32_t col;
    } blooms[5] = {
        { 120.0f, 130.0f, 34.0f, pal->moss       },
        { 214.0f, 112.0f, 22.0f, pal->fjord      },
        { 236.0f, 210.0f, 40.0f, pal->cloudberry },
        { 150.0f, 236.0f, 18.0f, pal->heather    },
        { 190.0f, 176.0f, 12.0f, pal->birch      }
    };
    for (int i = 0; i < 5; ++i) {
        dial_draw_circle_fill(fb,   blooms[i].x, blooms[i].y, blooms[i].r, blooms[i].col, 56);
        dial_draw_circle_stroke(fb, blooms[i].x, blooms[i].y, blooms[i].r, 1.5f, blooms[i].col, 204);
    }

    /* 7 floating dots */
    const float dots[7][2] = {
        {  96.0f, 200.0f }, { 262.0f, 150.0f }, { 180.0f,  92.0f },
        { 130.0f, 284.0f }, { 250.0f, 270.0f }, {  80.0f, 150.0f },
        { 290.0f, 210.0f }
    };
    const uint32_t dot_cols[4] = { pal->moss, pal->fjord, pal->cloudberry, pal->birch };
    for (int i = 0; i < 7; ++i) {
        dial_draw_circle_fill(fb, dots[i][0], dots[i][1], 4.0f, dot_cols[i % 4], 255);
    }

    dial_draw_text(fb, "LEVEL 12 · 18 / 24", DIAL_CENTER_X, 36, 11, pal->text_3, 255, true);
    dial_draw_hint(fb, pal, NULL, "Tap to start a chain", 0);
    dial_draw_circle_mask(fb);
}

/* ════════════════════════════════════════════════════════════════════
   SYSTEM OVERLAYS (OVL-01 .. OVL-04)
   ════════════════════════════════════════════════════════════════════ */

static void draw_toast_pill(dial_fb_t *fb,
                            const dial_palette_t *pal,
                            const char *icon_name,
                            uint32_t icon_col,
                            const char *text) {
    int tw = dial_measure_text(text, 13, 0);
    int w  = tw + 46;
    float x = (float)(DIAL_CENTER_X - w / 2);
    float y = 40.0f;

    dial_draw_round_rect(fb, x, y, (float)w, 34.0f, 17.0f,
                         pal->surface_2, 255, 1.0f, pal->line, 180);
    dial_draw_icon_by_name(fb, icon_name, x + 12.0f, y + 9.5f, 15.0f, 1.75f, icon_col, 255);
    dial_draw_text(fb, text, (int)(x + 33.0f), (int)(y + 9.0f), 13, pal->text_1, 255, false);
}

/* ── 26. OVL-01: Toast ("Added to Liked") over NP-01 ── */
void dial_render_ovl_01(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_screen_desc_t *np01 = dial_screen_find("NP-01");
    if (np01 && np01->render) {
        np01->render(fb, pal, tick_ms);
    }
    draw_toast_pill(fb, pal, "leaf", pal->moss, "Added to Liked");
    dial_draw_circle_mask(fb);
}

/* ── 27. OVL-02: Connection Toast ("XM5 · LDAC 990") over HOM-01 ── */
void dial_render_ovl_02(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_screen_desc_t *hom01 = dial_screen_find("HOM-01");
    if (hom01 && hom01->render) {
        hom01->render(fb, pal, tick_ms);
    }
    draw_toast_pill(fb, pal, "bluetooth", pal->fjord, "XM5 · LDAC 990");
    dial_draw_circle_mask(fb);
}

/* ── 28. OVL-03: Low Battery Dialog ("Battery at 10%") over HOM-01 + Scrim ── */
void dial_render_ovl_03(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_screen_desc_t *hom01 = dial_screen_find("HOM-01");
    if (hom01 && hom01->render) {
        hom01->render(fb, pal, tick_ms);
    }
    dial_draw_scrim(fb, pal, DIAL_SCRIM_ALPHA);

    /* Cloudberry pebble at top=78 */
    uint32_t well_bg = dial_color_lerp(pal->bg, pal->cloudberry, 46);
    dial_draw_circle_fill(fb, 180.0f, 108.0f, 30.0f, well_bg, 255);
    dial_draw_icon_by_name(fb, "battery", 166.0f, 94.0f, 28.0f, 1.75f, pal->cloudberry, 255);

    dial_draw_text(fb, "Battery at 10 %", DIAL_CENTER_X, 154, 17, pal->text_1, 255, true);
    dial_draw_text(fb, "Turn on Low Power to dim the", DIAL_CENTER_X, 180, 12, pal->text_2, 255, true);
    dial_draw_text(fb, "screen and stop Wi-Fi.",       DIAL_CENTER_X, 198, 12, pal->text_2, 255, true);

    /* Ghost "Not now" & Primary Moss "Turn on" buttons */
    dial_draw_round_rect(fb, 80.0f, 234.0f, 92.0f, 40.0f, 20.0f,
                         pal->bg, 0, 1.0f, pal->line, 255);
    dial_draw_text(fb, "Not now", 126, 246, 13, pal->text_1, 255, true);

    dial_draw_round_rect(fb, 182.0f, 234.0f, 98.0f, 40.0f, 20.0f,
                         pal->moss, 255, 0.0f, 0, 0);
    dial_draw_text(fb, "Turn on", 231, 246, 13, pal->on_accent, 255, true);

    dial_draw_circle_mask(fb);
}

/* ── 29. OVL-04: Charging Screen (64% Moss Ring + Mountain Horizon) ── */
void dial_render_ovl_04(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    dial_draw_clear(fb, pal->bg);
    dial_draw_horizon(fb, pal);

    /* Charging ring (cx=180, cy=170, r=92, w=8, 64% fill) */
    dial_draw_circle_stroke(fb, 180.0f, 170.0f, 92.0f, 8.0f, pal->surface_2, 255);
    dial_draw_arc_aa(fb, 180.0f, 170.0f, 92.0f, 0.0f, 360.0f * 0.64f, 8.0f, pal->moss, 255, true, true);

    dial_draw_icon_by_name(fb, "power", 170.0f, 120.0f, 20.0f, 1.75f, pal->moss, 255);
    dial_draw_text(fb, "64%",           DIAL_CENTER_X, 148, 36, pal->text_1, 255, true);
    dial_draw_text(fb, "1 h 10 m to full", DIAL_CENTER_X, 196, 12, pal->text_2, 255, true);

    dial_draw_circle_mask(fb);
}

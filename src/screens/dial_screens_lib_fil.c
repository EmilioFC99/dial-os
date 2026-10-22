#include "dial_screens.h"
#include "dial_app.h"
#include "dial_media.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ── Shared Album Catalog for LIB-02 Cover Arc ── */
typedef struct {
    const char     *title;
    const char     *artist_year;
    const char     *tracks_chip;
    dial_art_kind_t art;
} album_spec_t;

static const album_spec_t G_ALBUMS[6] = {
    { "Mezzanine",       "Massive Attack · 1998", "11 tracks", DIAL_ART_FJORD  },
    { "Tjärn",           "Jakob Ahlbom · 2024",   "9 tracks",  DIAL_ART_MOSS   },
    { "Skärgård",        "Olafur Arnalds · 2022", "10 tracks", DIAL_ART_BERRY  },
    { "Snöfall",         "Hania Rani · 2023",     "12 tracks", DIAL_ART_AURORA },
    { "Blåtimme",        "Nils Frahm · 2021",     "8 tracks",  DIAL_ART_SNOW   },
    { "Homogenic",       "Björk · 1997",          "10 tracks", DIAL_ART_RINGS  }
};

/* ── 1. LIB-01: Music Library Hub (Locked Rule #6: Strict 5-Row Order) ── */
void dial_render_lib_01(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_app_t *app = dial_app_get_state();
    size_t focus = (size_t)(app->list_focus_idx >= 0 && app->list_focus_idx < 5 ? app->list_focus_idx : 2);

    dial_draw_clear(fb, pal->bg);
    dial_draw_header(fb, pal, "Library", "", 42);

    char alb_val[16] = "214";
    if (dial_media_is_loaded()) {
        snprintf(alb_val, sizeof(alb_val), "%zu", dial_media_get_album_count());
    }

    /* Locked Rule #6: Row order is strictly Favorites, Playlists, Artists, Albums, Genres */
    const dial_rim_row_t rows[5] = {
        { .icon_name = "heart",    .title = "Favorites", .value_text = "64" },
        { .icon_name = "playlist", .title = "Playlists", .value_text = "12" },
        { .icon_name = "artist",   .title = "Artists",   .sub = dial_media_is_loaded() ? "Au5 · Discography" : "118 artists · A–Z", .has_chev = true },
        { .icon_name = "album",    .title = "Albums",    .value_text = alb_val },
        { .icon_name = "genre",    .title = "Genres",    .value_text = "24" }
    };

    dial_draw_rim_list(fb, pal, rows, 5, focus, 64, tick_ms);
    dial_draw_scroll_rim(fb, pal, 0.10f + 0.12f * (float)focus, 0.30f);
    dial_draw_circle_mask(fb);
}

/* ── 2. LIB-02: Albums · Cover Arc (132 / 84 / 56 px, Swipe + Rim Scroll) ── */
static void draw_cover_with_opacity(dial_fb_t *fb,
                                    const dial_palette_t *pal,
                                    const uint16_t *custom_tex,
                                    dial_art_kind_t art,
                                    int cx,
                                    int cy,
                                    int diameter,
                                    uint8_t opacity) {
    dial_draw_art_tex(fb, pal, custom_tex, art, cx, cy, diameter, false, 0.0f);
    if (opacity < 255) {
        dial_draw_circle_fill(fb, (float)cx, (float)cy, (float)diameter * 0.5f + 0.5f,
                              pal->bg, (uint8_t)(255 - opacity));
    }
}

void dial_render_lib_02(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    const dial_app_t *app = dial_app_get_state();

    dial_draw_clear(fb, pal->bg);

    if (dial_media_is_loaded() && dial_media_get_album_count() > 0) {
        size_t n = dial_media_get_album_count();
        size_t focus = (size_t)((app->album_focus_idx >= 0) ? app->album_focus_idx : 0) % n;
        size_t p1 = (focus + n - 1) % n;
        size_t n1 = (focus + 1) % n;
        size_t p2 = (focus + n - 2) % n;
        size_t n2 = (focus + 2) % n;

        char hdr[48];
        snprintf(hdr, sizeof(hdr), "Albums · %zu", n);
        dial_draw_header(fb, pal, hdr, "", 46);

        draw_cover_with_opacity(fb, pal, dial_media_get_album_cover_rgb565(p2), (dial_art_kind_t)(p2 % 6), 22,  144, 56, 90);
        draw_cover_with_opacity(fb, pal, dial_media_get_album_cover_rgb565(n2), (dial_art_kind_t)(n2 % 6), 338, 144, 56, 90);
        draw_cover_with_opacity(fb, pal, dial_media_get_album_cover_rgb565(p1), (dial_art_kind_t)(p1 % 6), 70,  138, 84, 191);
        draw_cover_with_opacity(fb, pal, dial_media_get_album_cover_rgb565(n1), (dial_art_kind_t)(n1 % 6), 290, 138, 84, 191);

        dial_draw_circle_fill(fb, 180.0f, 138.0f, 70.0f, pal->bg, 255);
        dial_draw_circle_stroke(fb, 180.0f, 138.0f, 71.0f, 1.5f, pal->moss, 255);
        dial_draw_art_tex(fb, pal, dial_media_get_album_cover_rgb565(focus), (dial_art_kind_t)(focus % 6), 180, 138, 132, false, 0.0f);

        const dial_album_info_t *alb = dial_media_get_album(focus);
        dial_draw_text(fb, alb->name,   DIAL_CENTER_X, 220, 20, pal->text_1, 255, true);
        dial_draw_text(fb, alb->artist, DIAL_CENTER_X, 245, 13, pal->text_2, 255, true);

        char tchip[32];
        snprintf(tchip, sizeof(tchip), "%zu track%s", alb->track_count, alb->track_count == 1 ? "" : "s");
        int chip_w = dial_measure_text(tchip, 10, 0) + 14;
        dial_draw_chip(fb, pal, DIAL_CENTER_X - chip_w / 2, 272, tchip, 0, NULL);

        dial_draw_scroll_rim(fb, pal, (float)focus / (float)n, 0.16f);
        dial_draw_hint(fb, pal, NULL, "Swipe or trace rim · tap to open", 0);
        dial_draw_circle_mask(fb);
        return;
    }

    int focus = (app->album_focus_idx >= 0) ? (app->album_focus_idx % 6) : 0;

    const album_spec_t *cur  = &G_ALBUMS[focus];
    const album_spec_t *prv1 = &G_ALBUMS[(focus + 5) % 6];
    const album_spec_t *nxt1 = &G_ALBUMS[(focus + 1) % 6];
    const album_spec_t *prv2 = &G_ALBUMS[(focus + 4) % 6];
    const album_spec_t *nxt2 = &G_ALBUMS[(focus + 2) % 6];

    dial_draw_header(fb, pal, "Albums · 214", "", 46);

    /* Outer 56px neighbours at 35% opacity (left=-6+28=22, right=310+28=338, cy=144) */
    draw_cover_with_opacity(fb, pal, NULL, prv2->art, 22,  144, 56, 90);
    draw_cover_with_opacity(fb, pal, NULL, nxt2->art, 338, 144, 56, 90);

    /* Inner 84px neighbours at 75% opacity (left=28+42=70, right=248+42=290, cy=138) */
    draw_cover_with_opacity(fb, pal, NULL, prv1->art, 70,  138, 84, 191);
    draw_cover_with_opacity(fb, pal, NULL, nxt1->art, 290, 138, 84, 191);

    /* Center focused 132px cover at (180, 138) with 4px bg gap + 1.5px moss ring */
    dial_draw_circle_fill(fb, 180.0f, 138.0f, 70.0f, pal->bg, 255);
    dial_draw_circle_stroke(fb, 180.0f, 138.0f, 71.0f, 1.5f, pal->moss, 255);
    dial_draw_art(fb, pal, cur->art, 180, 138, 132, false, 0.0f);

    /* Album title, artist · year, and track count chip */
    dial_draw_text(fb, cur->title,       DIAL_CENTER_X, 220, 20, pal->text_1, 255, true);
    dial_draw_text(fb, cur->artist_year, DIAL_CENTER_X, 245, 13, pal->text_2, 255, true);

    int chip_w = dial_measure_text(cur->tracks_chip, 10, 0) + 14;
    dial_draw_chip(fb, pal, DIAL_CENTER_X - chip_w / 2, 272, cur->tracks_chip, 0, NULL);

    dial_draw_scroll_rim(fb, pal, 0.18f + 0.08f * (float)focus, 0.22f);
    dial_draw_hint(fb, pal, NULL, "Swipe or trace rim · tap to open", 0);
    dial_draw_circle_mask(fb);
}

/* ── 3. LIB-03: Track List (Locked Rule #6: Animated Moss EQ + Playing, No Codec Clutter) ── */
void dial_render_lib_03(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_app_t *app = dial_app_get_state();

    dial_draw_clear(fb, pal->bg);

    /* 9 o'clock back chevron button */
    dial_draw_icon(fb, DIAL_ICON_BACK, 12.0f, 170.0f, 20.0f, 1.75f, pal->text_3, 255);

    if (dial_media_is_loaded() && dial_media_get_album_count() > 0) {
        size_t alb_idx = (size_t)((app->album_focus_idx >= 0) ? app->album_focus_idx : 0) % dial_media_get_album_count();
        const dial_album_info_t *alb = dial_media_get_album(alb_idx);
        dial_draw_header(fb, pal, alb->name, "", 42);

        size_t total = alb->track_count;
        size_t vis   = (total < 5) ? total : 5;
        size_t focus_abs = (size_t)((app->track_focus_idx >= 0) ? app->track_focus_idx : 0);
        if (focus_abs >= total && total > 0) focus_abs = total - 1;

        size_t win_start = 0;
        if (total > 5 && focus_abs > 2) {
            win_start = focus_abs - 2;
            if (win_start + 5 > total) win_start = total - 5;
        }

        dial_rim_row_t rows[5];
        char leads[5][8];
        char durs[5][16];
        memset(rows, 0, sizeof(rows));

        for (size_t i = 0; i < vis; ++i) {
            size_t tidx = win_start + i;
            const dial_track_info_t *tr = &alb->tracks[tidx];
            snprintf(leads[i], sizeof(leads[i]), "%02u", (unsigned)(tidx + 1));
            snprintf(durs[i], sizeof(durs[i]), "%u:%02u", tr->duration_sec / 60u, tr->duration_sec % 60u);
            rows[i].lead_text  = leads[i];
            rows[i].title      = tr->title;
            rows[i].value_text = durs[i];
            if (strcmp(tr->filepath, dial_media_get_now_filepath()) == 0) {
                rows[i].lead_color = pal->moss;
                rows[i].playing_eq = true;
            }
        }
        dial_draw_rim_list(fb, pal, rows, vis, focus_abs - win_start, 64, tick_ms);
        dial_draw_scroll_rim(fb, pal, total > 1 ? (float)focus_abs / (float)total : 0.1f, 0.25f);
        dial_draw_circle_mask(fb);
        return;
    }

    size_t focus   = (size_t)(app->track_focus_idx >= 0 && app->track_focus_idx < 5 ? app->track_focus_idx : 2);
    size_t playing = (size_t)(app->playing_track_idx >= 0 && app->playing_track_idx < 5 ? app->playing_track_idx : 2);

    dial_draw_header(fb, pal, "Mezzanine", "", 42);

    dial_rim_row_t rows[5] = {
        { .lead_text = "01", .title = "Angel",          .value_text = "6:18" },
        { .lead_text = "02", .title = "Risingson",      .value_text = "4:58" },
        { .lead_text = "03", .title = "Teardrop",       .value_text = "5:30" },
        { .lead_text = "04", .title = "Inertia Creeps", .value_text = "5:56" },
        { .lead_text = "05", .title = "Exchange",       .value_text = "4:11" }
    };

    rows[playing].lead_color = pal->moss;
    rows[playing].playing_eq = true;

    dial_draw_rim_list(fb, pal, rows, 5, focus, 64, tick_ms);
    dial_draw_scroll_rim(fb, pal, 0.20f + 0.10f * (float)focus, 0.25f);
    dial_draw_circle_mask(fb);
}

/* ── 4. LIB-04: Artists · Fast-Spin Letter Jump Bubble ── */
void dial_render_lib_04(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_app_t *app = dial_app_get_state();
    size_t focus = (size_t)(app->artist_focus_idx >= 0 && app->artist_focus_idx < 5 ? app->artist_focus_idx : 2);

    dial_draw_clear(fb, pal->bg);
    dial_draw_header(fb, pal, "Artists · 118", "", 42);

    const dial_rim_row_t rows[5] = {
        { .icon_name = "artist", .title = "Lamb" },
        { .icon_name = "artist", .title = "Massive Attack" },
        { .icon_name = "artist", .title = "Moderat", .sub = "3 albums · 31 tracks" },
        { .icon_name = "artist", .title = "Mogwai" },
        { .icon_name = "artist", .title = "Morcheeba" }
    };

    dial_draw_rim_list(fb, pal, rows, 5, focus, 64, tick_ms);
    dial_draw_scrim(fb, pal, DIAL_SCRIM_ALPHA);

    /* Center 100x100px letter jump bubble */
    dial_draw_circle_fill(fb, 180.0f, 180.0f, 50.0f, pal->surface_2, 255);
    dial_draw_circle_stroke(fb, 180.0f, 180.0f, 50.0f, 1.5f, pal->moss, 255);

    char letter_str[2] = { app->artist_jump_letter ? app->artist_jump_letter : 'M', '\0' };
    dial_draw_text(fb, letter_str, DIAL_CENTER_X, 158, 44, pal->moss, 255, true);

    dial_draw_scroll_rim(fb, pal, 0.48f, 0.12f);
    dial_draw_circle_mask(fb);
}

/* ── Shared Base Renderer for FIL-01..FIL-03 (/mnt/sdcard/Music/Trip-hop) ── */
static void draw_files_base(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_app_t *app = dial_app_get_state();

    dial_draw_clear(fb, pal->bg);

    if (dial_media_is_loaded() && dial_media_get_fs_entry_count() > 0) {
        dial_draw_header(fb, pal, dial_media_get_current_dir_label(), "", 42);
        size_t total = dial_media_get_fs_entry_count();
        size_t vis   = (total < 5) ? total : 5;
        size_t focus_abs = (size_t)((app->file_focus_idx >= 0) ? app->file_focus_idx : 0);
        if (focus_abs >= total && total > 0) focus_abs = total - 1;

        size_t win_start = 0;
        if (total > 5 && focus_abs > 2) {
            win_start = focus_abs - 2;
            if (win_start + 5 > total) win_start = total - 5;
        }

        dial_rim_row_t rows[5];
        char subs[5][48];
        memset(rows, 0, sizeof(rows));

        for (size_t i = 0; i < vis; ++i) {
            const dial_fs_entry_t *ent = dial_media_get_fs_entry(win_start + i);
            if (!ent) continue;
            if (strcmp(ent->name, "..") == 0) {
                rows[i].icon_name = "back";
                rows[i].title     = "..";
                rows[i].sub       = "Parent folder";
            } else if (ent->is_dir) {
                rows[i].icon_name = "folder";
                rows[i].title     = ent->name;
                rows[i].sub       = "Album folder";
                rows[i].has_chev  = true;
            } else {
                rows[i].icon_name = "fileaudio";
                rows[i].title     = ent->name;
                snprintf(subs[i], sizeof(subs[i]), "%.1f MB · FLAC", (double)ent->size_bytes / (1024.0 * 1024.0));
                rows[i].sub       = subs[i];
            }
        }
        dial_draw_rim_list(fb, pal, rows, vis, focus_abs - win_start, 64, tick_ms);
        return;
    }

    size_t focus = (size_t)(app->file_focus_idx >= 0 && app->file_focus_idx < 5 ? app->file_focus_idx : 2);
    dial_draw_header(fb, pal, "sdcard / Music / Trip-hop", "", 42);

    const dial_rim_row_t rows[5] = {
        { .icon_name = "back",      .title = "..",                     .sub = "Music" },
        { .icon_name = "folder",    .title = "Massive Attack",         .sub = "4 albums",              .has_chev = true },
        { .icon_name = "fileaudio", .title = app->file_deleted ? "04 Inertia Creeps.flac" : "03 Teardrop.flac",
                                    .sub   = "48.2 MB · FLAC 24/96",   .value_text = "5:30",           .value_is_chip = true },
        { .icon_name = "fileaudio", .title = "04 Inertia Creeps.flac", .sub = "52.0 MB" },
        { .icon_name = "folder",    .title = "Portishead",             .sub = "3 albums" }
    };

    dial_draw_rim_list(fb, pal, rows, 5, focus, 64, tick_ms);
}

/* ── 5. FIL-01: Browse Files (/mnt/sdcard/Music/Trip-hop) ── */
void dial_render_fil_01(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_app_t *app = dial_app_get_state();
    size_t focus = (size_t)(app->file_focus_idx >= 0 && app->file_focus_idx < 5 ? app->file_focus_idx : 2);

    draw_files_base(fb, pal, tick_ms);
    dial_draw_scroll_rim(fb, pal, 0.30f + 0.08f * (float)focus, 0.20f);
    dial_draw_circle_mask(fb);
}

/* ── 6. FIL-02: Action Sheet (File Metadata/Tags & Recursive Folder Actions) ── */
void dial_render_fil_02(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    draw_files_base(fb, pal, tick_ms);
    dial_draw_scrim(fb, pal, DIAL_SCRIM_ALPHA);

    /* Bottom action sheet (top = 138px) */
    dial_draw_round_rect(fb, 34.0f, 138.0f, 292.0f, 204.0f, 28.0f,
                         pal->surface_1, 255, 1.0f, pal->line, 255);
    dial_draw_round_rect(fb, 166.0f, 148.0f, 28.0f, 4.0f, 2.0f,
                         pal->surface_3, 255, 0.0f, 0, 0);

    dial_draw_text(fb, "FILE · METADATA & ACTIONS",              DIAL_CENTER_X, 162, 10, pal->text_3, 255, true);
    dial_draw_text(fb, "03 Teardrop.flac",                       DIAL_CENTER_X, 178, 16, pal->text_1, 255, true);
    dial_draw_text(fb, "Edit tags & filename · Folder: recursive move/del", DIAL_CENTER_X, 198, 10, pal->text_2, 255, true);

    /* 3 circular 48px action pucks: Edit tags (cx=106), Move (cx=180), Delete (cx=254) */
    const struct {
        float       cx;
        const char *icon;
        const char *label;
        bool        destructive;
    } actions[3] = {
        { 106.0f, "edit",  "Edit tags", false },
        { 180.0f, "move",  "Move",      false },
        { 254.0f, "trash", "Delete",    true  }
    };

    for (int i = 0; i < 3; ++i) {
        float cx = actions[i].cx;
        float cy = 246.0f;
        bool dng = actions[i].destructive;
        uint32_t fill = dng ? dial_color_lerp(pal->surface_2, pal->lingon, 46) : pal->surface_2;
        uint32_t brd  = dng ? pal->lingon : pal->line;
        uint32_t fg   = dng ? pal->lingon : pal->text_1;
        uint32_t lbl  = dng ? pal->lingon : pal->text_2;

        dial_draw_circle_fill(fb, cx, cy, 24.0f, fill, 255);
        dial_draw_circle_stroke(fb, cx, cy, 23.5f, dng ? 1.5f : 1.0f, brd, 255);
        dial_draw_icon_by_name(fb, actions[i].icon, cx - 10.0f, cy - 10.0f, 20.0f, 1.75f, fg, 255);
        dial_draw_text(fb, actions[i].label, (int)cx, 278, 11, lbl, 255, true);
    }

    dial_draw_circle_mask(fb);
}

/* ── 7. FIL-03: Delete Confirm (Locked Rule #7: 600ms Hold on Lingon Red Button) ── */
void dial_render_fil_03(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    const dial_app_t *app = dial_app_get_state();

    draw_files_base(fb, pal, tick_ms);
    dial_draw_scrim(fb, pal, DIAL_SCRIM_ALPHA);

    /* Lingon-tinted 60px trash pebble at (180, 100) */
    uint32_t p_fill = dial_color_lerp(pal->bg, pal->lingon, 48);
    dial_draw_circle_fill(fb, 180.0f, 100.0f, 30.0f, p_fill, 255);
    dial_draw_circle_stroke(fb, 180.0f, 100.0f, 29.5f, 1.2f, pal->lingon, 160);
    dial_draw_icon(fb, DIAL_ICON_TRASH, 167.0f, 87.0f, 26.0f, 1.75f, pal->lingon, 255);

    dial_draw_text(fb, "Delete this item?", DIAL_CENTER_X, 144, 17, pal->text_1, 255, true);
    dial_draw_text(fb, "“03 Teardrop.flac” (or folder & all contents)", DIAL_CENTER_X, 168, 11, pal->text_2, 255, true);
    dial_draw_text(fb, "is removed from microSD. Can't be undone.",     DIAL_CENTER_X, 184, 11, pal->text_2, 255, true);

    /* Left Button: Cancel (default Moss focus border) */
    dial_draw_round_rect(fb, 56.0f, 218.0f, 96.0f, 36.0f, 18.0f,
                         pal->surface_1, 255, 1.5f, pal->moss, 255);
    dial_draw_text(fb, "Cancel", 104, 229, 12, pal->text_1, 255, true);

    /* Right Button: Hold to delete (Lingon destructive fill & border + 600ms hold progress) */
    uint8_t hold_mix = (uint8_t)(48.0f + app->delete_hold_progress * 140.0f);
    uint32_t dng_fill = dial_color_lerp(pal->surface_1, pal->lingon, hold_mix);
    dial_draw_round_rect(fb, 164.0f, 218.0f, 140.0f, 36.0f, 18.0f,
                         dng_fill, 255, 1.5f, pal->lingon, 255);
    dial_draw_text(fb, "Hold to delete", 234, 229, 12, pal->lingon, 255, true);

    dial_draw_circle_mask(fb);
}

/* ── 8. FIL-04: Empty Folder State (Pine Glyph + File Transfer Shortcut) ── */
void dial_render_fil_04(dial_fb_t *fb, const dial_palette_t *pal, uint32_t tick_ms) {
    (void)tick_ms;
    dial_draw_clear(fb, pal->bg);
    dial_draw_header(fb, pal, "sdcard / Podcasts", "", 46);

    /* 76x76px surface_1 pebble with Pine glyph */
    dial_draw_circle_fill(fb, 180.0f, 142.0f, 38.0f, pal->surface_1, 255);
    dial_draw_circle_stroke(fb, 180.0f, 142.0f, 37.5f, 1.0f, pal->line, 255);
    dial_draw_icon(fb, DIAL_ICON_PINE, 163.0f, 125.0f, 34.0f, 1.5f, pal->text_3, 255);

    dial_draw_text(fb, "Nothing here yet",                    DIAL_CENTER_X, 196, 17, pal->text_1, 255, true);
    dial_draw_text(fb, "Copy audio files to this folder",     DIAL_CENTER_X, 220, 12, pal->text_2, 255, true);
    dial_draw_text(fb, "in File Transfer mode.",              DIAL_CENTER_X, 236, 12, pal->text_2, 255, true);

    dial_draw_pill(fb, pal, DIAL_CENTER_X, 266, "transfer", "File Transfer", true);
    dial_draw_circle_mask(fb);
}

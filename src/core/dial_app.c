/**
 * @file dial_app.c
 * @brief DialOS Core State Machine, Hierarchical Navigation Stack & Phase 1 Views
 *
 * Implements:
 *   - Real Hierarchical Navigation Stack (`nav_stack`) with universal Left-Edge
 *     Swipe-Right (`x < 48px -> dx > +45px`) and 9 o'clock `<` Back chevron.
 *   - Home Launcher (`HOM-01..03`): 6-pebble circular carousel (`Now Playing`,
 *     `Library`, `Files`, `Modes`, `Settings`, `Arcade`) with live dynamic
 *     subtitles driven by real disk/media/hardware state.
 *   - Files (`FIL-01..04`): Real `opendir`/`readdir` directory browser with
 *     fisheye rim scrolling, folder drill-down, Back to parent folder/Home,
 *     tap-to-play real audio files, long-press action sheet (`FIL-02`), and
 *     600ms hold-to-delete confirmation (`FIL-03`).
 *   - Now Playing (`NP-01..05`): Full-bleed spinning vinyl (`NP-01`), real
 *     progress on the `r = 38px` inner seek ring, single-tap `NP-02` Quick
 *     Actions overlay with real track metadata, double-tap Play/Pause,
 *     prev/next folder track skipping, swipe-up `NP-04` Track Details sheet,
 *     and right-rim `NP-05` Volume HUD.
 *   - Top-Rim Pull-Down Control Center (`CC-01..02`).
 */
#include "dial_app.h"
#include "dial_ipc.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    const char *id;
    const char *name;
    const char *icon;
} home_app_def_t;

static const home_app_def_t k_home_apps[DIAL_HOME_APP_COUNT] = {
    { "np",    "Now Playing", "rings"  },
    { "lib",   "Library",     "note"   },
    { "files", "Files",       "folder" },
    { "modes", "Modes",       "modes"  },
    { "set",   "Settings",    "tune"   },
    { "games", "Arcade",      "games"  }
};

static uint32_t get_app_tint(const dial_palette_t *pal, size_t app_idx) {
    switch (app_idx % DIAL_HOME_APP_COUNT) {
        case 0: return pal->moss;
        case 1: return pal->fjord;
        case 2: return pal->birch;
        case 3: return pal->heather;
        case 4: return pal->text_2;
        case 5: return pal->cloudberry;
        default: return pal->moss;
    }
}

static void sync_gesture_fsm_context(dial_app_t *app) {
    if (!app) return;
    uint32_t dur_sec = 0;
    float cur_prog = dial_media_get_progress(NULL, &dur_sec);
    uint32_t dur_ms = (dur_sec > 0) ? (dur_sec * 1000u) : 180000u;

    /* Enable NP-01 vinyl seek ring & 2x-tap disambiguation only when on base NP-01 */
    bool is_np_base = (app->current_screen == DIAL_SCREEN_NOW_PLAYING) &&
                      !app->cc_open &&
                      !app->np_qa_open &&
                      !app->np_lyrics_open &&
                      !app->np_playlist_open &&
                      !app->np_details_open &&
                      dial_media_has_active_track();

    app->gesture_fsm.is_now_playing_screen = is_np_base;
    app->gesture_fsm.track_duration_ms     = dur_ms;
    if (app->gesture_fsm.state != DIAL_GESTURE_VINYL_RING_SEEK &&
        app->gesture_fsm.state != DIAL_GESTURE_TURNTABLE_SCRUB) {
        app->gesture_fsm.current_seek_deg = dial_normalize_deg(cur_prog * 360.0f);
    }
}

void dial_app_init(dial_app_t *app, dial_mode_t initial_mode) {
    if (!app) return;
    memset(app, 0, sizeof(*app));
    app->theme_mode     = initial_mode;
    app->current_screen = DIAL_SCREEN_HOME;
    app->home_focus_idx = 0;
    app->brightness_pct = 75;
    app->last_tick_ms   = dial_hal_ticks_ms();

    dial_gesture_fsm_init(&app->gesture_fsm, false, 180000u);
    sync_gesture_fsm_context(app);
}

bool dial_app_push_screen(dial_app_t *app, dial_screen_id_t target_screen) {
    if (!app) return false;
    if (app->current_screen == target_screen) return true;

    if (app->nav_depth < DIAL_NAV_STACK_MAX) {
        size_t cur_focus = 0;
        if (app->current_screen == DIAL_SCREEN_HOME) {
            cur_focus = app->home_focus_idx;
        } else if (app->current_screen == DIAL_SCREEN_FILES) {
            cur_focus = app->files_focus_idx;
        }
        app->nav_stack[app->nav_depth].screen    = app->current_screen;
        app->nav_stack[app->nav_depth].focus_idx = cur_focus;
        app->nav_depth++;
    }

    app->current_screen            = target_screen;
    app->np_qa_open                = false;
    app->np_lyrics_open            = false;
    app->np_playlist_open          = false;
    app->np_details_open           = false;
    app->files_action_sheet_open   = false;
    app->files_delete_confirm_open = false;

    if (target_screen == DIAL_SCREEN_FILES) {
        size_t cnt = dial_media_get_fs_entry_count();
        if (app->files_focus_idx >= cnt) {
            app->files_focus_idx = 0;
        }
    }

    sync_gesture_fsm_context(app);
    dial_hal_haptic_tick(10);
    return true;
}

bool dial_app_pop_back(dial_app_t *app) {
    if (!app) return false;

    /* 1. Close Control Center if open (unless touch-locked) */
    if (app->cc_open) {
        if (!app->cc_touch_locked) {
            app->cc_open = false;
            sync_gesture_fsm_context(app);
            dial_hal_haptic_tick(10);
            return true;
        }
        return false;
    }

    /* 2. Close active overlays on Now Playing */
    if (app->current_screen == DIAL_SCREEN_NOW_PLAYING) {
        if (app->np_lyrics_open) {
            app->np_lyrics_open = false;
            sync_gesture_fsm_context(app);
            dial_hal_haptic_tick(8);
            return true;
        }
        if (app->np_playlist_open) {
            app->np_playlist_open = false;
            sync_gesture_fsm_context(app);
            dial_hal_haptic_tick(8);
            return true;
        }
        if (app->np_details_open) {
            app->np_details_open = false;
            sync_gesture_fsm_context(app);
            dial_hal_haptic_tick(8);
            return true;
        }
        if (app->np_qa_open) {
            app->np_qa_open = false;
            sync_gesture_fsm_context(app);
            dial_hal_haptic_tick(8);
            return true;
        }
    }

    /* 3. Close active sheets or step up to parent directory in Files */
    if (app->current_screen == DIAL_SCREEN_FILES) {
        if (app->files_delete_confirm_open) {
            app->files_delete_confirm_open = false;
            dial_hal_haptic_tick(8);
            return true;
        }
        if (app->files_action_sheet_open) {
            app->files_action_sheet_open = false;
            dial_hal_haptic_tick(8);
            return true;
        }
        if (!dial_media_is_at_root_dir()) {
            if (dial_media_browse_parent_dir()) {
                if (app->folder_depth > 0) {
                    app->files_focus_idx = app->folder_focus_stack[--app->folder_depth];
                } else {
                    app->files_focus_idx = 0;
                }
                size_t cnt = dial_media_get_fs_entry_count();
                if (app->files_focus_idx >= cnt) {
                    app->files_focus_idx = 0;
                }
                dial_hal_haptic_tick(10);
                return true;
            }
        }
    }

    /* 4. On Home radial wheel mode, exit back to standard carousel */
    if (app->current_screen == DIAL_SCREEN_HOME && app->home_wheel_mode) {
        app->home_wheel_mode = false;
        dial_hal_haptic_tick(8);
        return true;
    }

    /* 5. Pop parent screen from nav_stack */
    if (app->nav_depth > 0) {
        app->nav_depth--;
        dial_nav_frame_t prev = app->nav_stack[app->nav_depth];
        app->current_screen = prev.screen;
        if (prev.screen == DIAL_SCREEN_HOME) {
            app->home_focus_idx = prev.focus_idx % DIAL_HOME_APP_COUNT;
        } else if (prev.screen == DIAL_SCREEN_FILES) {
            size_t cnt = dial_media_get_fs_entry_count();
            app->files_focus_idx = (cnt > 0 && prev.focus_idx < cnt) ? prev.focus_idx : 0;
        }
        sync_gesture_fsm_context(app);
        dial_hal_haptic_tick(12);
        return true;
    }

    return false;
}

/* ── Helper UI Primitives (Fjäll Pebbles, Action Pucks, Back Chevron) ── */
static void draw_app_pebble(dial_fb_t *fb,
                            const dial_palette_t *pal,
                            const char *icon_name,
                            uint32_t tint_rgb,
                            float cx,
                            float cy,
                            float diameter,
                            bool focused_ring,
                            uint8_t alpha) {
    float r = diameter * 0.5f;
    uint32_t fill = dial_color_lerp(pal->surface_1, tint_rgb, 48);
    uint32_t rim  = dial_color_lerp(pal->surface_1, tint_rgb, 96);

    if (focused_ring) {
        dial_draw_circle_stroke(fb, cx, cy, r + 5.0f, 1.8f, pal->moss, alpha);
    }
    dial_draw_circle_fill(fb, cx, cy, r, fill, alpha);
    dial_draw_circle_stroke(fb, cx, cy, r - 0.5f, 1.2f, rim, alpha);

    float ic_sz = floorf(diameter * 0.52f);
    dial_draw_icon_by_name(fb, icon_name, cx - ic_sz * 0.5f, cy - ic_sz * 0.5f,
                           ic_sz, 1.75f, tint_rgb, alpha);
}

static void draw_round_puck(dial_fb_t *fb,
                            const dial_palette_t *pal,
                            const char *icon_name,
                            float cx,
                            float cy,
                            float diameter,
                            bool active,
                            bool primary,
                            bool danger,
                            const char *label) {
    float r = diameter * 0.5f;
    uint32_t bg  = pal->surface_2;
    uint32_t fg  = pal->text_1;
    uint32_t brd = 0;

    if (primary) {
        bg = pal->moss;
        fg = pal->on_accent;
    } else if (danger) {
        bg  = dial_color_lerp(pal->surface_2, pal->lingon, 46);
        fg  = pal->lingon;
        brd = pal->lingon;
    } else if (active) {
        bg  = dial_color_lerp(pal->surface_2, pal->moss, 52);
        fg  = pal->moss;
        brd = pal->moss;
    }

    dial_draw_circle_fill(fb, cx, cy, r, bg, 255);
    if (brd != 0) {
        dial_draw_circle_stroke(fb, cx, cy, r - 0.75f, 1.5f, brd, 255);
    }
    float ic_sz = floorf(diameter * 0.42f);
    dial_draw_icon_by_name(fb, icon_name, cx - ic_sz * 0.5f, cy - ic_sz * 0.5f,
                           ic_sz, 1.75f, fg, 255);

    if (label && label[0]) {
        char upper[32];
        size_t i = 0;
        for (; label[i] && i < sizeof(upper) - 1; ++i) {
            upper[i] = (char)toupper((unsigned char)label[i]);
        }
        upper[i] = '\0';
        uint32_t lbl_col = danger ? pal->lingon : (active ? pal->moss : pal->text_3);
        dial_draw_text(fb, upper, (int)cx, (int)(cy + r + 6.0f), 10, lbl_col, 255, true);
    }
}

static void draw_back_chevron(dial_fb_t *fb, const dial_palette_t *pal, uint32_t override_rgb) {
    uint32_t col = override_rgb ? override_rgb : pal->text_2;
    dial_draw_circle_fill(fb, 24.0f, 180.0f, 15.0f, pal->surface_1, 170);
    dial_draw_icon(fb, DIAL_ICON_BACK, 14.0f, 170.0f, 20.0f, 1.85f, col, 235);
}

static bool hit_test_back_chevron(float x, float y) {
    return (x < 46.0f && y >= 146.0f && y <= 214.0f);
}

static dial_art_kind_t select_art_kind_for_string(const char *str) {
    if (!str || !*str) return DIAL_ART_FJORD;
    uint32_t h = 2166136261u;
    for (const char *p = str; *p; ++p) {
        h = (h ^ (uint8_t)(*p)) * 16777619u;
    }
    return (dial_art_kind_t)(h % 6u);
}

/* ── Dynamic Subtitle Builder for Home Launcher ── */
static void format_home_app_subtitle(size_t app_idx, char *out_buf, size_t cap) {
    switch (app_idx % DIAL_HOME_APP_COUNT) {
        case 0: /* Now Playing */
            if (dial_media_has_active_track()) {
                const char *t = dial_media_get_now_title();
                const char *a = dial_media_get_now_artist();
                if (a && a[0]) {
                    snprintf(out_buf, cap, "%s · %s", t, a);
                } else {
                    snprintf(out_buf, cap, "%s", t);
                }
            } else {
                snprintf(out_buf, cap, "Not Playing");
            }
            break;
        case 1: /* Library */ {
            size_t tracks  = dial_media_get_total_track_count();
            size_t artists = dial_media_get_artist_count();
            if (tracks > 0) {
                snprintf(out_buf, cap, "%zu %s · %zu %s",
                         tracks, (tracks == 1) ? "track" : "tracks",
                         artists, (artists == 1) ? "artist" : "artists");
            } else {
                snprintf(out_buf, cap, "No Music Found");
            }
            break;
        }
        case 2: /* Files */
            dial_sys_get_free_space_str(out_buf, cap);
            break;
        case 3: /* Modes */
            snprintf(out_buf, cap, "Player · USB DAC · BT RX");
            break;
        case 4: /* Settings */
            snprintf(out_buf, cap, "Audio · Display · Power");
            break;
        case 5: /* Arcade */
            snprintf(out_buf, cap, "3 mini-games");
            break;
        default:
            out_buf[0] = '\0';
            break;
    }
}

static dial_screen_id_t home_idx_to_screen(size_t idx) {
    switch (idx % DIAL_HOME_APP_COUNT) {
        case 0: return DIAL_SCREEN_NOW_PLAYING;
        case 1: return DIAL_SCREEN_LIBRARY;
        case 2: return DIAL_SCREEN_FILES;
        case 3: return DIAL_SCREEN_MODES;
        case 4: return DIAL_SCREEN_SETTINGS;
        case 5: return DIAL_SCREEN_ARCADE;
        default: return DIAL_SCREEN_HOME;
    }
}

static void draw_home_pebble(dial_app_t *app,
                             dial_fb_t *fb,
                             const dial_palette_t *pal,
                             size_t app_idx,
                             float cx,
                             float cy,
                             float diameter,
                             bool focused_ring,
                             uint8_t alpha) {
    size_t idx = app_idx % DIAL_HOME_APP_COUNT;
    if (idx == 0 && dial_media_has_active_track()) {
        float r = diameter * 0.5f;
        if (focused_ring) {
            dial_draw_circle_stroke(fb, cx, cy, r + 5.0f, 1.8f, pal->moss, alpha);
        }
        const uint16_t *cover_tex = dial_media_get_now_cover_rgb565();
        dial_art_kind_t fallback_art = select_art_kind_for_string(dial_media_get_now_album());
        dial_draw_art_tex(fb, pal, cover_tex, fallback_art,
                          (int)(cx + 0.5f), (int)(cy + 0.5f), (int)(diameter + 0.5f),
                          true, app->vinyl_spin_deg);
        dial_draw_circle_stroke(fb, cx, cy, r - 0.5f, 1.4f, pal->moss, alpha);
        return;
    }
    draw_app_pebble(fb, pal, k_home_apps[idx].icon, get_app_tint(pal, idx),
                    cx, cy, diameter, focused_ring, alpha);
}

/* ── Screen 1 Renderer: Home Launcher (HOM-01..03) ── */
static void render_home_screen(dial_app_t *app, dial_fb_t *fb, const dial_palette_t *pal) {
    char clock_str[16];
    dial_sys_get_clock_str(clock_str, sizeof(clock_str));
    int batt_pct = dial_sys_get_battery_pct(NULL);

    dial_draw_top_grab(fb, pal);
    dial_draw_status_bar(fb, pal, clock_str, batt_pct, 0);

    size_t focus = app->home_focus_idx % DIAL_HOME_APP_COUNT;

    if (app->home_wheel_mode) {
        /* HOM-03: 6-Pebble Radial App Wheel at 60° intervals on r = 110px */
        for (size_t i = 0; i < DIAL_HOME_APP_COUNT; ++i) {
            float px = 0.0f, py = 0.0f;
            dial_polar_to_xy((float)(i * 60), 110.0f, &px, &py);
            draw_home_pebble(app, fb, pal, i, px, py, 64.0f, (i == focus), 255);
        }
        float a0 = (float)(focus * 60) - 16.0f;
        float a1 = (float)(focus * 60) + 16.0f;
        dial_draw_arc_aa(fb, 180.0f, 180.0f, 170.0f, a0, a1, 4.0f, pal->moss, 255, true, false);
        dial_draw_text(fb, k_home_apps[focus].name, 180, 172, 17, pal->text_1, 255, true);
        return;
    }

    /* HOM-01 / HOM-02: Circular Pebble Carousel */
    size_t prev = (focus + DIAL_HOME_APP_COUNT - 1) % DIAL_HOME_APP_COUNT;
    size_t next = (focus + 1) % DIAL_HOME_APP_COUNT;
    size_t p2   = (focus + DIAL_HOME_APP_COUNT - 2) % DIAL_HOME_APP_COUNT;
    size_t n2   = (focus + 2) % DIAL_HOME_APP_COUNT;

    float x = 0.0f, y = 0.0f;
    /* Outer neighbours (-98°, +98°, r = 142, size = 36, y - 34, alpha = 115) */
    dial_polar_to_xy(-98.0f, 142.0f, &x, &y);
    draw_home_pebble(app, fb, pal, p2, x, y - 34.0f, 36.0f, false, 115);

    dial_polar_to_xy(98.0f, 142.0f, &x, &y);
    draw_home_pebble(app, fb, pal, n2, x, y - 34.0f, 36.0f, false, 115);

    /* Immediate neighbours (-64°, +64°, r = 138, size = 50, y - 34, alpha = 230) */
    dial_polar_to_xy(-64.0f, 138.0f, &x, &y);
    draw_home_pebble(app, fb, pal, prev, x, y - 34.0f, 50.0f, false, 230);

    dial_polar_to_xy(64.0f, 138.0f, &x, &y);
    draw_home_pebble(app, fb, pal, next, x, y - 34.0f, 50.0f, false, 230);

    /* Center Focused Pebble (enlarged size = 132 at (180, 134)) */
    draw_home_pebble(app, fb, pal, focus, 180.0f, 134.0f, 132.0f, true, 255);

    /* Focused App Title & Live Dynamic Subtitle */
    dial_draw_text(fb, k_home_apps[focus].name, 180, 224, 24, pal->text_1, 255, true);

    char sub_buf[128];
    format_home_app_subtitle(focus, sub_buf, sizeof(sub_buf));
    if (dial_measure_text(sub_buf, 13, 0) > 250) {
        dial_draw_text_marquee(fb, sub_buf, 55, 256, 250, 13, pal->text_2, 255, app->last_tick_ms);
    } else {
        dial_draw_text(fb, sub_buf, 180, 256, 13, pal->text_2, 255, true);
    }

    /* 6 Pagination Dots at y = 304 */
    int total_dots_w = 5 * 5 + 16 + 5 * 5;
    int dot_x = 180 - total_dots_w / 2;
    for (size_t i = 0; i < DIAL_HOME_APP_COUNT; ++i) {
        int dw = (i == focus) ? 16 : 5;
        uint32_t dc = (i == focus) ? pal->moss : pal->surface_3;
        dial_draw_round_rect(fb, (float)dot_x, 304.0f, (float)dw, 5.0f, 2.5f, dc, 255, 0.0f, 0, 0);
        dot_x += dw + 5;
    }
}

/* ── Screen 2 Renderer: Files — Real Directory Browser (FIL-01..04) ── */
static void extract_format_name(const char *filename, char *out_fmt, size_t cap) {
    if (!out_fmt || cap == 0) return;
    out_fmt[0] = '\0';
    if (!filename) return;
    const char *dot = strrchr(filename, '.');
    if (!dot || !dot[1]) {
        strncpy(out_fmt, "AUDIO", cap - 1);
        out_fmt[cap - 1] = '\0';
        return;
    }
    const char *ext = dot + 1;
    size_t i = 0;
    for (; ext[i] && i < cap - 1; ++i) {
        out_fmt[i] = (char)toupper((unsigned char)ext[i]);
    }
    out_fmt[i] = '\0';
}

static void format_file_size(uint32_t bytes, const char *filename, char *out_buf, size_t cap) {
    char ext[16];
    extract_format_name(filename, ext, sizeof(ext));
    if (bytes >= 1024u * 1024u) {
        double mb = (double)bytes / (1024.0 * 1024.0);
        snprintf(out_buf, cap, "%.1f MB · %s", mb, ext);
    } else if (bytes >= 1024u) {
        snprintf(out_buf, cap, "%u KB · %s", (unsigned)(bytes / 1024u), ext);
    } else {
        snprintf(out_buf, cap, "%u B · %s", (unsigned)bytes, ext);
    }
}

static void render_files_screen(dial_app_t *app, dial_fb_t *fb, const dial_palette_t *pal) {
    dial_draw_top_grab(fb, pal);
    dial_draw_header(fb, pal, dial_media_get_current_dir_label(), "", 38);
    draw_back_chevron(fb, pal, 0);

    size_t total = dial_media_get_fs_entry_count();
    /* Check if folder is empty (either 0 entries, or 1 entry which is ".." in an empty subfolder) */
    bool is_empty_folder = (total == 0) ||
                           (total == 1 && strcmp(dial_media_get_fs_entry(0)->name, "..") == 0);

    if (is_empty_folder) {
        /* FIL-04: Empty Directory State */
        dial_draw_circle_fill(fb, 180.0f, 134.0f, 38.0f, pal->surface_1, 255);
        dial_draw_icon(fb, DIAL_ICON_PINE, 163.0f, 117.0f, 34.0f, 1.5f, pal->text_3, 255);
        dial_draw_text(fb, "Nothing here yet", 180, 188, 17, pal->text_1, 255, true);
        dial_draw_text(fb, "No audio files in this folder", 180, 212, 12, pal->text_2, 255, true);
        dial_draw_pill(fb, pal, 180, 248, "back", "Go Back", true);
        dial_draw_hint(fb, pal, NULL, "SWIPE RIGHT · GO BACK", 0);
        return;
    }

    if (app->files_focus_idx >= total) {
        app->files_focus_idx = total - 1;
    }

    /* Compute 4-row sliding window centered around files_focus_idx */
    size_t win_count = (total < 4) ? total : 4;
    size_t win_start = 0;
    if (total > 4) {
        if (app->files_focus_idx <= 1) {
            win_start = 0;
        } else if (app->files_focus_idx + 2 >= total) {
            win_start = total - 4;
        } else {
            win_start = app->files_focus_idx - 1;
        }
    }

    dial_rim_row_t rows[4];
    char sub_bufs[4][64];
    char val_bufs[4][24];
    memset(rows, 0, sizeof(rows));

    for (size_t i = 0; i < win_count; ++i) {
        size_t idx = win_start + i;
        const dial_fs_entry_t *ent = dial_media_get_fs_entry(idx);
        if (!ent) continue;

        bool is_focused = (idx == app->files_focus_idx);
        rows[i].title = ent->name;

        if (strcmp(ent->name, "..") == 0) {
            rows[i].icon_name = "back";
            snprintf(sub_bufs[i], sizeof(sub_bufs[i]), "Parent folder");
            rows[i].sub = sub_bufs[i];
        } else if (ent->is_dir) {
            rows[i].icon_name = "folder";
            rows[i].icon_tint = pal->birch;
            if (ent->sub_count == 0) {
                snprintf(sub_bufs[i], sizeof(sub_bufs[i]), "Empty folder");
            } else {
                snprintf(sub_bufs[i], sizeof(sub_bufs[i]), "%u %s",
                         (unsigned)ent->sub_count, (ent->sub_count == 1) ? "item" : "items");
            }
            rows[i].sub      = sub_bufs[i];
            rows[i].has_chev = true;
        } else {
            rows[i].icon_name = "fileaudio";
            format_file_size(ent->size_bytes, ent->name, sub_bufs[i], sizeof(sub_bufs[i]));
            rows[i].sub = sub_bufs[i];
            rows[i].marquee = is_focused && (dial_measure_text(ent->name, 14, 0) > 155);

            /* Check if this exact file is currently playing */
            if (dial_media_has_active_track() &&
                strcmp(ent->full_path, dial_media_get_now_filepath()) == 0) {
                rows[i].playing_eq = dial_media_get_playing();
                rows[i].icon_tint  = pal->moss;
            }

            extract_format_name(ent->name, val_bufs[i], sizeof(val_bufs[i]));
            rows[i].value_text    = val_bufs[i];
            rows[i].value_is_chip = is_focused;
        }
    }

    dial_draw_rim_list(fb, pal, rows, win_count, app->files_focus_idx - win_start, 66, app->last_tick_ms);

    float scroll_len = (total > 4) ? (4.0f / (float)total) : 0.45f;
    if (scroll_len < 0.12f) scroll_len = 0.12f;
    float scroll_pos = (total > 1)
                           ? ((float)app->files_focus_idx / (float)(total - 1)) * (1.0f - scroll_len)
                           : 0.0f;
    dial_draw_scroll_rim(fb, pal, scroll_pos, scroll_len);

    /* FIL-02: Long-Press File/Folder Action Sheet */
    if (app->files_action_sheet_open) {
        const dial_fs_entry_t *ent = dial_media_get_fs_entry(app->files_focus_idx);
        dial_draw_scrim(fb, pal, DIAL_SCRIM_ALPHA);
        dial_draw_round_rect(fb, 20.0f, 138.0f, 320.0f, 230.0f, 30.0f,
                             pal->surface_1, 255, 1.0f, pal->line, 255);
        dial_draw_round_rect(fb, 165.0f, 148.0f, 30.0f, 4.0f, 2.0f,
                             pal->surface_3, 255, 0.0f, 0, 0);

        dial_draw_header(fb, pal, ent && ent->is_dir ? "FOLDER ACTIONS" : "FILE ACTIONS",
                         ent ? ent->name : "", 162);
        draw_round_puck(fb, pal, (ent && ent->is_dir) ? "folder" : "play",
                        136.0f, 254.0f, 52.0f, false, true, false,
                        (ent && ent->is_dir) ? "Open" : "Play");
        draw_round_puck(fb, pal, "trash",
                        224.0f, 254.0f, 52.0f, false, false, true, "Delete");
    }

    /* FIL-03: 600ms Hold-to-Delete Confirmation Dialog */
    if (app->files_delete_confirm_open) {
        const dial_fs_entry_t *ent = dial_media_get_fs_entry(app->files_focus_idx);
        dial_draw_scrim(fb, pal, DIAL_SCRIM_ALPHA);
        draw_app_pebble(fb, pal, "trash", pal->lingon, 180.0f, 100.0f, 60.0f, false, 255);
        dial_draw_text(fb, "Delete this item?", 180, 144, 17, pal->text_1, 255, true);
        dial_draw_text(fb, ent ? ent->name : "", 180, 168, 13, pal->text_2, 255, true);
        dial_draw_text(fb, "Removed from disk permanently.", 180, 188, 11, pal->text_3, 255, true);

        dial_draw_pill(fb, pal, 118, 234, NULL, "Cancel", true);
        dial_draw_round_rect(fb, 172.0f, 234.0f, 124.0f, 30.0f, 15.0f,
                             pal->lingon, 255, 0.0f, 0, 0);
        dial_draw_text(fb, "Hold 600ms", 234, 243, 12, pal->on_accent, 255, true);
    }
}

/* ── Helper: Real Embedded Metadata (FLAC Vorbis / ID3v2) + .lrc/.txt Lyrics Loader ── */
#define DIAL_MAX_LYRIC_LINES 160

typedef struct {
    int32_t time_ms; /* >= 0 if [mm:ss.xx] timestamp present, -1 if unsynced */
    char    text[88];
} dial_lyric_line_t;

static char              g_lyrics_cached_path[512] = "";
static dial_lyric_line_t g_lyrics_lines[DIAL_MAX_LYRIC_LINES];
static size_t            g_lyrics_count          = 0;
static bool              g_lyrics_has_timestamps = false;

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint32_t read_u32_be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static void clean_utf8_lyric_line(const char *src, char *dst, size_t cap) {
    if (!dst || cap == 0) return;
    size_t out = 0;
    const unsigned char *s = (const unsigned char *)src;
    while (*s && out + 1 < cap) {
        /* Normalize UTF-8 curly quotes and dashes to ASCII so font glyphs match */
        if (s[0] == 0xE2 && s[1] == 0x80) {
            if (s[2] == 0x98 || s[2] == 0x99) {
                dst[out++] = '\'';
                s += 3;
                continue;
            }
            if (s[2] == 0x9C || s[2] == 0x9D) {
                dst[out++] = '"';
                s += 3;
                continue;
            }
            if (s[2] == 0x93 || s[2] == 0x94) {
                dst[out++] = '-';
                s += 3;
                continue;
            }
        }
        if (*s >= 32 || *s >= 0x80) {
            dst[out++] = (char)(*s);
        }
        s++;
    }
    while (out > 0 && isspace((unsigned char)dst[out - 1])) {
        out--;
    }
    dst[out] = '\0';
}

static bool extract_embedded_lyrics(const char *audio_path, char *out_buf, size_t out_cap) {
    if (!audio_path || !*audio_path || !out_buf || out_cap == 0) return false;
    out_buf[0] = '\0';

    FILE *fp = fopen(audio_path, "rb");
    if (!fp) return false;

    uint8_t magic[10];
    if (fread(magic, 1, 4, fp) != 4) {
        fclose(fp);
        return false;
    }

    static uint8_t block_buf[65536];

    /* 1. Native FLAC Vorbis Comment Block (block_type == 4) */
    if (magic[0] == 'f' && magic[1] == 'L' && magic[2] == 'a' && magic[3] == 'C') {
        bool is_last = false;
        while (!is_last) {
            uint8_t bhdr[4];
            if (fread(bhdr, 1, 4, fp) != 4) break;
            is_last = (bhdr[0] & 0x80u) != 0;
            uint8_t btype = bhdr[0] & 0x7Fu;
            uint32_t blen = ((uint32_t)bhdr[1] << 16) | ((uint32_t)bhdr[2] << 8) | (uint32_t)bhdr[3];
            if (btype == 127 || blen == 0) break;

            if (btype == 4 && blen >= 8) {
                uint32_t to_read = (blen < sizeof(block_buf)) ? blen : (uint32_t)sizeof(block_buf);
                size_t got = fread(block_buf, 1, to_read, fp);
                if (blen > to_read) {
                    fseek(fp, (long)(blen - to_read), SEEK_CUR);
                }
                if (got >= 8) {
                    uint32_t vendor_len = read_u32_le(block_buf);
                    uint32_t pos = 4u + vendor_len;
                    if (pos + 4u <= (uint32_t)got) {
                        uint32_t n_comments = read_u32_le(block_buf + pos);
                        pos += 4u;
                        for (uint32_t c = 0; c < n_comments && pos + 4u <= (uint32_t)got; ++c) {
                            uint32_t clen = read_u32_le(block_buf + pos);
                            pos += 4u;
                            if (pos + clen > (uint32_t)got) break;
                            const char *cstr = (const char *)(block_buf + pos);
                            size_t prefix_len = 0;
                            if (clen > 7 && strncasecmp(cstr, "LYRICS=", 7) == 0) {
                                prefix_len = 7;
                            } else if (clen > 15 && strncasecmp(cstr, "UNSYNCEDLYRICS=", 15) == 0) {
                                prefix_len = 15;
                            }
                            if (prefix_len > 0) {
                                size_t copy_len = clen - prefix_len;
                                if (copy_len >= out_cap) copy_len = out_cap - 1;
                                memcpy(out_buf, cstr + prefix_len, copy_len);
                                out_buf[copy_len] = '\0';
                                fclose(fp);
                                return (out_buf[0] != '\0');
                            }
                            pos += clen;
                        }
                    }
                }
            } else {
                if (fseek(fp, (long)blen, SEEK_CUR) != 0) break;
            }
        }
        fclose(fp);
        return false;
    }

    /* 2. ID3v2 (MP3) USLT / TXXX Lyrics Frame */
    if (magic[0] == 'I' && magic[1] == 'D' && magic[2] == '3') {
        if (fread(magic + 4, 1, 6, fp) == 6) {
            uint8_t ver = magic[3];
            uint32_t tag_sz = ((uint32_t)(magic[6] & 0x7Fu) << 21) |
                              ((uint32_t)(magic[7] & 0x7Fu) << 14) |
                              ((uint32_t)(magic[8] & 0x7Fu) << 7)  |
                              (uint32_t)(magic[9] & 0x7Fu);
            uint32_t to_read = (tag_sz < sizeof(block_buf)) ? tag_sz : (uint32_t)sizeof(block_buf);
            size_t got = fread(block_buf, 1, to_read, fp);
            uint32_t pos = 0;
            while (pos + 10u <= (uint32_t)got) {
                if (block_buf[pos] == 0) break;
                uint32_t fsz = (ver == 4)
                    ? (((uint32_t)(block_buf[pos + 4] & 0x7Fu) << 21) |
                       ((uint32_t)(block_buf[pos + 5] & 0x7Fu) << 14) |
                       ((uint32_t)(block_buf[pos + 6] & 0x7Fu) << 7)  |
                       (uint32_t)(block_buf[pos + 7] & 0x7Fu))
                    : read_u32_be(block_buf + pos + 4);
                if (fsz == 0 || pos + 10u + fsz > (uint32_t)got) break;
                if (memcmp(block_buf + pos, "USLT", 4) == 0 && fsz > 4) {
                    const uint8_t *payload = block_buf + pos + 10u;
                    uint8_t enc = payload[0];
                    uint32_t skip = 4u; /* 1 byte enc + 3 byte lang */
                    if (enc == 0 || enc == 3) {
                        while (skip < fsz && payload[skip] != 0) skip++;
                        if (skip < fsz) skip++;
                        size_t copy_len = fsz - skip;
                        if (copy_len >= out_cap) copy_len = out_cap - 1;
                        memcpy(out_buf, payload + skip, copy_len);
                        out_buf[copy_len] = '\0';
                        fclose(fp);
                        return (out_buf[0] != '\0');
                    }
                }
                pos += 10u + fsz;
            }
        }
    }

    fclose(fp);
    return false;
}

static void ensure_track_lyrics_loaded(const char *audio_path) {
    if (!audio_path || !*audio_path) {
        g_lyrics_cached_path[0] = '\0';
        g_lyrics_count          = 0;
        g_lyrics_has_timestamps = false;
        return;
    }
    if (strcmp(audio_path, g_lyrics_cached_path) == 0) {
        return;
    }
    strncpy(g_lyrics_cached_path, audio_path, sizeof(g_lyrics_cached_path) - 1);
    g_lyrics_cached_path[sizeof(g_lyrics_cached_path) - 1] = '\0';
    g_lyrics_count          = 0;
    g_lyrics_has_timestamps = false;

    static char raw_lyrics[16384];
    raw_lyrics[0] = '\0';

    /* 1. First try embedded metadata inside the audio file itself (FLAC / ID3v2) */
    bool loaded = extract_embedded_lyrics(audio_path, raw_lyrics, sizeof(raw_lyrics));

    /* 2. Fallback to sidecar .lrc or .txt file if no embedded lyrics tag */
    if (!loaded) {
        char lrc_path[512];
        strncpy(lrc_path, audio_path, sizeof(lrc_path) - 1);
        lrc_path[sizeof(lrc_path) - 1] = '\0';
        char *dot = strrchr(lrc_path, '.');
        if (dot) {
            strcpy(dot, ".lrc");
            FILE *fp = fopen(lrc_path, "r");
            if (!fp) {
                strcpy(dot, ".txt");
                fp = fopen(lrc_path, "r");
            }
            if (fp) {
                size_t n = fread(raw_lyrics, 1, sizeof(raw_lyrics) - 1, fp);
                raw_lyrics[n] = '\0';
                fclose(fp);
                loaded = (n > 0);
            }
        }
    }

    if (!loaded || raw_lyrics[0] == '\0') return;

    /* 3. Parse lines & optional [mm:ss.xx] LRC timestamps */
    char *cursor = raw_lyrics;
    while (*cursor && g_lyrics_count < DIAL_MAX_LYRIC_LINES) {
        char *eol = strchr(cursor, '\n');
        if (eol) {
            *eol = '\0';
        }
        char *p = cursor;
        int32_t line_ms = -1;

        while (*p == '[') {
            char *close = strchr(p, ']');
            if (!close) break;
            if (isdigit((unsigned char)p[1])) {
                int mm = atoi(p + 1);
                char *colon = strchr(p + 1, ':');
                if (colon && colon < close) {
                    int ss = atoi(colon + 1);
                    int hund = 0;
                    char *frac = strchr(colon + 1, '.');
                    if (!frac || frac > close) frac = strchr(colon + 1, ':');
                    if (frac && frac < close) {
                        hund = atoi(frac + 1);
                        if (hund < 10 && isdigit((unsigned char)frac[1]) && !isdigit((unsigned char)frac[2])) {
                            hund *= 100;
                        } else if (hund < 100) {
                            hund *= 10;
                        }
                    }
                    line_ms = (mm * 60 + ss) * 1000 + hund;
                }
            } else {
                /* Skip LRC metadata headers like [ar:...], [ti:...], [al:...] */
                p = close + 1;
                while (*p) p++;
                break;
            }
            p = close + 1;
        }

        while (*p && isspace((unsigned char)*p)) p++;

        char cleaned[88];
        clean_utf8_lyric_line(p, cleaned, sizeof(cleaned));
        if (cleaned[0] != '\0') {
            g_lyrics_lines[g_lyrics_count].time_ms = line_ms;
            strncpy(g_lyrics_lines[g_lyrics_count].text, cleaned, sizeof(g_lyrics_lines[0].text) - 1);
            g_lyrics_lines[g_lyrics_count].text[sizeof(g_lyrics_lines[0].text) - 1] = '\0';
            if (line_ms >= 0) {
                g_lyrics_has_timestamps = true;
            }
            g_lyrics_count++;
        }

        if (!eol) break;
        cursor = eol + 1;
    }
}

static const char *play_mode_short_label(dial_play_mode_t mode) {
    switch (mode) {
        case DIAL_PLAY_MODE_SEQUENTIAL: return "SEQ";
        case DIAL_PLAY_MODE_RANDOM:     return "RANDOM";
        case DIAL_PLAY_MODE_REPEAT_ALL: return "REP ALL";
        case DIAL_PLAY_MODE_REPEAT_ONE: return "REP 1";
        default:                        return "SEQ";
    }
}

static const char *play_mode_icon_name(dial_play_mode_t mode) {
    switch (mode) {
        case DIAL_PLAY_MODE_SEQUENTIAL: return "sequential";
        case DIAL_PLAY_MODE_RANDOM:     return "shuffle";
        case DIAL_PLAY_MODE_REPEAT_ALL: return "repeat";
        case DIAL_PLAY_MODE_REPEAT_ONE: return "repeat_one";
        default:                        return "sequential";
    }
}

/* ── Screen 3 Renderer: Now Playing (NP-01..04) ── */
static void render_now_playing_screen(dial_app_t *app, dial_fb_t *fb, const dial_palette_t *pal) {
    bool has_track = dial_media_has_active_track();
    const uint16_t *cover_tex = dial_media_get_now_cover_rgb565();
    dial_art_kind_t fallback_art = select_art_kind_for_string(dial_media_get_now_album());

    /* 1. Full-Bleed 360x360 Spinning Vinyl Art (NP-01) */
    dial_draw_art_tex(fb, pal, cover_tex, fallback_art, 180, 180, 360, true, app->vinyl_spin_deg);

    if (!has_track) {
        /* Clean "Not Playing" empty state -> Browse Library */
        dial_draw_scrim(fb, pal, 165);
        dial_draw_top_grab(fb, pal);
        draw_back_chevron(fb, pal, 0);
        draw_app_pebble(fb, pal, "rings", pal->moss, 180.0f, 118.0f, 76.0f, false, 255);
        dial_draw_text(fb, "Not Playing", 180, 174, 22, pal->text_1, 255, true);
        dial_draw_text(fb, "Choose music from your Library", 180, 202, 13, pal->text_2, 255, true);
        dial_draw_pill(fb, pal, 180, 244, "note", "Browse Library", true);
        return;
    }

    uint32_t pos_sec = 0, dur_sec = 0;
    float progress_01 = dial_media_get_progress(&pos_sec, &dur_sec);
    int vol = dial_media_get_volume();

    /* Base NP-01: Inner r=38px Vinyl Seek Ring + Right-Side Rim Volume Slider */
    if (!app->np_qa_open && !app->np_lyrics_open && !app->np_playlist_open && !app->np_details_open) {
        dial_draw_vinyl_seek_ring(fb, pal, progress_01);
        dial_draw_rim_arc(fb, pal, 145.0f, 35.0f, (float)vol / 100.0f,
                          pal->moss, (float)DIAL_RIM_ARC_R, 4.0f, true, false, true);
        char vol_str[8];
        snprintf(vol_str, sizeof(vol_str), "%d", vol);
        dial_draw_text(fb, vol_str, 326, 174, 11, pal->moss, 255, true);

        dial_draw_top_grab(fb, pal);
        draw_back_chevron(fb, pal, 0xECEEE8u);
        dial_draw_icon(fb, DIAL_ICON_UP, 170.0f, 324.0f, 20.0f, 2.2f, 0xECEEE8u, 215);
    }

    /* NP-02: Single-Tap Quick Actions Overlay (80% opacity / 20% transparent) */
    if (app->np_qa_open && !app->np_lyrics_open && !app->np_playlist_open && !app->np_details_open) {
        dial_draw_circle_fill(fb, 180.0f, 180.0f, 180.0f, pal->bg, 204); /* 80% opacity */
        dial_draw_top_grab(fb, pal);
        draw_back_chevron(fb, pal, 0);

        char time_lbl[32];
        snprintf(time_lbl, sizeof(time_lbl), "%02u:%02u / %02u:%02u",
                 (unsigned)(pos_sec / 60u), (unsigned)(pos_sec % 60u),
                 (unsigned)(dur_sec / 60u), (unsigned)(dur_sec % 60u));
        dial_draw_text(fb, "QUICK ACTIONS", 181, 35, 11, 0x000000u, 190, true);
        dial_draw_header(fb, pal, "QUICK ACTIONS", NULL, 34);
        dial_draw_text(fb, time_lbl, 181, 53, 11, 0x000000u, 190, true);
        dial_draw_text(fb, time_lbl, 180, 52, 11, pal->text_1, 255, true);

        const char *title  = dial_media_get_now_title();
        const char *artist = dial_media_get_now_artist();
        const char *album  = dial_media_get_now_album();

        if (dial_measure_text(title, 22, 0) > 260) {
            dial_draw_text_marquee(fb, title, 51, 97, 260, 22, 0x000000u, 200, app->last_tick_ms);
            dial_draw_text_marquee(fb, title, 50, 96, 260, 22, pal->text_1, 255, app->last_tick_ms);
        } else {
            dial_draw_text(fb, title, 181, 97, 22, 0x000000u, 200, true);
            dial_draw_text(fb, title, 180, 96, 22, pal->text_1, 255, true);
        }
        if (artist && artist[0]) {
            dial_draw_text(fb, artist, 181, 129, 13, 0x000000u, 200, true);
            dial_draw_text(fb, artist, 180, 128, 13, pal->moss, 255, true);
        }
        if (album && album[0]) {
            dial_draw_text(fb, album, 181, 149, 12, 0x000000u, 200, true);
            dial_draw_text(fb, album, 180, 148, 12, pal->text_1, 255, true);
        }

        dial_play_mode_t pmode = dial_media_get_play_mode();

        /* 4 Action Pucks along lower arc: Favorites, Shuffle/Mode, Add to Playlist, Lyrics */
        const float pucks_x[4] = { 66.0f, 142.0f, 218.0f, 294.0f };
        const float pucks_y[4] = { 216.0f, 244.0f, 244.0f, 216.0f };
        const char *icons[4]   = {
            "heart",
            play_mode_icon_name(pmode),
            "playlist",
            "lyrics"
        };
        const char *labels[4]  = {
            "FAV",
            play_mode_short_label(pmode),
            app->np_added_to_playlist ? "SAVED" : "PLAYLIST",
            "LYRICS"
        };
        const bool actives[4]  = {
            false,
            (pmode != DIAL_PLAY_MODE_SEQUENTIAL),
            app->np_added_to_playlist,
            false
        };
        const bool dangers[4]  = {
            app->np_favorite,
            false,
            false,
            false
        };

        for (int i = 0; i < 4; ++i) {
            draw_round_puck(fb, pal, icons[i], pucks_x[i], pucks_y[i], 48.0f,
                            actives[i], false, dangers[i], labels[i]);
        }
    }

    /* NP-03: Apple Music Style Live Lyrics (Blurry obscure background + crisp white lyrics on top) */
    if (app->np_lyrics_open) {
        /* Deep 2x frosted blur + obscure dark tint so album art colors glow softly behind white lyrics */
        dial_draw_scrim(fb, pal, 80);
        dial_draw_scrim(fb, pal, 165);
        dial_draw_top_grab(fb, pal);
        draw_back_chevron(fb, pal, 0xFFFFFFu);
        dial_draw_text(fb, "LYRICS", 181, 37, 11, 0x000000u, 190, true);
        dial_draw_header(fb, pal, "LYRICS", dial_media_get_now_title(), 36);

        ensure_track_lyrics_loaded(dial_media_get_now_filepath());
        if (g_lyrics_count > 0) {
            size_t active_idx = 0;
            int32_t cur_ms = (int32_t)((float)dur_sec * progress_01 * 1000.0f);
            if (g_lyrics_has_timestamps) {
                for (size_t i = 0; i < g_lyrics_count; ++i) {
                    if (g_lyrics_lines[i].time_ms >= 0 && g_lyrics_lines[i].time_ms <= cur_ms) {
                        active_idx = i;
                    }
                }
            } else {
                active_idx = (size_t)(progress_01 * (float)g_lyrics_count);
                if (active_idx >= g_lyrics_count) active_idx = g_lyrics_count - 1;
            }

            /* 1. Active Sentence (Never show past lines; big 22px pure white word-wrapped up to 3 lines) */
            const char *cur_sentence = g_lyrics_lines[active_idx].text;
            char cur_wrapped[3][64];
            size_t cur_lines = 0;
            {
                char tmp[88];
                strncpy(tmp, cur_sentence, sizeof(tmp) - 1);
                tmp[sizeof(tmp) - 1] = '\0';
                char *p = tmp;
                cur_wrapped[0][0] = '\0';
                while (*p && cur_lines < 3) {
                    while (*p && isspace((unsigned char)*p)) p++;
                    if (!*p) break;
                    char *sp = p;
                    while (*sp && !isspace((unsigned char)*sp)) sp++;
                    char saved = *sp;
                    *sp = '\0';

                    char candidate[64];
                    if (cur_wrapped[cur_lines][0] == '\0') {
                        snprintf(candidate, sizeof(candidate), "%s", p);
                    } else {
                        snprintf(candidate, sizeof(candidate), "%s %s", cur_wrapped[cur_lines], p);
                    }

                    if (cur_wrapped[cur_lines][0] != '\0' && dial_measure_text(candidate, 22, 0) > 268) {
                        cur_lines++;
                        if (cur_lines < 3) {
                            snprintf(cur_wrapped[cur_lines], sizeof(cur_wrapped[0]), "%s", p);
                        }
                    } else {
                        snprintf(cur_wrapped[cur_lines], sizeof(cur_wrapped[0]), "%s", candidate);
                    }

                    *sp = saved;
                    p = sp;
                }
                if (cur_lines < 3 && cur_wrapped[cur_lines][0] != '\0') {
                    cur_lines++;
                }
            }

            int base_y = (cur_lines == 1) ? 140 : ((cur_lines == 2) ? 124 : 110);
            for (size_t l = 0; l < cur_lines; ++l) {
                int ly = base_y + (int)l * 30;
                dial_draw_text(fb, cur_wrapped[l], 182, ly + 2, 22, 0x000000u, 220, true);
                dial_draw_text(fb, cur_wrapped[l], 180, ly,     22, 0xFFFFFFu, 255, true);
            }

            /* 2. Next Sentence Sneak-Peek at Bottom (Soft Gaussian-blurred 16px white preview) */
            if (active_idx + 1 < g_lyrics_count) {
                const char *next_sentence = g_lyrics_lines[active_idx + 1].text;
                char next_wrapped[2][64];
                size_t next_lines = 0;
                {
                    char tmp[88];
                    strncpy(tmp, next_sentence, sizeof(tmp) - 1);
                    tmp[sizeof(tmp) - 1] = '\0';
                    char *p = tmp;
                    next_wrapped[0][0] = '\0';
                    while (*p && next_lines < 2) {
                        while (*p && isspace((unsigned char)*p)) p++;
                        if (!*p) break;
                        char *sp = p;
                        while (*sp && !isspace((unsigned char)*sp)) sp++;
                        char saved = *sp;
                        *sp = '\0';

                        char candidate[64];
                        if (next_wrapped[next_lines][0] == '\0') {
                            snprintf(candidate, sizeof(candidate), "%s", p);
                        } else {
                            snprintf(candidate, sizeof(candidate), "%s %s", next_wrapped[next_lines], p);
                        }

                        if (next_wrapped[next_lines][0] != '\0' && dial_measure_text(candidate, 16, 0) > 236) {
                            next_lines++;
                            if (next_lines < 2) {
                                snprintf(next_wrapped[next_lines], sizeof(next_wrapped[0]), "%s", p);
                            }
                        } else {
                            snprintf(next_wrapped[next_lines], sizeof(next_wrapped[0]), "%s", candidate);
                        }

                        *sp = saved;
                        p = sp;
                    }
                    if (next_lines < 2 && next_wrapped[next_lines][0] != '\0') {
                        next_lines++;
                    }
                }

                /* Multi-tap Gaussian blur kernel for Apple Music out-of-focus upcoming lyric */
                static const int k_bx[9]   = { -2,  0,  2, -1,  0,  1, -2,  0,  2 };
                static const int k_by[9]   = { -1, -2, -1,  0,  0,  0,  1,  2,  1 };
                static const uint8_t k_ba[9] = { 16, 22, 16, 26, 36, 26, 16, 22, 16 };

                int next_base_y = 224;
                for (size_t l = 0; l < next_lines; ++l) {
                    int ny = next_base_y + (int)l * 24;
                    dial_draw_text(fb, next_wrapped[l], 181, ny + 1, 16, 0x000000u, 110, true);
                    for (int k = 0; k < 9; ++k) {
                        dial_draw_text(fb, next_wrapped[l], 180 + k_bx[k], ny + k_by[k],
                                       16, 0xE6EAE7u, k_ba[k], true);
                    }
                }
            }
        } else {
            draw_app_pebble(fb, pal, "lyrics", pal->moss, 180.0f, 136.0f, 64.0f, false, 255);
            dial_draw_text(fb, "No lyrics found", 181, 185, 16, 0x000000u, 200, true);
            dial_draw_text(fb, "No lyrics found", 180, 184, 16, 0xFFFFFFu, 255, true);
            dial_draw_text(fb, "No embedded or .lrc lyrics on track", 181, 209, 12, 0x000000u, 190, true);
            dial_draw_text(fb, "No embedded or .lrc lyrics on track", 180, 208, 12, 0xD0D6D2u, 255, true);
        }
    }

    /* Add to Playlist Sheet */
    if (app->np_playlist_open) {
        dial_draw_circle_fill(fb, 180.0f, 180.0f, 180.0f, pal->bg, 204); /* 80% opacity */
        dial_draw_top_grab(fb, pal);
        draw_back_chevron(fb, pal, 0);
        dial_draw_header(fb, pal, "ADD TO PLAYLIST", NULL, 38);

        static const char *const k_pl_names[3] = { "Favorites", "On-The-Go", "Late Night" };
        static const char *const k_pl_icons[3] = { "heart",     "playlist",  "moon"       };
        dial_rim_row_t rows[3];
        memset(rows, 0, sizeof(rows));
        for (size_t i = 0; i < 3; ++i) {
            rows[i].icon_name = k_pl_icons[i];
            rows[i].title     = k_pl_names[i];
            rows[i].sub       = "Tap to add current track";
            rows[i].has_check = app->np_added_to_playlist && (app->np_playlist_focus_idx == i);
        }
        dial_draw_rim_list(fb, pal, rows, 3, app->np_playlist_focus_idx % 3, 76, app->last_tick_ms);
    }

    /* NP-04: Swipe-Up Real Track Details Sheet (80% opacity = alpha 204 / 20% transparent) */
    if (app->np_details_open) {
        dial_draw_round_rect(fb, 16.0f, 58.0f, 328.0f, 302.0f, 30.0f,
                             pal->surface_1, 204, 1.0f, pal->line, 204);
        dial_draw_round_rect(fb, 165.0f, 68.0f, 30.0f, 4.0f, 2.0f,
                             pal->surface_3, 210, 0.0f, 0, 0);

        dial_draw_header(fb, pal, "TRACK DETAILS", dial_media_get_now_title(), 80);

        char sub_line[128];
        snprintf(sub_line, sizeof(sub_line), "%s · %s",
                 dial_media_get_now_artist()[0] ? dial_media_get_now_artist() : "Unknown Artist",
                 dial_media_get_now_album()[0]  ? dial_media_get_now_album()  : "Unknown Album");
        dial_draw_text(fb, sub_line, 181, 119, 12, 0x000000u, 200, true);
        dial_draw_text(fb, sub_line, 180, 118, 12, pal->text_1, 255, true);

        /* Format & Bitrate Chips */
        char fmt_chip[32];
        uint32_t sr = dial_media_get_now_sample_rate();
        uint32_t bd = dial_media_get_now_bit_depth();
        snprintf(fmt_chip, sizeof(fmt_chip), "%s %u/%u",
                 dial_media_get_now_format_name(),
                 (unsigned)bd, (unsigned)(sr / 1000u));

        char br_chip[32];
        snprintf(br_chip, sizeof(br_chip), "%u kbps", (unsigned)dial_media_get_now_bitrate_kbps());

        int w1 = 0;
        dial_draw_chip(fb, pal, 108, 138, fmt_chip, pal->moss, &w1);
        dial_draw_chip(fb, pal, 108 + w1 + 8, 138, br_chip, 0, NULL);

        /* Real Key-Value Rows */
        size_t q_total = 0;
        size_t q_idx   = dial_media_get_now_queue_index(&q_total);
        char kv_track[32], kv_dur[32], kv_size[32];
        snprintf(kv_track, sizeof(kv_track), "%02zu / %02zu", q_idx, q_total);
        snprintf(kv_dur, sizeof(kv_dur), "%02u:%02u / %02u:%02u",
                 (unsigned)(pos_sec / 60u), (unsigned)(pos_sec % 60u),
                 (unsigned)(dur_sec / 60u), (unsigned)(dur_sec % 60u));
        snprintf(kv_size, sizeof(kv_size), "%.2f MB",
                 (double)dial_media_get_now_file_size() / (1024.0 * 1024.0));

        const char *keys[3] = { "Track",    "Duration", "File Size" };
        const char *vals[3] = { kv_track,   kv_dur,     kv_size     };
        for (int r = 0; r < 3; ++r) {
            int ry = 172 + r * 26;
            dial_draw_text(fb, keys[r], 63, ry + 1, 12, 0x000000u, 200, false);
            dial_draw_text(fb, keys[r], 62, ry, 12, pal->text_1, 255, false);
            int vw = dial_measure_text(vals[r], 12, 0);
            dial_draw_text(fb, vals[r], 299 - vw, ry + 1, 12, 0x000000u, 200, false);
            dial_draw_text(fb, vals[r], 298 - vw, ry, 12, pal->text_1, 255, false);
            dial_draw_line_aa(fb, 60.0f, (float)(ry + 18), 300.0f, (float)(ry + 18), 1.0f, pal->line, 140);
        }

        const char *fpath = dial_media_get_now_filepath();
        if (dial_measure_text(fpath, 10, 0) > 220) {
            dial_draw_text_marquee(fb, fpath, 71, 259, 220, 10, 0x000000u, 190, app->last_tick_ms);
            dial_draw_text_marquee(fb, fpath, 70, 258, 220, 10, pal->text_2, 255, app->last_tick_ms);
        } else {
            dial_draw_text(fb, fpath, 181, 259, 10, 0x000000u, 190, true);
            dial_draw_text(fb, fpath, 180, 258, 10, pal->text_2, 255, true);
        }
        dial_draw_pill(fb, pal, 180, 286, NULL, "Close", true);
    }
}

/* ── Placeholder / Phase 2-4 Hub Screens (Navigable & Poppable via Back Gesture) ── */
static void render_sub_app_screen(dial_app_t *app,
                                  dial_fb_t *fb,
                                  const dial_palette_t *pal,
                                  const char *overline,
                                  const char *icon_name,
                                  uint32_t tint,
                                  const char *headline,
                                  const char *sub) {
    (void)app;
    dial_draw_top_grab(fb, pal);
    dial_draw_header(fb, pal, overline, "", 38);
    draw_back_chevron(fb, pal, 0);
    draw_app_pebble(fb, pal, icon_name, tint, 180.0f, 128.0f, 76.0f, false, 255);
    dial_draw_text(fb, headline, 180, 184, 18, pal->text_1, 255, true);
    dial_draw_text(fb, sub, 180, 210, 12, pal->text_2, 255, true);
    dial_draw_pill(fb, pal, 180, 248, "back", "Back to Home", true);
    dial_draw_hint(fb, pal, NULL, "SWIPE RIGHT · GO BACK", 0);
}

/* ── Global Overlay Renderer: Control Center (CC-01 & CC-02) ── */
static void render_control_center(dial_app_t *app, dial_fb_t *fb, const dial_palette_t *pal) {
    dial_draw_scrim(fb, pal, 225);

    char clock_str[16];
    dial_sys_get_clock_str(clock_str, sizeof(clock_str));
    int batt_pct = dial_sys_get_battery_pct(NULL);
    dial_draw_status_bar(fb, pal, clock_str, batt_pct, 0);

    if (app->cc_touch_locked) {
        /* CC-02: Touch Lock Shield */
        dial_draw_circle_fill(fb, 180.0f, 166.0f, 52.0f, pal->surface_1, 255);
        dial_draw_circle_stroke(fb, 180.0f, 166.0f, 52.0f, 3.0f, pal->surface_3, 255);
        dial_draw_icon(fb, DIAL_ICON_LOCK, 160.0f, 146.0f, 40.0f, 1.85f, pal->moss, 255);
        dial_draw_text(fb, "Screen locked", 180, 236, 17, pal->text_1, 255, true);
        dial_draw_text(fb, "Hold centre to unlock", 180, 258, 12, pal->text_2, 255, true);
        return;
    }

    /* Left Brightness Rim Arc (215° bottom = 0% -> 325° top = 100%) & Right Volume Rim Arc (145° bottom = 0% -> 35° top = 100%) */
    dial_draw_rim_arc(fb, pal, 215.0f, 325.0f, (float)app->brightness_pct / 100.0f,
                      pal->birch, (float)DIAL_RIM_ARC_R, 4.0f, true, false, true);
    dial_draw_rim_arc(fb, pal, 145.0f, 35.0f, (float)dial_media_get_volume() / 100.0f,
                      pal->moss, (float)DIAL_RIM_ARC_R, 4.0f, true, false, true);
    dial_draw_icon(fb, DIAL_ICON_SUN, 22.0f, 171.0f, 18.0f, 1.75f, pal->birch, 255);
    dial_draw_icon(fb, DIAL_ICON_VOLUME, 320.0f, 171.0f, 18.0f, 1.75f, pal->moss, 255);

    /* 3x2 Grid of 66px Icon Pucks */
    const float cols_x[3] = { 93.0f, 180.0f, 267.0f };
    const float rows_y[2] = { 131.0f, 219.0f };
    const char *icons[6] = {
        "wifi",
        "bluetooth",
        "mode_player",
        app->cc_high_gain ? "gain_high" : "gain_low",
        "usb_charge",
        "lock"
    };
    const uint32_t tones[6] = {
        app->cc_wifi_on    ? pal->fjord      : 0u,
        app->cc_bt_on      ? pal->fjord      : 0u,
        pal->moss,
        app->cc_high_gain  ? pal->cloudberry : 0u,
        app->cc_usb_charge ? pal->moss       : 0u,
        0u
    };

    for (int i = 0; i < 6; ++i) {
        float cx = cols_x[i % 3];
        float cy = rows_y[i / 3];
        uint32_t t = tones[i];
        uint32_t bg  = (t != 0) ? dial_color_lerp(pal->surface_1, t, 46) : pal->surface_1;
        uint32_t fg  = (t != 0) ? t : pal->text_3;
        uint32_t brd = (t != 0) ? t : pal->line;
        dial_draw_circle_fill(fb, cx, cy, 33.0f, bg, 255);
        dial_draw_circle_stroke(fb, cx, cy, 32.2f, (t != 0) ? 1.5f : 1.0f, brd, 255);
        dial_draw_icon_by_name(fb, icons[i], cx - 13.0f, cy - 13.0f, 26.0f, 1.75f, fg, 255);
    }

    dial_draw_hint(fb, pal, "up", "SWIPE UP TO CLOSE", 0);
}

/* ── Main Render Entry Point ── */
void dial_app_render(dial_app_t *app, dial_fb_t *fb) {
    if (!app || !fb) return;
    const dial_palette_t *pal = dial_get_palette(app->theme_mode);

    dial_draw_clear(fb, pal->bg);

    switch (app->current_screen) {
        case DIAL_SCREEN_HOME:
            render_home_screen(app, fb, pal);
            break;
        case DIAL_SCREEN_FILES:
            render_files_screen(app, fb, pal);
            break;
        case DIAL_SCREEN_NOW_PLAYING:
            render_now_playing_screen(app, fb, pal);
            break;
        case DIAL_SCREEN_LIBRARY: {
            char sub[96];
            format_home_app_subtitle(1, sub, sizeof(sub));
            render_sub_app_screen(app, fb, pal, "LIBRARY", "note", pal->fjord, "Music Library", sub);
            break;
        }
        case DIAL_SCREEN_MODES:
            render_sub_app_screen(app, fb, pal, "OPERATING MODES", "modes", pal->heather,
                                  "Player Mode", "Local playback · microSD");
            break;
        case DIAL_SCREEN_SETTINGS:
            render_sub_app_screen(app, fb, pal, "SETTINGS", "tune", pal->text_2,
                                  "System Settings", "Audio · Display · Power");
            break;
        case DIAL_SCREEN_ARCADE:
            render_sub_app_screen(app, fb, pal, "ARCADE", "games", pal->cloudberry,
                                  "Fjäll Arcade", "Dial Pong · 2048 · Boomshine");
            break;
    }

    if (app->cc_open) {
        render_control_center(app, fb, pal);
    }

    dial_draw_circle_mask(fb);
}

/* ── Event Dispatchers for Home, Files, Now Playing & Control Center ── */
static void activate_files_entry(dial_app_t *app, size_t idx) {
    const dial_fs_entry_t *ent = dial_media_get_fs_entry(idx);
    if (!ent) return;

    if (strcmp(ent->name, "..") == 0) {
        (void)dial_app_pop_back(app);
        return;
    }

    if (ent->is_dir) {
        char next_dir[512];
        strncpy(next_dir, ent->full_path, sizeof(next_dir) - 1);
        next_dir[sizeof(next_dir) - 1] = '\0';

        if (app->folder_depth < DIAL_FOLDER_DEPTH_MAX) {
            app->folder_focus_stack[app->folder_depth++] = app->files_focus_idx;
        }
        if (dial_media_browse_dir(next_dir)) {
            app->files_focus_idx = 0;
            dial_hal_haptic_tick(10);
        }
    } else {
        /* Real Audio File -> Start real playback and push Now Playing screen */
        if (dial_media_play_fs_entry(idx)) {
            dial_app_push_screen(app, DIAL_SCREEN_NOW_PLAYING);
        }
    }
}

static void handle_np_tap(dial_app_t *app, float x, float y) {
    /* 9 o'clock `<` back chevron always pops back */
    if (hit_test_back_chevron(x, y)) {
        (void)dial_app_pop_back(app);
        return;
    }

    if (!dial_media_has_active_track()) {
        /* Tap on "Browse Library" pill navigates to Library */
        if (y >= 220.0f && y <= 284.0f) {
            dial_app_push_screen(app, DIAL_SCREEN_LIBRARY);
        }
        return;
    }

    if (app->np_lyrics_open) {
        app->np_lyrics_open = false;
        sync_gesture_fsm_context(app);
        dial_hal_haptic_tick(8);
        return;
    }

    if (app->np_playlist_open) {
        int cur_y = 76;
        for (size_t i = 0; i < 3; ++i) {
            int rh = (i == (app->np_playlist_focus_idx % 3)) ? 52 : 44;
            if (y >= (float)cur_y && y <= (float)(cur_y + rh) && x >= 38.0f && x <= 322.0f) {
                app->np_playlist_focus_idx = i;
                app->np_added_to_playlist  = true;
                app->np_playlist_open      = false;
                sync_gesture_fsm_context(app);
                dial_hal_haptic_tick(12);
                return;
            }
            cur_y += rh + DIAL_SPACE_2;
        }
        app->np_playlist_open = false;
        sync_gesture_fsm_context(app);
        return;
    }

    if (app->np_details_open) {
        if (y >= 270.0f || y < 60.0f) {
            app->np_details_open = false;
            sync_gesture_fsm_context(app);
            dial_hal_haptic_tick(8);
        }
        return;
    }

    if (app->np_qa_open) {
        /* Check 4 Action Pucks: Favorites, Shuffle/Mode, Add to Playlist, Lyrics */
        const float pucks_x[4] = { 66.0f, 142.0f, 218.0f, 294.0f };
        const float pucks_y[4] = { 216.0f, 244.0f, 244.0f, 216.0f };
        for (int i = 0; i < 4; ++i) {
            float dx = x - pucks_x[i];
            float dy = y - pucks_y[i];
            if ((dx * dx + dy * dy) <= (32.0f * 32.0f)) {
                dial_hal_haptic_tick(12);
                if (i == 0) {
                    app->np_favorite = !app->np_favorite;
                    dial_hal_send_ipc(DIAL_CMD_FAVORITE, app->np_favorite ? "1" : "0");
                } else if (i == 1) {
                    (void)dial_media_cycle_play_mode();
                } else if (i == 2) {
                    app->np_playlist_open = true;
                    sync_gesture_fsm_context(app);
                } else if (i == 3) {
                    app->np_lyrics_open = true;
                    sync_gesture_fsm_context(app);
                }
                return;
            }
        }

        /* Tapping anywhere else closes NP-02 back to NP-01 */
        app->np_qa_open = false;
        sync_gesture_fsm_context(app);
        dial_hal_haptic_tick(8);
        return;
    }

    /* On NP-01: Check bottom `^` chevron tap -> open NP-04 Track Details */
    if (y > 312.0f && x >= 140.0f && x <= 220.0f) {
        app->np_details_open = true;
        sync_gesture_fsm_context(app);
        dial_hal_haptic_tick(12);
        return;
    }

    /* Ignore taps that occurred on the right-rim volume slider */
    if (x >= 290.0f && y >= 60.0f && y <= 300.0f) {
        return;
    }

    /* Otherwise toggle NP-02 Quick Actions */
    app->np_qa_open = true;
    sync_gesture_fsm_context(app);
    dial_hal_haptic_tick(10);
}

static void dispatch_gesture_event(dial_app_t *app, const dial_gesture_event_t *ev) {
    if (!app || !ev || ev->type == DIAL_EVENT_NONE) return;

    /* 0. If Touch Lock Shield (CC-02) is active, only 600ms Hold unlocks it */
    if (app->cc_open && app->cc_touch_locked) {
        if (ev->type == DIAL_EVENT_HOLD_600MS) {
            app->cc_touch_locked = false;
            dial_hal_haptic_tick(25);
        }
        return;
    }

    /* 1. Global Top-Rim Pull-Down -> Open Control Center (CC-01) */
    if (ev->type == DIAL_EVENT_OPEN_CONTROL_CENTER) {
        app->cc_open = true;
        sync_gesture_fsm_context(app);
        return;
    }

    /* 2. Control Center Overlay Interactions */
    if (app->cc_open) {
        if (ev->type == DIAL_EVENT_BOTTOM_SWIPE_UP ||
            (ev->type == DIAL_EVENT_SWIPE_RIGHT && ev->start_x < 48.0f)) {
            app->cc_open = false;
            sync_gesture_fsm_context(app);
            return;
        }
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            if (ev->x >= 180.0f) {
                /* Right rim: moving UP (counter-clockwise, detent_steps < 0) increases volume */
                int vol = dial_media_get_volume() - (int)ev->detent_steps * 2;
                dial_media_set_volume(vol);
            } else {
                /* Left rim: moving UP (clockwise, detent_steps > 0) increases brightness */
                int br = app->brightness_pct + (int)ev->detent_steps * 2;
                if (br < 5) br = 5;
                if (br > 100) br = 100;
                app->brightness_pct = br;
            }
            return;
        }
        if (ev->type == DIAL_EVENT_TAP) {
            const float cols_x[3] = { 93.0f, 180.0f, 267.0f };
            const float rows_y[2] = { 131.0f, 219.0f };
            for (int i = 0; i < 6; ++i) {
                float dx = ev->x - cols_x[i % 3];
                float dy = ev->y - rows_y[i / 3];
                if ((dx * dx + dy * dy) <= (34.0f * 34.0f)) {
                    dial_hal_haptic_tick(12);
                    if (i == 0) app->cc_wifi_on = !app->cc_wifi_on;
                    else if (i == 1) app->cc_bt_on = !app->cc_bt_on;
                    else if (i == 2) {
                        app->cc_open = false;
                        dial_app_push_screen(app, DIAL_SCREEN_MODES);
                    } else if (i == 3) {
                        app->cc_high_gain = !app->cc_high_gain;
                        dial_hal_send_ipc(DIAL_CMD_GAIN_MODE, app->cc_high_gain ? "high" : "low");
                    } else if (i == 4) {
                        app->cc_usb_charge = !app->cc_usb_charge;
                        dial_hal_send_ipc(DIAL_CMD_USB_CHARGE, app->cc_usb_charge ? "1" : "0");
                    } else if (i == 5) {
                        app->cc_touch_locked = true;
                    }
                    return;
                }
            }
            if (ev->y > 280.0f) {
                app->cc_open = false;
                sync_gesture_fsm_context(app);
            }
        }
        return;
    }

    /* 3. Universal Left-Edge Swipe-Right ("Go Back": start_x < 48px) */
    if (ev->type == DIAL_EVENT_SWIPE_RIGHT && ev->start_x < 48.0f &&
        app->current_screen != DIAL_SCREEN_HOME) {
        (void)dial_app_pop_back(app);
        return;
    }

    /* 4. Screen-Specific Event Dispatch */
    switch (app->current_screen) {
        case DIAL_SCREEN_HOME: {
            if (ev->type == DIAL_EVENT_RIM_DETENT) {
                int next = ((int)app->home_focus_idx + (int)ev->detent_steps) % DIAL_HOME_APP_COUNT;
                if (next < 0) next += DIAL_HOME_APP_COUNT;
                app->home_focus_idx = (size_t)next;
            } else if (ev->type == DIAL_EVENT_SWIPE_LEFT) {
                app->home_focus_idx = (app->home_focus_idx + 1) % DIAL_HOME_APP_COUNT;
            } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
                app->home_focus_idx = (app->home_focus_idx + DIAL_HOME_APP_COUNT - 1) % DIAL_HOME_APP_COUNT;
            } else if (ev->type == DIAL_EVENT_HOLD_450MS) {
                app->home_wheel_mode = !app->home_wheel_mode;
            } else if (ev->type == DIAL_EVENT_TAP) {
                if (app->home_wheel_mode) {
                    for (size_t i = 0; i < DIAL_HOME_APP_COUNT; ++i) {
                        float px = 0.0f, py = 0.0f;
                        dial_polar_to_xy((float)(i * 60), 110.0f, &px, &py);
                        float dx = ev->x - px, dy = ev->y - py;
                        if ((dx * dx + dy * dy) <= (34.0f * 34.0f)) {
                            app->home_focus_idx  = i;
                            app->home_wheel_mode = false;
                            dial_app_push_screen(app, home_idx_to_screen(i));
                            return;
                        }
                    }
                    app->home_wheel_mode = false;
                    return;
                }

                /* Check neighbour pebbles on left (x < 95, y < 155) or right (x > 265, y < 155) */
                if (ev->y < 155.0f && ev->x < 95.0f) {
                    app->home_focus_idx = (app->home_focus_idx + DIAL_HOME_APP_COUNT - 1) % DIAL_HOME_APP_COUNT;
                    dial_hal_haptic_tick(8);
                    return;
                }
                if (ev->y < 155.0f && ev->x > 265.0f) {
                    app->home_focus_idx = (app->home_focus_idx + 1) % DIAL_HOME_APP_COUNT;
                    dial_hal_haptic_tick(8);
                    return;
                }

                /* Tap on focused pebble or title opens the focused app */
                dial_app_push_screen(app, home_idx_to_screen(app->home_focus_idx));
            }
            break;
        }

        case DIAL_SCREEN_FILES: {
            size_t total = dial_media_get_fs_entry_count();
            bool is_empty_folder = (total == 0) ||
                                   (total == 1 && strcmp(dial_media_get_fs_entry(0)->name, "..") == 0);

            if (app->files_delete_confirm_open) {
                if (ev->type == DIAL_EVENT_HOLD_600MS) {
                    dial_media_delete_fs_entry(app->files_focus_idx);
                    app->files_delete_confirm_open = false;
                    app->files_action_sheet_open   = false;
                    size_t new_total = dial_media_get_fs_entry_count();
                    if (app->files_focus_idx >= new_total && new_total > 0) {
                        app->files_focus_idx = new_total - 1;
                    }
                } else if (ev->type == DIAL_EVENT_TAP && ev->x < 170.0f && ev->y >= 220.0f) {
                    app->files_delete_confirm_open = false;
                    dial_hal_haptic_tick(8);
                } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
                    app->files_delete_confirm_open = false;
                }
                break;
            }

            if (app->files_action_sheet_open) {
                if (ev->type == DIAL_EVENT_TAP) {
                    if (ev->y >= 190.0f && ev->y <= 325.0f) {
                        if (ev->x < 180.0f) {
                            app->files_action_sheet_open = false;
                            activate_files_entry(app, app->files_focus_idx);
                        } else {
                            app->files_action_sheet_open   = false;
                            app->files_delete_confirm_open = true;
                            dial_hal_haptic_tick(12);
                        }
                    } else if (ev->y < 140.0f) {
                        app->files_action_sheet_open = false;
                    }
                } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT || ev->type == DIAL_EVENT_BOTTOM_SWIPE_UP) {
                    app->files_action_sheet_open = false;
                }
                break;
            }

            if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
                (void)dial_app_pop_back(app);
                break;
            }

            size_t win_count = (total < 4) ? total : 4;
            size_t win_start = 0;
            if (total > 4) {
                if (app->files_focus_idx <= 1) win_start = 0;
                else if (app->files_focus_idx + 2 >= total) win_start = total - 4;
                else win_start = app->files_focus_idx - 1;
            }

            if (ev->type == DIAL_EVENT_RIM_DETENT && total > 0 && !is_empty_folder) {
                int next = (int)app->files_focus_idx + (int)ev->detent_steps;
                if (next < 0) next = 0;
                if (next >= (int)total) next = (int)total - 1;
                app->files_focus_idx = (size_t)next;
            } else if (ev->type == DIAL_EVENT_HOLD_450MS && total > 0 && !is_empty_folder) {
                int cur_y = 66;
                for (size_t i = 0; i < win_count; ++i) {
                    size_t idx = win_start + i;
                    int rh = (idx == app->files_focus_idx) ? 52 : 44;
                    if (ev->y >= (float)(cur_y - 4) && ev->y <= (float)(cur_y + rh + DIAL_SPACE_2) &&
                        ev->x >= 18.0f && ev->x <= 342.0f) {
                        app->files_focus_idx = idx;
                        break;
                    }
                    cur_y += rh + DIAL_SPACE_2;
                }
                const dial_fs_entry_t *ent = dial_media_get_fs_entry(app->files_focus_idx);
                if (ent && strcmp(ent->name, "..") != 0) {
                    app->files_action_sheet_open = true;
                }
            } else if (ev->type == DIAL_EVENT_TAP) {
                if (hit_test_back_chevron(ev->x, ev->y) || is_empty_folder) {
                    (void)dial_app_pop_back(app);
                    break;
                }
                /* Hit-test the 4 visible rows in the Fisheye Rim List (top_y = 66) */
                bool matched_row = false;
                int cur_y = 66;
                for (size_t i = 0; i < win_count; ++i) {
                    size_t idx = win_start + i;
                    int rh = (idx == app->files_focus_idx) ? 52 : 44;
                    if (ev->y >= (float)(cur_y - 6) && ev->y <= (float)(cur_y + rh + DIAL_SPACE_2) &&
                        ev->x >= 18.0f && ev->x <= 342.0f) {
                        app->files_focus_idx = idx;
                        activate_files_entry(app, idx);
                        matched_row = true;
                        break;
                    }
                    cur_y += rh + DIAL_SPACE_2;
                }
                /* If fewer than 4 rows are visible (e.g. 1 folder on SD card) and user taps center stage, open focused row */
                if (!matched_row && total > 0 && win_count < 4 &&
                    ev->y >= 54.0f && ev->y <= 300.0f && ev->x >= 46.0f && ev->x <= 330.0f) {
                    activate_files_entry(app, app->files_focus_idx);
                }
            }
            break;
        }

        case DIAL_SCREEN_NOW_PLAYING: {
            if (app->np_playlist_open && ev->type == DIAL_EVENT_RIM_DETENT) {
                int next = (int)app->np_playlist_focus_idx + (int)ev->detent_steps;
                if (next < 0) next = 0;
                if (next > 2) next = 2;
                app->np_playlist_focus_idx = (size_t)next;
                break;
            }
            if (ev->type == DIAL_EVENT_PLAY_PAUSE) {
                bool next_play = !dial_media_get_playing();
                dial_media_set_playing(next_play);
            } else if (ev->type == DIAL_EVENT_TURNTABLE_LOCKED) {
                app->vinyl_grabbed         = true;
                app->vinyl_grab_spin_deg   = app->vinyl_spin_deg;
                app->vinyl_grab_finger_deg = app->gesture_fsm.last_deg;
                app->vinyl_last_finger_deg = app->gesture_fsm.last_deg;
            } else if (ev->type == DIAL_EVENT_SEEK) {
                dial_media_seek_progress(ev->seek_progress);
            } else if (ev->type == DIAL_EVENT_NP_QUICK_ACTIONS || ev->type == DIAL_EVENT_TAP) {
                handle_np_tap(app, ev->x, ev->y);
            } else if (ev->type == DIAL_EVENT_BOTTOM_SWIPE_UP) {
                if (dial_media_has_active_track()) {
                    app->np_qa_open       = false;
                    app->np_lyrics_open   = false;
                    app->np_playlist_open = false;
                    app->np_details_open  = !app->np_details_open;
                    sync_gesture_fsm_context(app);
                }
            } else if (ev->type == DIAL_EVENT_SWIPE_LEFT) {
                if (dial_media_has_active_track()) {
                    dial_media_next_track();
                }
            } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
                /* Stage swipe-right (start_x >= 48px) skips to previous track */
                if (dial_media_has_active_track()) {
                    dial_media_prev_track();
                } else {
                    (void)dial_app_pop_back(app);
                }
            } else if (ev->type == DIAL_EVENT_RIM_DETENT) {
                /* Right rim: moving finger UP toward top (35°) produces negative detent_steps -> increase volume */
                int vol = dial_media_get_volume() - (int)ev->detent_steps * 2;
                dial_media_set_volume(vol);
            }
            break;
        }

        default: {
            if (ev->type == DIAL_EVENT_SWIPE_RIGHT || ev->type == DIAL_EVENT_TAP) {
                (void)dial_app_pop_back(app);
            }
            break;
        }
    }
}

void dial_app_handle_touch(dial_app_t *app, const dial_touch_event_t *touch) {
    if (!app || !touch) return;

    /* Direct 1:1 Rim Slider Tracking for Right-Rim Volume & Left-Rim Brightness */
    bool on_np_base = (app->current_screen == DIAL_SCREEN_NOW_PLAYING) &&
                      !app->cc_open && !app->np_qa_open && !app->np_lyrics_open &&
                      !app->np_playlist_open && !app->np_details_open &&
                      dial_media_has_active_track();
    bool on_cc_unlocked = app->cc_open && !app->cc_touch_locked;

    float pr = 0.0f, pdeg = 0.0f;
    dial_xy_to_polar(touch->x, touch->y, &pr, &pdeg);

    if (touch->pressed && (on_np_base || on_cc_unlocked)) {
        /* Right-Rim Volume Slider (35° top = 100%, 145° bottom = 0%) */
        if (!app->gesture_fsm.touch_active &&
            pr >= 136.0f && touch->x >= 200.0f && pdeg >= 25.0f && pdeg <= 155.0f) {
            app->rim_slider_active = true;
        }
        if (app->rim_slider_active && touch->x >= 180.0f) {
            float t = (145.0f - pdeg) / 110.0f;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            int new_vol = (int)roundf(t * 100.0f);
            if (new_vol != dial_media_get_volume()) {
                dial_media_set_volume(new_vol);
            }
            return;
        }
        /* Left-Rim Brightness Slider in Control Center (215° bottom = 0%, 325° top = 100%) */
        if (on_cc_unlocked && !app->gesture_fsm.touch_active &&
            pr >= 136.0f && touch->x <= 160.0f && pdeg >= 205.0f && pdeg <= 335.0f) {
            app->rim_slider_active = true;
        }
        if (on_cc_unlocked && app->rim_slider_active && touch->x <= 180.0f) {
            float t = (pdeg - 215.0f) / 110.0f;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            int new_br = (int)roundf(t * 100.0f);
            if (new_br < 5) new_br = 5;
            app->brightness_pct = new_br;
            return;
        }
    } else if (!touch->pressed && app->rim_slider_active) {
        app->rim_slider_active = false;
        return;
    }

    /* Snapshot current vinyl disc rotation on touch-down so seeking rotates from current disc angle */
    if (on_np_base && touch->pressed && !app->gesture_fsm.touch_active) {
        app->vinyl_grab_spin_deg   = app->vinyl_spin_deg;
        app->vinyl_grab_finger_deg = pdeg;
        app->vinyl_last_finger_deg = pdeg;
        app->vinyl_grabbed         = false;
    } else if (!touch->pressed) {
        app->vinyl_grabbed = false;
    }

    sync_gesture_fsm_context(app);
    dial_gesture_event_t ev = dial_gesture_fsm_step(&app->gesture_fsm, touch);

    /* When seeking, grab the current position of the disc and rotate 1:1 with finger position */
    if (on_np_base && touch->pressed &&
        (ev.type == DIAL_EVENT_SEEK ||
         app->gesture_fsm.state == DIAL_GESTURE_VINYL_RING_SEEK ||
         app->gesture_fsm.state == DIAL_GESTURE_TURNTABLE_SCRUB)) {
        if (!app->vinyl_grabbed) {
            float delta = dial_angle_diff(app->vinyl_grab_finger_deg, pdeg);
            app->vinyl_spin_deg        = dial_normalize_deg(app->vinyl_grab_spin_deg + delta);
            app->vinyl_last_finger_deg = pdeg;
            app->vinyl_grabbed         = true;
        } else {
            float d_deg = dial_angle_diff(app->vinyl_last_finger_deg, pdeg);
            app->vinyl_spin_deg        = dial_normalize_deg(app->vinyl_spin_deg + d_deg);
            app->vinyl_last_finger_deg = pdeg;
        }
    }

    if (ev.type != DIAL_EVENT_NONE) {
        dispatch_gesture_event(app, &ev);
    }
}

void dial_app_handle_key(dial_app_t *app, const dial_key_event_t *key) {
    if (!app || !key || !key->pressed) return;

    switch (key->key) {
        case DIAL_KEY_PLAYPAUSE: {
            if (app->current_screen == DIAL_SCREEN_HOME) {
                dial_app_push_screen(app, home_idx_to_screen(app->home_focus_idx));
            } else if (app->current_screen == DIAL_SCREEN_FILES) {
                activate_files_entry(app, app->files_focus_idx);
            } else if (dial_media_has_active_track()) {
                bool next_play = !dial_media_get_playing();
                dial_media_set_playing(next_play);
                dial_hal_send_ipc(DIAL_CMD_PLAY_PAUSE, next_play ? "play" : "pause");
                dial_hal_haptic_tick(15);
            }
            break;
        }
        case DIAL_KEY_POWER:
            (void)dial_app_pop_back(app);
            break;
        case DIAL_KEY_VOLUP: {
            if (app->current_screen == DIAL_SCREEN_FILES) {
                if (app->files_focus_idx > 0) app->files_focus_idx--;
            } else {
                dial_media_set_volume(dial_media_get_volume() + 2);
            }
            break;
        }
        case DIAL_KEY_VOLDOWN: {
            if (app->current_screen == DIAL_SCREEN_FILES) {
                size_t total = dial_media_get_fs_entry_count();
                if (app->files_focus_idx + 1 < total) app->files_focus_idx++;
            } else {
                dial_media_set_volume(dial_media_get_volume() - 2);
            }
            break;
        }
        case DIAL_KEY_NEXT_SCREEN:
            if (app->current_screen == DIAL_SCREEN_HOME) {
                app->home_focus_idx = (app->home_focus_idx + 1) % DIAL_HOME_APP_COUNT;
            } else if (app->current_screen == DIAL_SCREEN_NOW_PLAYING) {
                dial_media_next_track();
            }
            break;
        case DIAL_KEY_PREV_SCREEN:
            if (app->current_screen == DIAL_SCREEN_HOME) {
                app->home_focus_idx = (app->home_focus_idx + DIAL_HOME_APP_COUNT - 1) % DIAL_HOME_APP_COUNT;
            } else {
                (void)dial_app_pop_back(app);
            }
            break;
        case DIAL_KEY_TOGGLE_THEME:
            app->theme_mode = (app->theme_mode == DIAL_MODE_NIGHT) ? DIAL_MODE_DAY : DIAL_MODE_NIGHT;
            break;
        case DIAL_KEY_SCREENSHOT: {
            dial_fb_t *fb = dial_hal_get_framebuffer();
            dial_hal_save_png(fb, "/tmp/dialos_live.png");
            printf("[DialOS] Saved live screenshot to /tmp/dialos_live.png\n");
            break;
        }
        default:
            break;
    }
}

void dial_app_tick(dial_app_t *app, uint32_t now_ms) {
    if (!app) return;
    uint32_t dt_ms = (now_ms > app->last_tick_ms) ? (now_ms - app->last_tick_ms) : 0u;
    app->last_tick_ms = now_ms;

    /* Pause auto-spin while finger is grabbing the seek ring or scrubbing the vinyl */
    bool holding_seek_ring = app->gesture_fsm.touch_active &&
                             app->gesture_fsm.is_now_playing_screen &&
                             app->gesture_fsm.start_r >= (float)DIAL_VINYL_SEEK_GRAB_MIN &&
                             app->gesture_fsm.start_r <= (float)DIAL_VINYL_SEEK_GRAB_MAX;

    if (dial_media_get_playing() && !app->vinyl_grabbed && !holding_seek_ring) {
        const float deg_per_ms = (DIAL_VINYL_RPM * 360.0f) / 60000.0f;
        app->vinyl_spin_deg = fmodf(app->vinyl_spin_deg + (float)dt_ms * deg_per_ms, 360.0f);
    }

    sync_gesture_fsm_context(app);
    dial_gesture_event_t ev = dial_gesture_fsm_tick(&app->gesture_fsm, now_ms);
    if (ev.type != DIAL_EVENT_NONE) {
        dispatch_gesture_event(app, &ev);
    }
}

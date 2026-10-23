#include "dial_app.h"
#include "dial_screens.h"
#include "dial_media.h"
#include "dial_ipc.h"
#include "dial_dac.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static dial_app_t g_app;
static bool       g_app_inited = false;

dial_app_t *dial_app_get_state(void) {
    if (!g_app_inited) {
        dial_app_init(&g_app, "NP-01", DIAL_MODE_NIGHT);
    }
    return &g_app;
}

static bool is_np01_screen(const char *id) {
    return id && strcmp(id, "NP-01") == 0;
}

void dial_app_set_screen(dial_app_t *app, const char *screen_id) {
    if (!app || !screen_id) return;
    if (strcmp(app->screen_id, screen_id) != 0 &&
        strcmp(screen_id, "CC-01") != 0 &&
        strcmp(screen_id, "CC-02") != 0 &&
        strcmp(screen_id, "NP-02") != 0 &&
        strcmp(screen_id, "NP-04") != 0 &&
        strcmp(screen_id, "NP-05") != 0) {
        strncpy(app->prev_screen_id, app->screen_id, sizeof(app->prev_screen_id) - 1);
    }
    memset(app->screen_id, 0, sizeof(app->screen_id));
    strncpy(app->screen_id, screen_id, sizeof(app->screen_id) - 1);

    app->fsm.is_now_playing_screen = is_np01_screen(screen_id);
    app->fsm.current_seek_deg      = app->seek_progress * 360.0f;
}

void dial_app_show_toast(dial_app_t *app,
                         const char *text,
                         const char *icon_name,
                         uint32_t tone,
                         uint32_t now_ms) {
    if (!app || !text) return;
    memset(app->toast_text, 0, sizeof(app->toast_text));
    strncpy(app->toast_text, text, sizeof(app->toast_text) - 1);
    memset(app->toast_icon, 0, sizeof(app->toast_icon));
    strncpy(app->toast_icon, (icon_name && icon_name[0]) ? icon_name : "check", sizeof(app->toast_icon) - 1);
    app->toast_tone     = tone;
    app->toast_until_ms = (now_ms > 0 ? now_ms : 1u) + 1600u;
}

void dial_app_init(dial_app_t *app, const char *initial_screen_id, dial_mode_t mode) {
    if (!app) return;
    memset(app, 0, sizeof(*app));
    g_app_inited = true;

    const char *init_id = (initial_screen_id && initial_screen_id[0]) ? initial_screen_id : "NP-01";
    strncpy(app->screen_id, init_id, sizeof(app->screen_id) - 1);
    strncpy(app->prev_screen_id, "NP-01", sizeof(app->prev_screen_id) - 1);
    app->mode                = mode;

    /* Default state matching fjall-design-system.html master screens */
    app->is_playing          = true;
    app->seek_progress       = 0.41f;
    app->volume              = 82;
    app->brightness          = 70;
    app->is_favorited        = true;
    app->is_shuffled         = false;
    app->volume_hud_until_ms = 0;

    app->wifi_on             = true;
    app->bt_on               = true;
    app->active_mode_idx     = 0; /* 0 = mode_player */
    app->high_gain           = true;
    app->usb_charge          = false;
    app->touch_locked        = false;
    app->unlock_hold_progress= 0.42f; /* Default 150/360 deg visual preview for static CC-02 */

    app->home_focus_idx      = (strcmp(init_id, "HOM-02") == 0) ? 1 : 0;
    app->list_focus_idx      = 2; /* Focus row 2 ("Artists") on LIB-01 */
    app->album_focus_idx     = 0; /* Focus album 0 ("Mezzanine" / fjord) on LIB-02 */
    app->track_focus_idx     = 2; /* Focus track 2 ("03 Teardrop") on LIB-03 */
    app->playing_track_idx   = 2; /* Playing track 2 ("03 Teardrop") on LIB-03 */
    app->artist_focus_idx    = 2; /* Focus artist 2 ("Moderat") on LIB-04 */
    app->artist_jump_letter  = 'M';
    app->file_focus_idx      = 2; /* Focus item 2 ("03 Teardrop.flac") on FIL-01 */
    app->delete_hold_progress= 0.0f;
    app->file_deleted        = false;

    app->mode_picker_focus   = 0; /* Focus mode 0 ("Player") on MOD-00 */
    app->settings_focus_idx  = 2; /* Focus row 2 ("Bluetooth") on SET-01 */
    app->audio_focus_idx     = 1; /* Focus row 1 ("Gain") on SET-02 */
    app->dac_filter_idx      = 0; /* Applied filter 0 ("Fast roll-off · linear") on SET-03 */
    app->gapless_on          = true;
    app->eq_preset_idx       = 0; /* 0 = "Nordic Warm" on SET-04 */
    app->eq_focus_band       = 2; /* Focus band 2 ("125 Hz · +2.4 dB") on SET-04 */
    static const float kDefaultEq[10] = {
        0.52f, 0.40f, 0.32f, 0.55f, 0.62f, 0.58f, 0.44f, 0.38f, 0.46f, 0.54f
    };
    for (int i = 0; i < 10; ++i) {
        app->eq_band_norm[i] = kDefaultEq[i];
    }
    app->wifi_focus_idx      = 1; /* Focus row 1 ("Studio 5G") on SET-06 */
    app->bt_focus_idx        = 1; /* Focus row 1 ("WH-1000XM5") on SET-07 */
    app->pong_paddle_deg     = 202.0f; /* Default 182°..222° paddle center on GAM-02 */
    app->toast_until_ms      = 0;

    dial_gesture_fsm_init(&app->fsm, is_np01_screen(init_id), 330000u);
    app->fsm.current_seek_deg = app->seek_progress * 360.0f;
}

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void handle_disambiguated_event(dial_app_t *app,
                                       const dial_gesture_event_t *ev,
                                       uint32_t now_ms) {
    if (!app || !ev || ev->type == DIAL_EVENT_NONE) return;

    const char *scr = app->screen_id;

    /* 1. Global Top Pull-Down -> Control Center (CC-01) unless locked */
    if (ev->type == DIAL_EVENT_OPEN_CONTROL_CENTER) {
        if (!app->touch_locked && strcmp(scr, "CC-02") != 0) {
            if (strcmp(scr, "CC-01") != 0) {
                strncpy(app->prev_screen_id, scr, sizeof(app->prev_screen_id) - 1);
            }
            dial_app_set_screen(app, "CC-01");
        }
        return;
    }

    /* 2. CC-02 Touch Lock Shield: only 600ms center hold unlocks */
    if (strcmp(scr, "CC-02") == 0) {
        if (ev->type == DIAL_EVENT_HOLD_600MS && ev->r <= 68.0f) {
            app->touch_locked = false;
            app->unlock_hold_progress = 1.0f;
            dial_app_set_screen(app, "CC-01");
        }
        return;
    }

    /* 3. Lock Screens (LCK-01 / LCK-02) */
    if (strcmp(scr, "LCK-01") == 0 || strcmp(scr, "LCK-02") == 0) {
        if (ev->type == DIAL_EVENT_BOTTOM_SWIPE_UP) {
            dial_app_set_screen(app, "HOM-01");
            return;
        }
        if (strcmp(scr, "LCK-02") == 0 && ev->type == DIAL_EVENT_TAP) {
            /* Check 3 bottom transport controls on LCK-02 */
            if (hypotf(ev->x - 180.0f, ev->y - 286.0f) <= 28.0f) {
                app->is_playing = !app->is_playing;
                dial_hal_send_ipc(DIAL_CMD_PLAY_PAUSE, app->is_playing ? "play" : "pause");
            }
        }
        return;
    }

    /* 4. Now Playing (NP-01) */
    if (strcmp(scr, "NP-01") == 0) {
        if (ev->type == DIAL_EVENT_SEEK) {
            app->seek_progress = ev->seek_progress;
            dial_media_seek_progress(app->seek_progress);
        } else if (ev->type == DIAL_EVENT_PLAY_PAUSE) {
            app->is_playing = !app->is_playing;
            dial_media_set_playing(app->is_playing);
            dial_hal_send_ipc(DIAL_CMD_PLAY_PAUSE, app->is_playing ? "play" : "pause");
            dial_app_show_toast(app, app->is_playing ? "Playback · Playing" : "Playback · Paused",
                                app->is_playing ? "play" : "pause",
                                dial_get_palette(app->mode)->moss, now_ms);
        } else if (ev->type == DIAL_EVENT_SWIPE_LEFT) {
            dial_media_next_track();
            app->is_playing = true;
            dial_hal_send_ipc(DIAL_CMD_NEXT_TRACK, "next");
            dial_app_show_toast(app, "Next track", "skip_fwd",
                                dial_get_palette(app->mode)->fjord, now_ms);
        } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_media_prev_track();
            app->is_playing = true;
            dial_hal_send_ipc(DIAL_CMD_PREV_TRACK, "prev");
            dial_app_show_toast(app, "Previous track", "skip_back",
                                dial_get_palette(app->mode)->fjord, now_ms);
        } else if (ev->type == DIAL_EVENT_NP_QUICK_ACTIONS) {
            /* Check if bottom '^' chevron (y >= 312) was tapped -> open NP-04 */
            if (ev->y >= 312.0f && fabsf(ev->x - 180.0f) <= 32.0f) {
                dial_app_set_screen(app, "NP-04");
            } else {
                dial_app_set_screen(app, "NP-02");
            }
        } else if (ev->type == DIAL_EVENT_BOTTOM_SWIPE_UP) {
            dial_app_set_screen(app, "NP-04");
        } else if (ev->type == DIAL_EVENT_RIM_DETENT) {
            app->volume = clamp_int(app->volume + ev->detent_steps * 2, 0, 100);
            dial_media_set_volume(app->volume);
            char vbuf[16];
            snprintf(vbuf, sizeof(vbuf), "%d", app->volume);
            dial_hal_send_ipc(DIAL_CMD_SET_VOLUME, vbuf);
            app->volume_hud_until_ms = now_ms + 800u;
            dial_app_set_screen(app, "NP-05");
        }
        return;
    }

    /* 5. Quick Actions (NP-02) */
    if (strcmp(scr, "NP-02") == 0) {
        if (ev->type == DIAL_EVENT_TAP) {
            /* Check 4 circular 52px action pucks at r = 118px: 234°, 198°, 162°, 126° */
            const float angles[4] = { 234.0f, 198.0f, 162.0f, 126.0f };
            for (int i = 0; i < 4; ++i) {
                float px = 0.0f, py = 0.0f;
                dial_polar_to_xy(angles[i], (float)DIAL_QA_ARC_RADIUS, &px, &py);
                if (hypotf(ev->x - px, ev->y - py) <= 28.0f) {
                    dial_hal_haptic_tick(12);
                    const dial_palette_t *pal = dial_get_palette(app->mode);
                    if (i == 0) {
                        app->is_favorited = !app->is_favorited;
                        dial_hal_send_ipc(DIAL_CMD_FAVORITE, app->is_favorited ? "1" : "0");
                        dial_app_show_toast(app,
                                            app->is_favorited ? "Added to Favorites" : "Removed from Favorites",
                                            "heart", pal->lingon, now_ms);
                    } else if (i == 1) {
                        app->is_shuffled = !app->is_shuffled;
                        dial_app_show_toast(app,
                                            app->is_shuffled ? "Shuffle · Enabled" : "Shuffle · Off",
                                            "shuffle", pal->moss, now_ms);
                    } else if (i == 2) {
                        dial_app_set_screen(app, "LIB-01");
                    } else if (i == 3) {
                        dial_app_set_screen(app, "NP-03");
                    }
                    return;
                }
            }
            /* Locked Rule #3: 1 touch anywhere else closes NP-02 back to NP-01 */
            dial_app_set_screen(app, "NP-01");
        }
        return;
    }

    /* 6. Synced Lyrics (NP-03), Track Details (NP-04), Volume HUD (NP-05) */
    if (strcmp(scr, "NP-03") == 0 || strcmp(scr, "NP-04") == 0) {
        if (ev->type == DIAL_EVENT_TAP ||
            ev->type == DIAL_EVENT_SWIPE_RIGHT ||
            ev->type == DIAL_EVENT_BOTTOM_SWIPE_UP) {
            dial_app_set_screen(app, "NP-01");
        }
        return;
    }
    if (strcmp(scr, "NP-05") == 0) {
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            app->volume = clamp_int(app->volume + ev->detent_steps * 2, 0, 100);
            dial_media_set_volume(app->volume);
            char vbuf[16];
            snprintf(vbuf, sizeof(vbuf), "%d", app->volume);
            dial_hal_send_ipc(DIAL_CMD_SET_VOLUME, vbuf);
            app->volume_hud_until_ms = now_ms + 800u;
        } else if (ev->type == DIAL_EVENT_TAP) {
            app->volume_hud_until_ms = 0;
            dial_app_set_screen(app, "NP-01");
        }
        return;
    }

    /* 7. Control Center (CC-01): 3x2 grid of 66px pucks */
    if (strcmp(scr, "CC-01") == 0) {
        const float col_cx[3] = { 93.0f, 180.0f, 267.0f };
        const float row_cy[2] = { 131.0f, 219.0f };
        const dial_palette_t *pal = dial_get_palette(app->mode);
        if (ev->type == DIAL_EVENT_TAP) {
            for (int r = 0; r < 2; ++r) {
                for (int c = 0; c < 3; ++c) {
                    if (hypotf(ev->x - col_cx[c], ev->y - row_cy[r]) <= 34.0f) {
                        dial_hal_haptic_tick(12);
                        int idx = r * 3 + c;
                        if (idx == 0) {
                            app->wifi_on = !app->wifi_on;
                            dial_app_show_toast(app, app->wifi_on ? "Wi-Fi · Studio 5G" : "Wi-Fi · Off",
                                                "wifi", pal->fjord, now_ms);
                        } else if (idx == 1) {
                            app->bt_on = !app->bt_on;
                            dial_hal_send_ipc(DIAL_CMD_BT_RECONNECT, app->bt_on ? "WH-1000XM5" : "off");
                            dial_app_show_toast(app, app->bt_on ? "BT · WH-1000XM5 (LDAC)" : "Bluetooth · Off",
                                                "bluetooth", pal->fjord, now_ms);
                        } else if (idx == 2) {
                            static const char *const kModeNames[6] = {
                                "Mode · Player", "Mode · USB DAC", "Mode · BT Audio",
                                "Mode · AirPlay", "Mode · HibyLink", "Mode · Storage"
                            };
                            app->active_mode_idx = (app->active_mode_idx + 1) % 6;
                            dial_app_show_toast(app, kModeNames[app->active_mode_idx],
                                                "disc", pal->moss, now_ms);
                        } else if (idx == 3) {
                            app->high_gain = !app->high_gain;
                            dial_dac_state_t dac;
                            dial_dac_init(&dac, (uint8_t)app->volume, !app->high_gain);
                            dial_dac_request_gain(&dac, app->high_gain, now_ms);
                            dial_dac_tick(&dac, now_ms + 10u);
                            dial_dac_tick(&dac, now_ms + 28u);
                            dial_app_show_toast(app,
                                                app->high_gain ? "CS43131 · High Gain (2.0V)" : "CS43131 · Low Gain (1.0V)",
                                                "gain", pal->cloudberry, now_ms);
                        } else if (idx == 4) {
                            app->usb_charge = !app->usb_charge;
                            dial_hal_send_ipc(DIAL_CMD_USB_CHARGE, app->usb_charge ? "on" : "off");
                            dial_app_show_toast(app,
                                                app->usb_charge ? "USB Host Charge · On" : "USB Host Charge · Off",
                                                "bolt", pal->moss, now_ms);
                        } else if (idx == 5) {
                            app->touch_locked = true;
                            app->unlock_hold_progress = 0.0f;
                            dial_app_set_screen(app, "CC-02");
                        }
                        return;
                    }
                }
            }
            /* Tap outside pucks closes CC-01 */
            dial_app_set_screen(app, app->prev_screen_id[0] ? app->prev_screen_id : "NP-01");
        } else if (ev->type == DIAL_EVENT_HOLD_450MS) {
            /* Locked Rule #5: Hold (450ms) on Wi-Fi, Bluetooth, or Mode opens submenu */
            if (hypotf(ev->x - col_cx[0], ev->y - row_cy[0]) <= 34.0f) {
                dial_app_set_screen(app, "SET-06");
            } else if (hypotf(ev->x - col_cx[1], ev->y - row_cy[0]) <= 34.0f) {
                dial_app_set_screen(app, "SET-07");
            } else if (hypotf(ev->x - col_cx[2], ev->y - row_cy[0]) <= 34.0f) {
                app->mode_picker_focus = app->active_mode_idx % 6;
                dial_app_set_screen(app, "MOD-00");
            }
        } else if (ev->type == DIAL_EVENT_BOTTOM_SWIPE_UP) {
            dial_app_set_screen(app, app->prev_screen_id[0] ? app->prev_screen_id : "NP-01");
        } else if (ev->type == DIAL_EVENT_RIM_DETENT) {
            if (ev->x >= 180.0f) {
                app->volume = clamp_int(app->volume + ev->detent_steps * 2, 0, 100);
                dial_media_set_volume(app->volume);
                char vbuf[16];
                snprintf(vbuf, sizeof(vbuf), "%d", app->volume);
                dial_hal_send_ipc(DIAL_CMD_SET_VOLUME, vbuf);
            } else {
                app->brightness = clamp_int(app->brightness + ev->detent_steps * 3, 5, 100);
            }
        }
        return;
    }

    /* 8. Home Carousel & Wheel (HOM-01, HOM-02, HOM-03) */
    if (strncmp(scr, "HOM-", 4) == 0) {
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            int next = (app->home_focus_idx + ev->detent_steps) % 6;
            if (next < 0) next += 6;
            app->home_focus_idx = next;
        } else if (ev->type == DIAL_EVENT_HOLD_450MS) {
            dial_app_set_screen(app, "HOM-03");
        } else if (ev->type == DIAL_EVENT_TAP && ev->r < 90.0f) {
            static const char *const G_HOME_TARGET[6] = {
                "NP-01", "LIB-01", "FIL-01", "MOD-00", "SET-01", "GAM-01"
            };
            dial_app_set_screen(app, G_HOME_TARGET[app->home_focus_idx % 6]);
        }
        return;
    }

    /* 9. Music Library Hub (LIB-01) */
    if (strcmp(scr, "LIB-01") == 0) {
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            app->list_focus_idx = clamp_int(app->list_focus_idx + ev->detent_steps, 0, 4);
        } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "HOM-01");
        } else if (ev->type == DIAL_EVENT_TAP && ev->r < 140.0f) {
            if (app->list_focus_idx == 2) {
                dial_app_set_screen(app, "LIB-04");
            } else if (app->list_focus_idx == 3) {
                dial_app_set_screen(app, "LIB-02");
            } else {
                dial_app_set_screen(app, "LIB-03");
            }
        }
        return;
    }

    /* 10. Albums Cover Arc (LIB-02) — Locked Rule #6: Horizontal Swipe AND Right Rim Scroll */
    if (strcmp(scr, "LIB-02") == 0) {
        int n_alb = (dial_media_is_loaded() && dial_media_get_album_count() > 0)
                        ? (int)dial_media_get_album_count()
                        : 6;
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            int next = (app->album_focus_idx + ev->detent_steps) % n_alb;
            if (next < 0) next += n_alb;
            app->album_focus_idx = next;
        } else if (ev->type == DIAL_EVENT_SWIPE_LEFT) {
            app->album_focus_idx = (app->album_focus_idx + 1) % n_alb;
            dial_hal_haptic_tick(10);
        } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            app->album_focus_idx = (app->album_focus_idx + n_alb - 1) % n_alb;
            dial_hal_haptic_tick(10);
        } else if (ev->type == DIAL_EVENT_TAP && ev->r < 95.0f) {
            if (dial_media_is_loaded()) {
                app->track_focus_idx = 0;
            }
            dial_app_set_screen(app, "LIB-03");
        }
        return;
    }

    /* 11. Track List (LIB-03) */
    if (strcmp(scr, "LIB-03") == 0) {
        int max_tr = 4;
        if (dial_media_is_loaded() && dial_media_get_album_count() > 0) {
            const dial_album_info_t *alb = dial_media_get_album((size_t)app->album_focus_idx);
            if (alb && alb->track_count > 0) {
                max_tr = (int)alb->track_count - 1;
            }
        }
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            app->track_focus_idx = clamp_int(app->track_focus_idx + ev->detent_steps, 0, max_tr);
        } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "LIB-02");
        } else if (ev->type == DIAL_EVENT_TAP) {
            if (ev->x < 46.0f && fabsf(ev->y - 180.0f) < 36.0f) {
                dial_app_set_screen(app, "LIB-02");
            } else if (ev->r < 140.0f) {
                app->playing_track_idx = app->track_focus_idx;
                app->is_playing = true;
                if (dial_media_is_loaded()) {
                    dial_media_play_track((size_t)app->album_focus_idx, (size_t)app->track_focus_idx);
                }
                dial_hal_send_ipc(DIAL_CMD_PLAY_PAUSE, "play");
                dial_app_set_screen(app, "NP-01");
            }
        }
        return;
    }

    /* 12. Artists Fast-Spin Letter Jump (LIB-04) */
    if (strcmp(scr, "LIB-04") == 0) {
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            app->artist_focus_idx = clamp_int(app->artist_focus_idx + ev->detent_steps, 0, 4);
            int ch = (int)app->artist_jump_letter + ev->detent_steps;
            if (ch < 'A') ch = 'A';
            if (ch > 'Z') ch = 'Z';
            app->artist_jump_letter = (char)ch;
        } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "LIB-01");
        } else if (ev->type == DIAL_EVENT_TAP && ev->r < 130.0f) {
            dial_app_set_screen(app, "LIB-02");
        }
        return;
    }

    /* 13. Browse Files (FIL-01) — Locked Rule #7: Long-press opens FIL-02 Action Sheet */
    if (strcmp(scr, "FIL-01") == 0) {
        int max_fs = (dial_media_is_loaded() && dial_media_get_fs_entry_count() > 0)
                         ? ((int)dial_media_get_fs_entry_count() - 1)
                         : 4;
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            app->file_focus_idx = clamp_int(app->file_focus_idx + ev->detent_steps, 0, max_fs);
        } else if (ev->type == DIAL_EVENT_HOLD_450MS) {
            dial_app_set_screen(app, "FIL-02");
        } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "HOM-01");
        } else if (ev->type == DIAL_EVENT_TAP && ev->r < 140.0f) {
            if (dial_media_is_loaded() && dial_media_get_fs_entry_count() > 0) {
                const dial_fs_entry_t *ent = dial_media_get_fs_entry((size_t)app->file_focus_idx);
                if (ent) {
                    if (ent->is_dir) {
                        dial_media_browse_dir(ent->full_path);
                        app->file_focus_idx = 0;
                    } else {
                        dial_media_play_file(ent->full_path, ent->name, "Au5", dial_media_get_current_dir_label());
                        app->is_playing = true;
                        dial_app_set_screen(app, "NP-01");
                    }
                }
            } else {
                if (app->file_focus_idx == 0) {
                    dial_app_set_screen(app, "FIL-04");
                } else if (app->file_focus_idx == 2 || app->file_focus_idx == 3) {
                    dial_app_set_screen(app, "NP-01");
                }
            }
        }
        return;
    }

    /* 14. File & Folder Action Sheet (FIL-02) */
    if (strcmp(scr, "FIL-02") == 0) {
        if (ev->type == DIAL_EVENT_TAP) {
            /* 3 action pucks: Edit tags (cx=106), Move (cx=180), Delete (cx=254) at cy=246 */
            if (hypotf(ev->x - 254.0f, ev->y - 246.0f) <= 32.0f) {
                app->delete_hold_progress = 0.0f;
                app->file_deleted = false;
                dial_app_set_screen(app, "FIL-03");
            } else {
                dial_app_set_screen(app, "FIL-01");
            }
        }
        return;
    }

    /* 15. Delete Confirmation (FIL-03) — Locked Rule #7: 600ms hold on Lingon button */
    if (strcmp(scr, "FIL-03") == 0) {
        if (ev->type == DIAL_EVENT_HOLD_600MS && ev->x >= 165.0f && ev->y >= 200.0f && ev->y <= 290.0f) {
            app->file_deleted = true;
            app->delete_hold_progress = 1.0f;
            dial_hal_send_ipc(DIAL_CMD_DELETE_FILE, "03 Teardrop.flac");
            dial_app_show_toast(app, "Deleted · 03 Teardrop.flac", "trash",
                                dial_get_palette(app->mode)->lingon, now_ms);
            dial_app_set_screen(app, "FIL-01");
        } else if (ev->type == DIAL_EVENT_TAP && ev->x < 165.0f && ev->y >= 200.0f && ev->y <= 290.0f) {
            app->file_deleted = false;
            app->delete_hold_progress = 0.0f;
            dial_app_set_screen(app, "FIL-01");
        }
        return;
    }

    /* 16. Empty Folder State (FIL-04) */
    if (strcmp(scr, "FIL-04") == 0) {
        if (ev->type == DIAL_EVENT_TAP || ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "FIL-01");
        }
        return;
    }

    /* 17. Operating Modes Picker & Dashboards (MOD-00 .. MOD-06) */
    if (strcmp(scr, "MOD-00") == 0) {
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            int next = (app->mode_picker_focus + ev->detent_steps) % 6;
            if (next < 0) next += 6;
            app->mode_picker_focus = next;
        } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "HOM-01");
        } else if (ev->type == DIAL_EVENT_TAP) {
            if (ev->r < 72.0f) {
                app->active_mode_idx = app->mode_picker_focus % 6;
                char target[16];
                snprintf(target, sizeof(target), "MOD-0%d", (app->active_mode_idx % 6) + 1);
                dial_app_set_screen(app, target);
            } else {
                for (int i = 0; i < 6; ++i) {
                    float px = 0.0f, py = 0.0f;
                    dial_polar_to_xy((float)(i * 60), 116.0f, &px, &py);
                    if (hypotf(ev->x - px, ev->y - py) <= 30.0f) {
                        app->mode_picker_focus = i;
                        app->active_mode_idx   = i;
                        char target[16];
                        snprintf(target, sizeof(target), "MOD-0%d", i + 1);
                        dial_app_set_screen(app, target);
                        return;
                    }
                }
            }
        }
        return;
    }
    if (strncmp(scr, "MOD-0", 5) == 0) {
        if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "MOD-00");
        } else if (ev->type == DIAL_EVENT_TAP && strcmp(scr, "MOD-02") == 0) {
            app->usb_charge = !app->usb_charge;
            dial_hal_send_ipc(DIAL_CMD_USB_CHARGE, app->usb_charge ? "on" : "off");
            dial_app_show_toast(app,
                                app->usb_charge ? "USB Host Power · Enabled" : "USB Host Power · Isolated",
                                "bolt", dial_get_palette(app->mode)->moss, now_ms);
        }
        return;
    }

    /* 18. Settings Screens (SET-01 .. SET-07) */
    if (strcmp(scr, "SET-01") == 0) {
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            app->settings_focus_idx = clamp_int(app->settings_focus_idx + ev->detent_steps, 0, 4);
        } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "HOM-01");
        } else if (ev->type == DIAL_EVENT_TAP && ev->r < 140.0f) {
            static const char *const G_SET_TARGETS[5] = {
                "SET-02", "SET-05", "SET-07", "SET-06", "OVL-03"
            };
            dial_app_set_screen(app, G_SET_TARGETS[app->settings_focus_idx % 5]);
        }
        return;
    }
    if (strcmp(scr, "SET-02") == 0) {
        const dial_palette_t *pal = dial_get_palette(app->mode);
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            app->audio_focus_idx = clamp_int(app->audio_focus_idx + ev->detent_steps, 0, 4);
        } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "SET-01");
        } else if (ev->type == DIAL_EVENT_TAP && ev->r < 140.0f) {
            int row = app->audio_focus_idx;
            if (ev->y < 115.0f)      row = 0;
            else if (ev->y < 170.0f) row = 1;
            else if (ev->y < 225.0f) row = 2;
            else if (ev->y < 280.0f) row = 3;
            else                     row = 4;
            app->audio_focus_idx = row;

            if (row == 0) {
                dial_app_set_screen(app, "SET-03");
            } else if (row == 1) {
                app->high_gain = !app->high_gain;
                dial_dac_state_t dac;
                dial_dac_init(&dac, (uint8_t)app->volume, !app->high_gain);
                dial_dac_request_gain(&dac, app->high_gain, now_ms);
                dial_dac_tick(&dac, now_ms + 10u);
                dial_dac_tick(&dac, now_ms + 28u);
                dial_app_show_toast(app,
                                    app->high_gain ? "CS43131 · High Gain (2.0 Vrms)" : "CS43131 · Low Gain (1.0 Vrms)",
                                    "gain", pal->cloudberry, now_ms);
            } else if (row == 2) {
                dial_app_set_screen(app, "SET-04");
            } else if (row == 3) {
                app->gapless_on = !app->gapless_on;
                dial_app_show_toast(app,
                                    app->gapless_on ? "Gapless playback · On" : "Gapless playback · Off",
                                    "check", pal->moss, now_ms);
            } else if (row == 4) {
                dial_app_show_toast(app, "L/R Balance · 0.0 dB Center", "sliders", pal->fjord, now_ms);
            }
        }
        return;
    }
    if (strcmp(scr, "SET-03") == 0) {
        static const char *const kFilterNames[5] = {
            "Fast roll-off · linear",
            "Slow roll-off · linear",
            "Fast roll-off · min phase",
            "Slow roll-off · min phase",
            "NOS · non-oversampling"
        };
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            app->dac_filter_idx = clamp_int(app->dac_filter_idx + ev->detent_steps, 0, 4);
            char fbuf[8];
            snprintf(fbuf, sizeof(fbuf), "%d", app->dac_filter_idx);
            dial_hal_send_ipc(DIAL_CMD_DAC_FILTER, fbuf);
            dial_app_show_toast(app, kFilterNames[app->dac_filter_idx],
                                "wave", dial_get_palette(app->mode)->moss, now_ms);
        } else if (ev->type == DIAL_EVENT_TAP && ev->r < 140.0f) {
            int row = clamp_int((int)((ev->y - 64.0f) / 52.0f), 0, 4);
            app->dac_filter_idx = row;
            char fbuf[8];
            snprintf(fbuf, sizeof(fbuf), "%d", app->dac_filter_idx);
            dial_hal_send_ipc(DIAL_CMD_DAC_FILTER, fbuf);
            dial_app_show_toast(app, kFilterNames[app->dac_filter_idx],
                                "wave", dial_get_palette(app->mode)->moss, now_ms);
            dial_app_set_screen(app, "SET-02");
        } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "SET-02");
        }
        return;
    }
    if (strcmp(scr, "SET-04") == 0) {
        static const char *const kFreqs[10] = {
            "31 Hz", "62 Hz", "125 Hz", "250 Hz", "500 Hz",
            "1 kHz", "2 kHz", "4 kHz", "8 kHz", "16 kHz"
        };
        static const float kPresets[4][10] = {
            { 0.52f, 0.40f, 0.32f, 0.55f, 0.62f, 0.58f, 0.44f, 0.38f, 0.46f, 0.54f }, /* Nordic Warm */
            { 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f, 0.50f }, /* Studio Flat */
            { 0.25f, 0.28f, 0.36f, 0.50f, 0.54f, 0.52f, 0.48f, 0.44f, 0.40f, 0.42f }, /* Fjord Bass  */
            { 0.56f, 0.54f, 0.50f, 0.44f, 0.36f, 0.32f, 0.34f, 0.40f, 0.46f, 0.50f }  /* Vocal Air   */
        };
        static const char *const kPresetLabels[4] = {
            "EQ · Nordic Warm", "EQ · Studio Flat", "EQ · Fjord Bass", "EQ · Vocal Air"
        };
        const dial_palette_t *pal = dial_get_palette(app->mode);

        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            int b = clamp_int(app->eq_focus_band, 0, 9);
            float v = app->eq_band_norm[b] - (float)ev->detent_steps * 0.04f;
            if (v < 0.10f) v = 0.10f;
            if (v > 0.90f) v = 0.90f;
            app->eq_band_norm[b] = v;
            float db = (0.50f - v) * 12.0f;
            char msg[48];
            snprintf(msg, sizeof(msg), "%s · %+.1f dB", kFreqs[b], db);
            dial_hal_send_ipc(DIAL_CMD_EQ_PRESET, msg);
            dial_app_show_toast(app, msg, "eq", pal->moss, now_ms);
        } else if (ev->type == DIAL_EVENT_TAP) {
            if (ev->y < 98.0f) {
                app->eq_preset_idx = (app->eq_preset_idx + 1) % 4;
                for (int i = 0; i < 10; ++i) {
                    app->eq_band_norm[i] = kPresets[app->eq_preset_idx][i];
                }
                dial_hal_send_ipc(DIAL_CMD_EQ_PRESET, kPresetLabels[app->eq_preset_idx]);
                dial_app_show_toast(app, kPresetLabels[app->eq_preset_idx], "eq", pal->moss, now_ms);
            } else if (ev->y >= 165.0f && ev->y <= 285.0f && ev->x >= 48.0f && ev->x <= 312.0f) {
                int band = clamp_int((int)((ev->x - 54.0f) / 25.0f), 0, 9);
                app->eq_focus_band = band;
                float norm = (ev->y - 178.0f) / 82.0f;
                if (norm < 0.10f) norm = 0.10f;
                if (norm > 0.90f) norm = 0.90f;
                app->eq_band_norm[band] = norm;
                float db = (0.50f - norm) * 12.0f;
                char msg[48];
                snprintf(msg, sizeof(msg), "%s · %+.1f dB", kFreqs[band], db);
                dial_hal_send_ipc(DIAL_CMD_EQ_PRESET, msg);
                dial_app_show_toast(app, msg, "eq", pal->moss, now_ms);
            }
        } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "SET-02");
        }
        return;
    }
    if (strcmp(scr, "SET-05") == 0) {
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            app->brightness = clamp_int(app->brightness + ev->detent_steps * 4, 5, 100);
        } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "SET-01");
        } else if (ev->type == DIAL_EVENT_TAP) {
            app->mode = (app->mode == DIAL_MODE_NIGHT) ? DIAL_MODE_DAY : DIAL_MODE_NIGHT;
            dial_app_show_toast(app,
                                app->mode == DIAL_MODE_NIGHT ? "Theme · Polar Night" : "Theme · Birch Day",
                                "sun", dial_get_palette(app->mode)->moss, now_ms);
        }
        return;
    }
    if (strcmp(scr, "SET-06") == 0) {
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            app->wifi_focus_idx = clamp_int(app->wifi_focus_idx + ev->detent_steps, 0, 4);
        } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "SET-01");
        } else if (ev->type == DIAL_EVENT_TAP && ev->r < 140.0f) {
            app->wifi_on = !app->wifi_on;
            dial_app_show_toast(app,
                                app->wifi_on ? "Wi-Fi · Connected (Studio 5G)" : "Wi-Fi · Disabled",
                                "wifi", dial_get_palette(app->mode)->fjord, now_ms);
        }
        return;
    }
    if (strcmp(scr, "SET-07") == 0) {
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            app->bt_focus_idx = clamp_int(app->bt_focus_idx + ev->detent_steps, 0, 4);
        } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "SET-01");
        } else if (ev->type == DIAL_EVENT_TAP && ev->r < 140.0f) {
            if (ev->y < 115.0f) {
                app->bt_on = !app->bt_on;
                dial_app_show_toast(app,
                                    app->bt_on ? "Bluetooth · Enabled" : "Bluetooth · Disabled",
                                    "bluetooth", dial_get_palette(app->mode)->fjord, now_ms);
            } else {
                dial_hal_send_ipc(DIAL_CMD_BT_RECONNECT, "WH-1000XM5");
                dial_app_show_toast(app, "Fast-Cache · WH-1000XM5 (LDAC 990)",
                                    "headphones", dial_get_palette(app->mode)->moss, now_ms);
            }
        }
        return;
    }

    /* 19. Rotary Arcade & Overlays (GAM-01..04, OVL-01..04) */
    if (strcmp(scr, "GAM-01") == 0) {
        if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "HOM-01");
        } else if (ev->type == DIAL_EVENT_TAP) {
            if (ev->y < 158.0f)      dial_app_set_screen(app, "GAM-02");
            else if (ev->y < 208.0f) dial_app_set_screen(app, "GAM-03");
            else                     dial_app_set_screen(app, "GAM-04");
        }
        return;
    }
    if (strcmp(scr, "GAM-02") == 0) {
        if (ev->type == DIAL_EVENT_RIM_DETENT) {
            app->pong_paddle_deg = dial_normalize_deg(app->pong_paddle_deg + (float)ev->detent_steps * 8.0f);
        } else if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "GAM-01");
        }
        return;
    }
    if (strcmp(scr, "GAM-03") == 0 || strcmp(scr, "GAM-04") == 0) {
        if (ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "GAM-01");
        }
        return;
    }
    if (strncmp(scr, "OVL-", 4) == 0) {
        if (ev->type == DIAL_EVENT_TAP || ev->type == DIAL_EVENT_SWIPE_RIGHT) {
            dial_app_set_screen(app, "HOM-01");
        }
        return;
    }
}

dial_gesture_event_t dial_app_on_touch(dial_app_t *app, const dial_touch_event_t *touch) {
    dial_gesture_event_t ev;
    memset(&ev, 0, sizeof(ev));
    if (!app || !touch) return ev;

    ev = dial_gesture_fsm_step(&app->fsm, touch);
    handle_disambiguated_event(app, &ev, touch->timestamp_ms);

    if (strcmp(app->screen_id, "CC-02") == 0 && !touch->pressed) {
        app->unlock_hold_progress = 0.0f;
    }
    if (strcmp(app->screen_id, "FIL-03") == 0 && !touch->pressed) {
        app->delete_hold_progress = 0.0f;
    }
    return ev;
}

dial_gesture_event_t dial_app_tick(dial_app_t *app, uint32_t now_ms) {
    dial_gesture_event_t ev;
    memset(&ev, 0, sizeof(ev));
    if (!app) return ev;

    /* Update live 600ms hold progress on CC-02 Touch Lock Shield or FIL-03 Hold to Delete */
    if (strcmp(app->screen_id, "CC-02") == 0 && app->fsm.touch_active) {
        uint32_t held = (now_ms > app->fsm.start_ts_ms) ? (now_ms - app->fsm.start_ts_ms) : 0u;
        float p = (float)held / (float)DIAL_HOLD_CONFIRM_MS;
        if (p > 1.0f) p = 1.0f;
        app->unlock_hold_progress = p;
    } else if (strcmp(app->screen_id, "FIL-03") == 0 && app->fsm.touch_active && app->fsm.last_x >= 165.0f) {
        uint32_t held = (now_ms > app->fsm.start_ts_ms) ? (now_ms - app->fsm.start_ts_ms) : 0u;
        float p = (float)held / (float)DIAL_HOLD_CONFIRM_MS;
        if (p > 1.0f) p = 1.0f;
        app->delete_hold_progress = p;
    }

    /* Auto-dismiss NP-05 Volume HUD after 800 ms */
    if (strcmp(app->screen_id, "NP-05") == 0 &&
        app->volume_hud_until_ms > 0 &&
        now_ms >= app->volume_hud_until_ms) {
        app->volume_hud_until_ms = 0;
        dial_app_set_screen(app, "NP-01");
    }

    /* Auto-dismiss non-blocking live toast pill */
    if (app->toast_until_ms > 0 && now_ms >= app->toast_until_ms) {
        app->toast_until_ms = 0;
    }

    /* Sync real hardware/CoreAudio playback progress onto r=38px vinyl seek ring */
    if (dial_media_is_loaded() && app->is_playing && !app->fsm.touch_active) {
        float live_prog = dial_media_get_progress(NULL, NULL);
        if (live_prog >= 0.0f && live_prog <= 0.999f) {
            app->seek_progress       = live_prog;
            app->fsm.current_seek_deg = live_prog * 360.0f;
        }
    }

    ev = dial_gesture_fsm_tick(&app->fsm, now_ms);
    handle_disambiguated_event(app, &ev, now_ms);
    return ev;
}

void dial_app_on_key(dial_app_t *app, const dial_key_event_t *key, const dial_fb_t *fb) {
    if (!app || !key || !key->pressed) return;

    switch (key->key) {
        case DIAL_KEY_PLAYPAUSE:
            app->is_playing = !app->is_playing;
            dial_media_set_playing(app->is_playing);
            dial_hal_send_ipc(DIAL_CMD_PLAY_PAUSE, app->is_playing ? "play" : "pause");
            dial_app_show_toast(app, app->is_playing ? "Playback · Playing" : "Playback · Paused",
                                app->is_playing ? "play" : "pause",
                                dial_get_palette(app->mode)->moss, key->timestamp_ms);
            break;
        case DIAL_KEY_VOLUP:
            app->volume = clamp_int(app->volume + 2, 0, 100);
            dial_media_set_volume(app->volume);
            {
                char vbuf[16];
                snprintf(vbuf, sizeof(vbuf), "%d", app->volume);
                dial_hal_send_ipc(DIAL_CMD_SET_VOLUME, vbuf);
            }
            if (strcmp(app->screen_id, "NP-01") == 0 || strcmp(app->screen_id, "NP-05") == 0) {
                app->volume_hud_until_ms = key->timestamp_ms + 800u;
                dial_app_set_screen(app, "NP-05");
            }
            break;
        case DIAL_KEY_VOLDOWN:
            app->volume = clamp_int(app->volume - 2, 0, 100);
            dial_media_set_volume(app->volume);
            {
                char vbuf[16];
                snprintf(vbuf, sizeof(vbuf), "%d", app->volume);
                dial_hal_send_ipc(DIAL_CMD_SET_VOLUME, vbuf);
            }
            if (strcmp(app->screen_id, "NP-01") == 0 || strcmp(app->screen_id, "NP-05") == 0) {
                app->volume_hud_until_ms = key->timestamp_ms + 800u;
                dial_app_set_screen(app, "NP-05");
            }
            break;
        case DIAL_KEY_POWER:
            if (strncmp(app->screen_id, "LCK-", 4) == 0) {
                dial_app_set_screen(app, "NP-01");
            } else {
                dial_app_set_screen(app, app->is_playing ? "LCK-02" : "LCK-01");
            }
            break;
        case DIAL_KEY_TOGGLE_THEME:
            app->mode = (app->mode == DIAL_MODE_NIGHT) ? DIAL_MODE_DAY : DIAL_MODE_NIGHT;
            dial_app_show_toast(app,
                                app->mode == DIAL_MODE_NIGHT ? "Theme · Polar Night" : "Theme · Birch Day",
                                "sun", dial_get_palette(app->mode)->moss, key->timestamp_ms);
            break;
        case DIAL_KEY_SCREENSHOT:
            if (fb) {
                dial_hal_save_png(fb, "/tmp/dialos_live.png");
                printf("[DialOS] Saved live screenshot -> /tmp/dialos_live.png\n");
            }
            break;
        case DIAL_KEY_NEXT_SCREEN: {
            dial_gesture_event_t sev = { .type = DIAL_EVENT_SWIPE_LEFT };
            handle_disambiguated_event(app, &sev, key->timestamp_ms);
            break;
        }
        case DIAL_KEY_PREV_SCREEN: {
            dial_gesture_event_t sev = { .type = DIAL_EVENT_SWIPE_RIGHT };
            handle_disambiguated_event(app, &sev, key->timestamp_ms);
            break;
        }
        default:
            break;
    }
}

void dial_app_render(dial_app_t *app, dial_fb_t *fb, uint32_t tick_ms) {
    if (!app || !fb) return;
    /* Keep global singleton synced if caller passed a custom dial_app_t */
    if (app != &g_app) {
        g_app = *app;
        g_app_inited = true;
    }
    const dial_palette_t *pal = dial_get_palette(app->mode);
    const dial_screen_desc_t *desc = dial_screen_find(app->screen_id);
    if (desc && desc->render) {
        desc->render(fb, pal, tick_ms);
    } else {
        dial_screen_render_by_id(fb, "NP-01", app->mode, tick_ms);
    }

    /* Render non-blocking Fjäll live feedback toast banner when active */
    if (app->toast_until_ms > 0 && app->toast_text[0] != '\0') {
        uint32_t tone = app->toast_tone ? app->toast_tone : pal->moss;
        dial_draw_round_rect(fb, 48.0f, 38.0f, 264.0f, 36.0f, 18.0f,
                             pal->surface_2, 245, 1.0f, tone, 175);
        dial_draw_circle_fill(fb, 68.0f, 56.0f, 11.0f,
                              dial_color_lerp(pal->surface_2, tone, 60), 255);
        dial_draw_icon_by_name(fb, app->toast_icon[0] ? app->toast_icon : "check",
                               61.0f, 49.0f, 14.0f, 1.8f, tone, 255);
        dial_draw_text(fb, app->toast_text, 86, 49, 12, pal->text_1, 255, false);
        dial_draw_circle_mask(fb);
    }
}

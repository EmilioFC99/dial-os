#include "dial_app.h"
#include "dial_screens.h"
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
        } else if (ev->type == DIAL_EVENT_PLAY_PAUSE) {
            app->is_playing = !app->is_playing;
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
                    if (i == 0) {
                        app->is_favorited = !app->is_favorited;
                        dial_hal_send_ipc(DIAL_CMD_FAVORITE, app->is_favorited ? "1" : "0");
                    } else if (i == 1) {
                        app->is_shuffled = !app->is_shuffled;
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
            app->volume_hud_until_ms = now_ms + 800u;
        } else if (ev->type == DIAL_EVENT_TAP) {
            app->volume_hud_until_ms = 0;
            dial_app_set_screen(app, "NP-01");
        }
        return;
    }

    /* 7. Control Center (CC-01): 3x2 grid of 66px pucks */
    if (strcmp(scr, "CC-01") == 0) {
        if (ev->type == DIAL_EVENT_TAP) {
            const float col_cx[3] = { 93.0f, 180.0f, 267.0f };
            const float row_cy[2] = { 131.0f, 219.0f };
            for (int r = 0; r < 2; ++r) {
                for (int c = 0; c < 3; ++c) {
                    if (hypotf(ev->x - col_cx[c], ev->y - row_cy[r]) <= 34.0f) {
                        dial_hal_haptic_tick(12);
                        int idx = r * 3 + c;
                        if (idx == 0)      app->wifi_on = !app->wifi_on;
                        else if (idx == 1) app->bt_on   = !app->bt_on;
                        else if (idx == 2) app->active_mode_idx = (app->active_mode_idx + 1) % 6;
                        else if (idx == 3) app->high_gain = !app->high_gain;
                        else if (idx == 4) app->usb_charge = !app->usb_charge;
                        else if (idx == 5) {
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
        } else if (ev->type == DIAL_EVENT_BOTTOM_SWIPE_UP) {
            dial_app_set_screen(app, app->prev_screen_id[0] ? app->prev_screen_id : "NP-01");
        } else if (ev->type == DIAL_EVENT_RIM_DETENT) {
            if (ev->x >= 180.0f) {
                app->volume = clamp_int(app->volume + ev->detent_steps * 2, 0, 100);
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
            if (app->home_focus_idx == 0) {
                dial_app_set_screen(app, "NP-01");
            } else if (app->home_focus_idx == 1) {
                dial_app_set_screen(app, "LIB-01");
            }
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
            dial_app_set_screen(app, "NP-01");
        }
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
    return ev;
}

dial_gesture_event_t dial_app_tick(dial_app_t *app, uint32_t now_ms) {
    dial_gesture_event_t ev;
    memset(&ev, 0, sizeof(ev));
    if (!app) return ev;

    /* Update live 600ms hold progress on CC-02 Touch Lock Shield */
    if (strcmp(app->screen_id, "CC-02") == 0 && app->fsm.touch_active) {
        uint32_t held = (now_ms > app->fsm.start_ts_ms) ? (now_ms - app->fsm.start_ts_ms) : 0u;
        float p = (float)held / (float)DIAL_HOLD_CONFIRM_MS;
        if (p > 1.0f) p = 1.0f;
        app->unlock_hold_progress = p;
    }

    /* Auto-dismiss NP-05 Volume HUD after 800 ms */
    if (strcmp(app->screen_id, "NP-05") == 0 &&
        app->volume_hud_until_ms > 0 &&
        now_ms >= app->volume_hud_until_ms) {
        app->volume_hud_until_ms = 0;
        dial_app_set_screen(app, "NP-01");
    }

    ev = dial_gesture_fsm_tick(&app->fsm, now_ms);
    handle_disambiguated_event(app, &ev, now_ms);
    return ev;
}

void dial_app_on_key(dial_app_t *app, const dial_key_event_t *key, const dial_fb_t *fb) {
    if (!app || !key || !key->pressed) return;

    size_t screen_count = 0;
    const dial_screen_desc_t *screens = dial_screen_get_all(&screen_count);

    switch (key->key) {
        case DIAL_KEY_PLAYPAUSE:
            app->is_playing = !app->is_playing;
            dial_hal_send_ipc(DIAL_CMD_PLAY_PAUSE, app->is_playing ? "play" : "pause");
            break;
        case DIAL_KEY_VOLUP:
            app->volume = clamp_int(app->volume + 2, 0, 100);
            if (strcmp(app->screen_id, "NP-01") == 0 || strcmp(app->screen_id, "NP-05") == 0) {
                app->volume_hud_until_ms = key->timestamp_ms + 800u;
                dial_app_set_screen(app, "NP-05");
            }
            break;
        case DIAL_KEY_VOLDOWN:
            app->volume = clamp_int(app->volume - 2, 0, 100);
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
            break;
        case DIAL_KEY_SCREENSHOT:
            if (fb) {
                dial_hal_save_png(fb, "/tmp/dialos_live.png");
                printf("[DialOS] Saved live screenshot -> /tmp/dialos_live.png\n");
            }
            break;
        case DIAL_KEY_NEXT_SCREEN:
        case DIAL_KEY_PREV_SCREEN: {
            size_t cur_idx = 0;
            for (size_t i = 0; i < screen_count; ++i) {
                if (strcmp(screens[i].id, app->screen_id) == 0) {
                    cur_idx = i;
                    break;
                }
            }
            size_t next_idx = (key->key == DIAL_KEY_NEXT_SCREEN)
                                  ? (cur_idx + 1) % screen_count
                                  : (cur_idx + screen_count - 1) % screen_count;
            dial_app_set_screen(app, screens[next_idx].id);
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
}

#ifndef DIAL_SCREENS_H
#define DIAL_SCREENS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dial_config.h"
#include "dial_theme.h"
#include "dial_hal.h"
#include "dial_draw.h"
#include "dial_icons.h"

typedef void (*dial_screen_render_fn)(dial_fb_t *fb,
                                      const dial_palette_t *pal,
                                      uint32_t tick_ms);

typedef struct {
    const char           *id;     /* e.g. "SYS-00", "NP-01", "CC-01" */
    const char           *group;  /* "core", "np", "lib", "files", "modes", "settings", "games", "overlays" */
    const char           *title;
    dial_screen_render_fn render;
} dial_screen_desc_t;

/* Look up a registered screen by ID (e.g. "NP-01", "CC-01") */
const dial_screen_desc_t *dial_screen_find(const char *screen_id);

/* Return the table of registered screens and its count */
const dial_screen_desc_t *dial_screen_get_all(size_t *out_count);

/* Render a screen by ID (falls back to NP-01 if not found) */
bool dial_screen_render_by_id(dial_fb_t *fb,
                              const char *screen_id,
                              dial_mode_t mode,
                              uint32_t tick_ms);

#endif /* DIAL_SCREENS_H */

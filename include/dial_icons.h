#ifndef DIAL_ICONS_H
#define DIAL_ICONS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dial_hal.h"

/* Complete Registry of 70 Fjäll Vector Icons (24x24 grid, 1.75px stroke, round caps) */
typedef enum {
    /* Transport (8) */
    DIAL_ICON_PLAY = 0,
    DIAL_ICON_PAUSE,
    DIAL_ICON_NEXT,
    DIAL_ICON_PREV,
    DIAL_ICON_SHUFFLE,
    DIAL_ICON_REPEAT,
    DIAL_ICON_VOLUME,
    DIAL_ICON_RINGS,
    /* Library (9) */
    DIAL_ICON_NOTE,
    DIAL_ICON_ALBUM,
    DIAL_ICON_ARTIST,
    DIAL_ICON_PLAYLIST,
    DIAL_ICON_GENRE,
    DIAL_ICON_LEAF,
    DIAL_ICON_HEART,
    DIAL_ICON_MIC,
    DIAL_ICON_LYRICS,
    /* Files (6) */
    DIAL_ICON_FOLDER,
    DIAL_ICON_FILEAUDIO,
    DIAL_ICON_SDCARD,
    DIAL_ICON_EDIT,
    DIAL_ICON_MOVE,
    DIAL_ICON_TRASH,
    /* Modes & Connectivity (16) */
    DIAL_ICON_MODES,
    DIAL_ICON_MODE_PLAYER,
    DIAL_ICON_MODE_USBDAC,
    DIAL_ICON_MODE_TRANSPORT,
    DIAL_ICON_MODE_BTRX,
    DIAL_ICON_MODE_AIRPLAY,
    DIAL_ICON_MODE_TRANSFER,
    DIAL_ICON_USB,
    DIAL_ICON_USB_CHARGE,
    DIAL_ICON_BROADCAST,
    DIAL_ICON_BLUETOOTH,
    DIAL_ICON_AIRPLAY,
    DIAL_ICON_TRANSFER,
    DIAL_ICON_WIFI,
    DIAL_ICON_HEADPHONES,
    DIAL_ICON_CHIP,
    /* Settings (17) */
    DIAL_ICON_TUNE,
    DIAL_ICON_EQ,
    DIAL_ICON_FILTER,
    DIAL_ICON_GAIN,
    DIAL_ICON_GAIN_LOW,
    DIAL_ICON_GAIN_HIGH,
    DIAL_ICON_CONTRAST,
    DIAL_ICON_SUN,
    DIAL_ICON_MOON,
    DIAL_ICON_BATTERY,
    DIAL_ICON_POWER,
    DIAL_ICON_LOCK,
    DIAL_ICON_TIMER,
    DIAL_ICON_INFO,
    DIAL_ICON_GAMES,
    DIAL_ICON_APPS,
    /* Utility (11) */
    DIAL_ICON_BACK,
    DIAL_ICON_CHEV,
    DIAL_ICON_UP,
    DIAL_ICON_DOWN,
    DIAL_ICON_CHECK,
    DIAL_ICON_CLOSE,
    DIAL_ICON_PLUS,
    DIAL_ICON_SEARCH,
    DIAL_ICON_MORE,
    DIAL_ICON_RIM,
    DIAL_ICON_WARNING,
    /* Nature / Brand (4) */
    DIAL_ICON_FJALL,
    DIAL_ICON_PINE,
    DIAL_ICON_PEBBLE,
    DIAL_ICON_SNOW,

    DIAL_ICON_COUNT /* Must equal 70 */
} dial_icon_id_t;

typedef struct {
    dial_icon_id_t id;
    const char    *name;
    const char    *subpaths[10];
    size_t         subpath_count;
} dial_icon_def_t;

/* Look up icon definition by ID or by string name (e.g. "lyrics", "mode_usbdac") */
const dial_icon_def_t *dial_icon_get(dial_icon_id_t id);
const dial_icon_def_t *dial_icon_by_name(const char *name);

/* Rasterize a Fjäll vector icon at top-left (x, y) with pixel box `size_px` and stroke `stroke_w` (default 1.75f) */
void dial_draw_icon(dial_fb_t *fb,
                    dial_icon_id_t id,
                    float x,
                    float y,
                    float size_px,
                    float stroke_w,
                    uint32_t rgb,
                    uint8_t alpha);

void dial_draw_icon_by_name(dial_fb_t *fb,
                            const char *name,
                            float x,
                            float y,
                            float size_px,
                            float stroke_w,
                            uint32_t rgb,
                            uint8_t alpha);

#endif /* DIAL_ICONS_H */

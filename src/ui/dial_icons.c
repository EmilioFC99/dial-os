#include "dial_icons.h"
#include "dial_draw.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Exact 70 Fjäll Icons from assets/nordic/fjall-icons.svg & fjall-design-system.html */
static const dial_icon_def_t G_FJALL_ICONS[DIAL_ICON_COUNT] = {
    /* Transport (8) */
    { DIAL_ICON_PLAY, "play", {
        "F:M8 5.6v12.8a1 1 0 0 0 1.53.85l10.1-6.4a1 1 0 0 0 0-1.7L9.53 4.75A1 1 0 0 0 8 5.6z"
    }, 1 },
    { DIAL_ICON_PAUSE, "pause", {
        "F:M7 5h3a.9.9 0 0 1 .9.9v12.2a.9.9 0 0 1-.9.9H7a.9.9 0 0 1-.9-.9V5.9A.9.9 0 0 1 7 5z",
        "F:M14 5h3a.9.9 0 0 1 .9.9v12.2a.9.9 0 0 1-.9.9h-3a.9.9 0 0 1-.9-.9V5.9A.9.9 0 0 1 14 5z"
    }, 2 },
    { DIAL_ICON_NEXT, "next", {
        "M5.5 6.6v10.8a.8.8 0 0 0 1.25.66l7.4-5.4a.8.8 0 0 0 0-1.32l-7.4-5.4A.8.8 0 0 0 5.5 6.6z",
        "M18.5 6v12"
    }, 2 },
    { DIAL_ICON_PREV, "prev", {
        "M18.5 6.6v10.8a.8.8 0 0 1-1.25.66l-7.4-5.4a.8.8 0 0 1 0-1.32l7.4-5.4a.8.8 0 0 1 1.25.66z",
        "M5.5 6v12"
    }, 2 },
    { DIAL_ICON_SHUFFLE, "shuffle", {
        "M3 7h3c2 0 3.2 1 4.2 2.7l2.6 4.6C13.8 16 15 17 17 17h4",
        "M18.5 14.5 21 17l-2.5 2.5",
        "M3 17h3c1.3 0 2.3-.4 3.1-1.2",
        "M14.9 8.2C15.7 7.4 16.7 7 18 7h3",
        "M18.5 4.5 21 7l-2.5 2.5"
    }, 5 },
    { DIAL_ICON_REPEAT, "repeat", {
        "M4 11.5V9.5A3.5 3.5 0 0 1 7.5 6H19",
        "M16.5 3.5 19 6l-2.5 2.5",
        "M20 12.5v2A3.5 3.5 0 0 1 16.5 18H5",
        "M7.5 20.5 5 18l2.5-2.5"
    }, 4 },
    { DIAL_ICON_VOLUME, "volume", {
        "M4 9.6v4.8a.6.6 0 0 0 .6.6h3l4.4 3.6V5.4L7.6 9H4.6a.6.6 0 0 0-.6.6z",
        "M15.5 9a4.2 4.2 0 0 1 0 6",
        "M18.2 6.3a8 8 0 0 1 0 11.4"
    }, 3 },
    { DIAL_ICON_RINGS, "rings", {
        "M12 3a9 9 0 1 1 0 18 9 9 0 0 1 0-18z",
        "M12 6.6c3.2 0 5.5 2.3 5.4 5.6-.1 3-2.5 5.2-5.5 5.2-3.1 0-5.3-2.4-5.3-5.4 0-3 2.4-5.4 5.4-5.4z",
        "F:M12 10.3a1.7 1.7 0 1 1 0 3.4 1.7 1.7 0 0 1 0-3.4z"
    }, 3 },

    /* Library (9) */
    { DIAL_ICON_NOTE, "note", {
        "M9 18V6.6l10-2.4v11.4",
        "M6.5 15.5a2.5 2.5 0 1 1 0 5 2.5 2.5 0 0 1 0-5z",
        "M16.5 13a2.5 2.5 0 1 1 0 5 2.5 2.5 0 0 1 0-5z"
    }, 3 },
    { DIAL_ICON_ALBUM, "album", {
        "M7.5 3.5h9a4 4 0 0 1 4 4v9a4 4 0 0 1-4 4h-9a4 4 0 0 1-4-4v-9a4 4 0 0 1 4-4z",
        "M12 8a4 4 0 1 1 0 8 4 4 0 0 1 0-8z",
        "F:M12 11.1a.9.9 0 1 1 0 1.8.9.9 0 0 1 0-1.8z"
    }, 3 },
    { DIAL_ICON_ARTIST, "artist", {
        "M12 4.5a4 4 0 1 1 0 8 4 4 0 0 1 0-8z",
        "M4.5 20c1.2-3.6 4-5.5 7.5-5.5s6.3 1.9 7.5 5.5"
    }, 2 },
    { DIAL_ICON_PLAYLIST, "playlist", {
        "M4 6h12",
        "M4 11h12",
        "M4 16h7",
        "M17.5 13.5v6",
        "M14.5 16.5h6"
    }, 5 },
    { DIAL_ICON_GENRE, "genre", {
        "M12 3.5 20.5 12 12 20.5 3.5 12z",
        "M12 8.5 15.5 12 12 15.5 8.5 12z"
    }, 2 },
    { DIAL_ICON_LEAF, "leaf", {
        "M5 19.5C5 11 10 5 20 4.5c0 10-5.6 15-13.6 15",
        "M5 19.5 13.5 11"
    }, 2 },
    { DIAL_ICON_HEART, "heart", {
        "M12 19.8 4.9 13a4.6 4.6 0 0 1 6.5-6.5l.6.6.6-.6a4.6 4.6 0 0 1 6.5 6.5z"
    }, 1 },
    { DIAL_ICON_MIC, "mic", {
        "M9 7a3 3 0 0 1 6 0v4a3 3 0 0 1-6 0z",
        "M5.8 11a6.2 6.2 0 0 0 12.4 0",
        "M12 17.2v3.3",
        "M9 20.5h6"
    }, 4 },
    { DIAL_ICON_LYRICS, "lyrics", {
        "M6 4.5h12A2.5 2.5 0 0 1 20.5 7v7.5A2.5 2.5 0 0 1 18 17h-6.8l-4.2 3.2V17H6a2.5 2.5 0 0 1-2.5-2.5V7A2.5 2.5 0 0 1 6 4.5z",
        "F:M9.3 8.2a1.35 1.35 0 1 1 0 2.7 1.35 1.35 0 0 1 0-2.7z",
        "M10.4 9.7c0 1.6-.7 2.7-1.8 3.3",
        "F:M14.7 8.2a1.35 1.35 0 1 1 0 2.7 1.35 1.35 0 0 1 0-2.7z",
        "M15.8 9.7c0 1.6-.7 2.7-1.8 3.3"
    }, 5 },

    /* Files (6) */
    { DIAL_ICON_FOLDER, "folder", {
        "M3.5 7.5a2 2 0 0 1 2-2h3.7a1.5 1.5 0 0 1 1.06.44L11.5 7.2h7a2 2 0 0 1 2 2v8.3a2 2 0 0 1-2 2h-13a2 2 0 0 1-2-2z"
    }, 1 },
    { DIAL_ICON_FILEAUDIO, "fileaudio", {
        "M7 3.5h6.4L18.5 8.6V19a1.5 1.5 0 0 1-1.5 1.5H7A1.5 1.5 0 0 1 5.5 19V5A1.5 1.5 0 0 1 7 3.5z",
        "M13.2 3.6V8.8h5.2",
        "M11 16.3v-4.6l2.6-.7",
        "M9.8 15.1a1.2 1.2 0 1 1 0 2.4 1.2 1.2 0 0 1 0-2.4z"
    }, 4 },
    { DIAL_ICON_SDCARD, "sdcard", {
        "M8.4 3.5h8.1a2 2 0 0 1 2 2v13a2 2 0 0 1-2 2h-9a2 2 0 0 1-2-2V9.2a1 1 0 0 1 .3-.7l3.9-4.7a1 1 0 0 1 .7-.3z",
        "M10.5 7v2.5",
        "M13 7v2.5",
        "M15.5 7v2.5"
    }, 4 },
    { DIAL_ICON_EDIT, "edit", {
        "M4.5 19.5h3.8L19 8.8a2.7 2.7 0 0 0-3.8-3.8L4.5 15.7z",
        "M13.8 6.4l3.8 3.8"
    }, 2 },
    { DIAL_ICON_MOVE, "move", {
        "M3.5 7.5a2 2 0 0 1 2-2h3.7a1.5 1.5 0 0 1 1.06.44L11.5 7.2h7a2 2 0 0 1 2 2v8.3a2 2 0 0 1-2 2h-13a2 2 0 0 1-2-2z",
        "M8.5 13.5h7",
        "M13 11l2.5 2.5L13 16"
    }, 3 },
    { DIAL_ICON_TRASH, "trash", {
        "M4.5 7h15",
        "M9.5 7V5.3a.8.8 0 0 1 .8-.8h3.4a.8.8 0 0 1 .8.8V7",
        "M6.5 7l.9 11.6a1.8 1.8 0 0 0 1.8 1.6h5.6a1.8 1.8 0 0 0 1.8-1.6L17.5 7",
        "M10.3 11v5.2",
        "M13.7 11v5.2"
    }, 5 },

    /* Modes & Connectivity (16) */
    { DIAL_ICON_MODES, "modes", {
        "M4 8h13.5",
        "M14.5 4.5 18 8l-3.5 3.5",
        "M20 16H6.5",
        "M9.5 12.5 6 16l3.5 3.5"
    }, 4 },
    { DIAL_ICON_MODE_PLAYER, "mode_player", {
        "M12 3a9 9 0 1 1 0 18 9 9 0 0 1 0-18z",
        "M12 6.8a5.2 5.2 0 1 1 0 10.4 5.2 5.2 0 0 1 0-10.4z",
        "F:M10.8 9.6v4.8l4.2-2.4z"
    }, 3 },
    { DIAL_ICON_MODE_USBDAC, "mode_usbdac", {
        "M4.5 9h7.5a3 3 0 0 1 0 6H4.5a3 3 0 0 1 0-6z",
        "M6.5 12h3.5",
        "M16.2 9.5a3.8 3.8 0 0 1 0 5",
        "M19 7a7.2 7.2 0 0 1 0 10"
    }, 4 },
    { DIAL_ICON_MODE_TRANSPORT, "mode_transport", {
        "F:M6 10.2a1.8 1.8 0 1 1 0 3.6 1.8 1.8 0 0 1 0-3.6z",
        "M10.5 8.5a5 5 0 0 1 0 7",
        "M14 6a8.5 8.5 0 0 1 0 12",
        "M17.5 12h3.5",
        "M19.2 9.8 21.4 12l-2.2 2.2"
    }, 5 },
    { DIAL_ICON_MODE_BTRX, "mode_btrx", {
        "M10.5 7.5l8 8-4 3.8V4.7l4 3.8-8 8",
        "M3 12h4.5",
        "M5.5 9.6 7.9 12l-2.4 2.4"
    }, 3 },
    { DIAL_ICON_MODE_AIRPLAY, "mode_airplay", {
        "M4.5 15.2a9 9 0 0 1 15 0",
        "M7.5 12.6a5.2 5.2 0 0 1 9 0",
        "M12 14.5l4.4 5.5H7.6z"
    }, 3 },
    { DIAL_ICON_MODE_TRANSFER, "mode_transfer", {
        "M7.5 3.5h7l4 4V19a1.5 1.5 0 0 1-1.5 1.5h-9A1.5 1.5 0 0 1 6 19V5A1.5 1.5 0 0 1 7.5 3.5z",
        "M9.5 11.2h5.2",
        "M12.8 9.2l2 2-2 2",
        "M14.7 15.5H9.5",
        "M11.2 13.5l-2 2 2 2"
    }, 5 },
    { DIAL_ICON_USB, "usb", {
        "M8 8h8a4 4 0 0 1 0 8H8a4 4 0 0 1 0-8z",
        "M8.5 12h7"
    }, 2 },
    { DIAL_ICON_USB_CHARGE, "usb_charge", {
        "M6.5 7.5h11a4.5 4.5 0 0 1 0 9h-11a4.5 4.5 0 0 1 0-9z",
        "M12.6 9.3 10.5 12h3l-2.1 2.7"
    }, 2 },
    { DIAL_ICON_BROADCAST, "broadcast", {
        "F:M12 10.2a1.8 1.8 0 1 1 0 3.6 1.8 1.8 0 0 1 0-3.6z",
        "M8.2 8.2a5.4 5.4 0 0 0 0 7.6",
        "M15.8 8.2a5.4 5.4 0 0 1 0 7.6",
        "M5.4 5.4a9.3 9.3 0 0 0 0 13.2",
        "M18.6 5.4a9.3 9.3 0 0 1 0 13.2"
    }, 5 },
    { DIAL_ICON_BLUETOOTH, "bluetooth", {
        "M7 7.5l10 9-5 4V3.5l5 4-10 9"
    }, 1 },
    { DIAL_ICON_AIRPLAY, "airplay", {
        "M6.5 17H5.5a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h13a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2h-1",
        "M12 14.2l4.6 5.8H7.4z"
    }, 2 },
    { DIAL_ICON_TRANSFER, "transfer", {
        "M12 4v10.5",
        "M8 10.5l4 4 4-4",
        "M4.5 15v2.5a2 2 0 0 0 2 2h11a2 2 0 0 0 2-2V15"
    }, 3 },
    { DIAL_ICON_WIFI, "wifi", {
        "M2.8 9.2a13.6 13.6 0 0 1 18.4 0",
        "M5.8 12.4a9.2 9.2 0 0 1 12.4 0",
        "M8.8 15.6a4.8 4.8 0 0 1 6.4 0",
        "F:M12 17.6a1.1 1.1 0 1 1 0 2.2 1.1 1.1 0 0 1 0-2.2z"
    }, 4 },
    { DIAL_ICON_HEADPHONES, "headphones", {
        "M4 15v-3a8 8 0 0 1 16 0v3",
        "M5.5 14h1a1.5 1.5 0 0 1 1.5 1.5v3.5a1.5 1.5 0 0 1-1.5 1.5h-1A1.5 1.5 0 0 1 4 19v-3.5A1.5 1.5 0 0 1 5.5 14z",
        "M17.5 14h1a1.5 1.5 0 0 1 1.5 1.5v3.5a1.5 1.5 0 0 1-1.5 1.5h-1A1.5 1.5 0 0 1 16 19v-3.5a1.5 1.5 0 0 1 1.5-1.5z"
    }, 3 },
    { DIAL_ICON_CHIP, "chip", {
        "M8.5 6h7A2.5 2.5 0 0 1 18 8.5v7a2.5 2.5 0 0 1-2.5 2.5h-7A2.5 2.5 0 0 1 6 15.5v-7A2.5 2.5 0 0 1 8.5 6z",
        "M9.5 3v3", "M14.5 3v3", "M9.5 18v3", "M14.5 18v3",
        "M3 9.5h3", "M3 14.5h3", "M18 9.5h3", "M18 14.5h3"
    }, 9 },

    /* Settings (16) */
    { DIAL_ICON_TUNE, "tune", {
        "M4 7h9", "M17 7h3", "M4 17h3", "M11 17h9",
        "M15 5a2 2 0 1 1 0 4 2 2 0 0 1 0-4z",
        "M9 15a2 2 0 1 1 0 4 2 2 0 0 1 0-4z"
    }, 6 },
    { DIAL_ICON_EQ, "eq", {
        "M3 13c2.4 0 2.6-6.5 5-6.5s2.6 11 5 11 2.6-7.5 5-7.5c1.4 0 2 1.2 3 2.5"
    }, 1 },
    { DIAL_ICON_FILTER, "filter", {
        "M3 8.5h6c3.4 0 4.2 7 8.5 7H21",
        "M3 19.5h18"
    }, 2 },
    { DIAL_ICON_GAIN, "gain", {
        "M3 13h3l2.6-6 3.8 11 3-8.5L17 13h4"
    }, 1 },
    { DIAL_ICON_GAIN_LOW, "gain_low", {
        "M3 13.5c2.2-3 4.5-3 6.8 0s4.5 3 6.8 0c1.4-1.8 2.8-2.2 4.4-1.5",
        "M4 18h16"
    }, 2 },
    { DIAL_ICON_GAIN_HIGH, "gain_high", {
        "M2.5 13h2.2l2.3-7.5 3.4 13.5 3.3-11 2.3 5h5.5",
        "M19 4.2v3.6",
        "M17.2 6h3.6"
    }, 3 },
    { DIAL_ICON_CONTRAST, "contrast", {
        "M12 3.5a8.5 8.5 0 1 1 0 17 8.5 8.5 0 0 1 0-17z",
        "F:M12 3.5a8.5 8.5 0 0 1 0 17z"
    }, 2 },
    { DIAL_ICON_SUN, "sun", {
        "M12 8a4 4 0 1 1 0 8 4 4 0 0 1 0-8z",
        "M12 2.8v1.8", "M12 19.4v1.8", "M2.8 12h1.8", "M19.4 12h1.8",
        "M5.5 5.5l1.3 1.3", "M17.2 17.2l1.3 1.3", "M5.5 18.5l1.3-1.3", "M17.2 6.8l1.3-1.3"
    }, 9 },
    { DIAL_ICON_MOON, "moon", {
        "M19.5 14.6A7.8 7.8 0 1 1 9.4 4.5a6.3 6.3 0 0 0 10.1 10.1z"
    }, 1 },
    { DIAL_ICON_BATTERY, "battery", {
        "M5.5 7h11a3 3 0 0 1 3 3v4a3 3 0 0 1-3 3h-11a3 3 0 0 1-3-3v-4a3 3 0 0 1 3-3z",
        "M22 10.5v3",
        "F:M6.5 9.6h7a.9.9 0 0 1 .9.9v3a.9.9 0 0 1-.9.9h-7a.9.9 0 0 1-.9-.9v-3a.9.9 0 0 1 .9-.9z"
    }, 3 },
    { DIAL_ICON_POWER, "power", {
        "M12 3.5v8",
        "M7.2 6.6a7.5 7.5 0 1 0 9.6 0"
    }, 2 },
    { DIAL_ICON_LOCK, "lock", {
        "M7.5 10.5h9a2.5 2.5 0 0 1 2.5 2.5v5a2.5 2.5 0 0 1-2.5 2.5h-9A2.5 2.5 0 0 1 5 18v-5a2.5 2.5 0 0 1 2.5-2.5z",
        "M8 10.5V8a4 4 0 0 1 8 0v2.5",
        "F:M12 14.3a1.2 1.2 0 1 1 0 2.4 1.2 1.2 0 0 1 0-2.4z"
    }, 3 },
    { DIAL_ICON_TIMER, "timer", {
        "M12 5.5a7.5 7.5 0 1 1 0 15 7.5 7.5 0 0 1 0-15z",
        "M12 9.5v3.5l2.3 1.6",
        "M9.5 2.8h5"
    }, 3 },
    { DIAL_ICON_INFO, "info", {
        "M12 3a9 9 0 1 1 0 18 9 9 0 0 1 0-18z",
        "M12 11v5.5",
        "F:M12 6.9a1.1 1.1 0 1 1 0 2.2 1.1 1.1 0 0 1 0-2.2z"
    }, 3 },
    { DIAL_ICON_GAMES, "games", {
        "M7.8 7h8.4a5 5 0 0 1 4.85 6.2l-.9 3.6a2.4 2.4 0 0 1-4.1 1.1L14.4 16H9.6l-1.65 1.9a2.4 2.4 0 0 1-4.1-1.1l-.9-3.6A5 5 0 0 1 7.8 7z",
        "M8.2 10v3.2",
        "M6.6 11.6h3.2",
        "F:M15.6 9.9a.95.95 0 1 1 0 1.9.95.95 0 0 1 0-1.9z",
        "F:M17.4 12a.95.95 0 1 1 0 1.9.95.95 0 0 1 0-1.9z"
    }, 5 },
    { DIAL_ICON_APPS, "apps", {
        "M7.5 4.5a3 3 0 1 1 0 6 3 3 0 0 1 0-6z",
        "M16.5 4.5a3 3 0 1 1 0 6 3 3 0 0 1 0-6z",
        "M7.5 13.5a3 3 0 1 1 0 6 3 3 0 0 1 0-6z",
        "M16.5 13.5a3 3 0 1 1 0 6 3 3 0 0 1 0-6z"
    }, 4 },

    /* Utility (11) */
    { DIAL_ICON_BACK, "back", { "M14.5 5.5 8 12l6.5 6.5" }, 1 },
    { DIAL_ICON_CHEV, "chev", { "M9.5 5.5 16 12l-6.5 6.5" }, 1 },
    { DIAL_ICON_UP,   "up",   { "M5.5 14.5 12 8l6.5 6.5" }, 1 },
    { DIAL_ICON_DOWN, "down", { "M5.5 9.5 12 16l6.5-6.5" }, 1 },
    { DIAL_ICON_CHECK,"check",{ "M5 12.5l4.5 4.5L19 7.5" }, 1 },
    { DIAL_ICON_CLOSE,"close",{ "M6.5 6.5l11 11", "M17.5 6.5l-11 11" }, 2 },
    { DIAL_ICON_PLUS, "plus", { "M12 5v14", "M5 12h14" }, 2 },
    { DIAL_ICON_SEARCH,"search",{ "M11 4.5a6.5 6.5 0 1 1 0 13 6.5 6.5 0 0 1 0-13z", "M16 16l4.5 4.5" }, 2 },
    { DIAL_ICON_MORE, "more", {
        "F:M6 10.6a1.4 1.4 0 1 1 0 2.8 1.4 1.4 0 0 1 0-2.8z",
        "F:M12 10.6a1.4 1.4 0 1 1 0 2.8 1.4 1.4 0 0 1 0-2.8z",
        "F:M18 10.6a1.4 1.4 0 1 1 0 2.8 1.4 1.4 0 0 1 0-2.8z"
    }, 3 },
    { DIAL_ICON_RIM, "rim", {
        "M5 17.5A8.6 8.6 0 1 1 19 17.5",
        "F:M19 15.6a1.9 1.9 0 1 1 0 3.8 1.9 1.9 0 0 1 0-3.8z"
    }, 2 },
    { DIAL_ICON_WARNING, "warning", {
        "M10.4 4.6 3.3 17a1.8 1.8 0 0 0 1.6 2.7h14.2a1.8 1.8 0 0 0 1.6-2.7L13.6 4.6a1.8 1.8 0 0 0-3.2 0z",
        "M12 9.5v4",
        "F:M12 15.6a1.05 1.05 0 1 1 0 2.1 1.05 1.05 0 0 1 0-2.1z"
    }, 3 },

    /* Nature / Brand (4) */
    { DIAL_ICON_FJALL, "fjall", {
        "M12 2.5a9.5 9.5 0 1 1 0 19 9.5 9.5 0 0 1 0-19z",
        "M4.8 16.5 9.4 10l3.1 4.2 2.2-2.7 4.5 5"
    }, 2 },
    { DIAL_ICON_PINE, "pine", {
        "M12 3l5 7h-3l4 6H6l4-6H7z",
        "M12 16v5"
    }, 2 },
    { DIAL_ICON_PEBBLE, "pebble", {
        "M6.5 8.5C8.5 5.5 15 4.6 18 7.2s3 8.4-.5 10.6-11 2.7-12.6-1.1S4.8 11 6.5 8.5z"
    }, 1 },
    { DIAL_ICON_SNOW, "snow", {
        "M12 3v18",
        "M4.2 7.5l15.6 9",
        "M4.2 16.5l15.6-9",
        "M9.8 4.8 12 7l2.2-2.2",
        "M9.8 19.2 12 17l2.2 2.2"
    }, 5 }
};

const dial_icon_def_t *dial_icon_get(dial_icon_id_t id) {
    if (id < 0 || id >= DIAL_ICON_COUNT) {
        return &G_FJALL_ICONS[DIAL_ICON_INFO];
    }
    return &G_FJALL_ICONS[id];
}

const dial_icon_def_t *dial_icon_by_name(const char *name) {
    if (!name) return &G_FJALL_ICONS[DIAL_ICON_INFO];
    /* Strip optional "fjall-" prefix if present */
    if (strncmp(name, "fjall-", 6) == 0) {
        name += 6;
    }
    for (size_t i = 0; i < DIAL_ICON_COUNT; ++i) {
        if (strcmp(G_FJALL_ICONS[i].name, name) == 0) {
            return &G_FJALL_ICONS[i];
        }
    }
    return &G_FJALL_ICONS[DIAL_ICON_INFO];
}

/* ── SVG Path Parser & Flattener (24x24 grid -> pixel space) ── */
#define MAX_PATH_SEGS 256

typedef struct {
    float x0, y0, x1, y1;
} icon_seg_t;

typedef struct {
    icon_seg_t segs[MAX_PATH_SEGS];
    size_t     count;
} icon_poly_t;

static void poly_add_seg(icon_poly_t *poly, float x0, float y0, float x1, float y1) {
    if (poly->count >= MAX_PATH_SEGS) return;
    if (fabsf(x1 - x0) < 1e-4f && fabsf(y1 - y0) < 1e-4f) return;
    poly->segs[poly->count].x0 = x0;
    poly->segs[poly->count].y0 = y0;
    poly->segs[poly->count].x1 = x1;
    poly->segs[poly->count].y1 = y1;
    poly->count++;
}

static void poly_add_cubic(icon_poly_t *poly,
                           float x0, float y0,
                           float x1, float y1,
                           float x2, float y2,
                           float x3, float y3) {
    const int steps = 16;
    float px = x0, py = y0;
    for (int i = 1; i <= steps; ++i) {
        float t  = (float)i / (float)steps;
        float u  = 1.0f - t;
        float b0 = u * u * u;
        float b1 = 3.0f * u * u * t;
        float b2 = 3.0f * u * t * t;
        float b3 = t * t * t;
        float nx = b0 * x0 + b1 * x1 + b2 * x2 + b3 * x3;
        float ny = b0 * y0 + b1 * y1 + b2 * y2 + b3 * y3;
        poly_add_seg(poly, px, py, nx, ny);
        px = nx;
        py = ny;
    }
}

/* W3C SVG F.6.5 Endpoint-to-Center Arc Parameterization */
static void poly_add_svg_arc(icon_poly_t *poly,
                             float x1, float y1,
                             float rx, float ry,
                             float phi_deg,
                             int large_arc,
                             int sweep,
                             float x2, float y2) {
    if (fabsf(x2 - x1) < 1e-4f && fabsf(y2 - y1) < 1e-4f) return;
    rx = fabsf(rx);
    ry = fabsf(ry);
    if (rx < 1e-4f || ry < 1e-4f) {
        poly_add_seg(poly, x1, y1, x2, y2);
        return;
    }

    float phi = phi_deg * ((float)M_PI / 180.0f);
    float cos_phi = cosf(phi);
    float sin_phi = sinf(phi);

    float dx2 = (x1 - x2) * 0.5f;
    float dy2 = (y1 - y2) * 0.5f;
    float x1p =  cos_phi * dx2 + sin_phi * dy2;
    float y1p = -sin_phi * dx2 + cos_phi * dy2;

    float rx2 = rx * rx;
    float ry2 = ry * ry;
    float x1p2 = x1p * x1p;
    float y1p2 = y1p * y1p;

    float lambda = x1p2 / rx2 + y1p2 / ry2;
    if (lambda > 1.0f) {
        float s = sqrtf(lambda);
        rx *= s;
        ry *= s;
        rx2 = rx * rx;
        ry2 = ry * ry;
    }

    float num = rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2;
    float den = rx2 * y1p2 + ry2 * x1p2;
    float rad = (den > 1e-7f && num > 0.0f) ? sqrtf(num / den) : 0.0f;
    if (large_arc == sweep) {
        rad = -rad;
    }

    float cxp =  rad * (rx * y1p) / ry;
    float cyp = -rad * (ry * x1p) / rx;

    float cx = cos_phi * cxp - sin_phi * cyp + (x1 + x2) * 0.5f;
    float cy = sin_phi * cxp + cos_phi * cyp + (y1 + y2) * 0.5f;

    float ux = (x1p - cxp) / rx;
    float uy = (y1p - cyp) / ry;
    float vx = (-x1p - cxp) / rx;
    float vy = (-y1p - cyp) / ry;

    float theta1 = atan2f(uy, ux);
    float dtheta = atan2f(ux * vy - uy * vx, ux * vx + uy * vy);

    if (sweep == 0 && dtheta > 0.0f) {
        dtheta -= 2.0f * (float)M_PI;
    } else if (sweep != 0 && dtheta < 0.0f) {
        dtheta += 2.0f * (float)M_PI;
    }

    const int steps = 24;
    float px = x1, py = y1;
    for (int i = 1; i <= steps; ++i) {
        float t = theta1 + dtheta * ((float)i / (float)steps);
        float ct = cosf(t);
        float st = sinf(t);
        float nx = cos_phi * rx * ct - sin_phi * ry * st + cx;
        float ny = sin_phi * rx * ct + cos_phi * ry * st + cy;
        poly_add_seg(poly, px, py, nx, ny);
        px = nx;
        py = ny;
    }
}

static void skip_ws_comma(const char **p) {
    while (**p && (isspace((unsigned char)**p) || **p == ',')) {
        (*p)++;
    }
}

static bool parse_float(const char **p, float *out) {
    skip_ws_comma(p);
    const char *s = *p;
    if (!*s) return false;
    if (!isdigit((unsigned char)*s) && *s != '-' && *s != '+' && *s != '.') {
        return false;
    }
    char *end = NULL;
    float v = strtof(s, &end);
    if (end == s) return false;
    *p = end;
    *out = v;
    return true;
}

static bool parse_flag(const char **p, int *out) {
    skip_ws_comma(p);
    if (**p == '0' || **p == '1') {
        *out = (**p == '1') ? 1 : 0;
        (*p)++;
        return true;
    }
    return false;
}

static void parse_svg_subpath(const char *d,
                              float ox,
                              float oy,
                              float scale,
                              icon_poly_t *poly,
                              bool close_for_fill) {
    poly->count = 0;
    const char *p = d;
    char cmd = 0;
    float cx = 0.0f, cy = 0.0f;
    float sx = 0.0f, sy = 0.0f;
    float last_c2x = 0.0f, last_c2y = 0.0f;
    char prev_cmd = 0;

    while (*p) {
        skip_ws_comma(&p);
        if (!*p) break;

        if (isalpha((unsigned char)*p)) {
            cmd = *p++;
            if (cmd == 'Z' || cmd == 'z') {
                poly_add_seg(poly,
                             ox + cx * scale, oy + cy * scale,
                             ox + sx * scale, oy + sy * scale);
                cx = sx;
                cy = sy;
                prev_cmd = cmd;
                continue;
            }
        }

        switch (cmd) {
            case 'M':
            case 'm': {
                float x = 0.0f, y = 0.0f;
                if (!parse_float(&p, &x) || !parse_float(&p, &y)) return;
                if (cmd == 'm') { x += cx; y += cy; }
                cx = sx = x;
                cy = sy = y;
                cmd = (cmd == 'M') ? 'L' : 'l';
                break;
            }
            case 'L':
            case 'l': {
                float x = 0.0f, y = 0.0f;
                if (!parse_float(&p, &x) || !parse_float(&p, &y)) return;
                if (cmd == 'l') { x += cx; y += cy; }
                poly_add_seg(poly,
                             ox + cx * scale, oy + cy * scale,
                             ox + x * scale,  oy + y * scale);
                cx = x;
                cy = y;
                break;
            }
            case 'H':
            case 'h': {
                float x = 0.0f;
                if (!parse_float(&p, &x)) return;
                if (cmd == 'h') x += cx;
                poly_add_seg(poly,
                             ox + cx * scale, oy + cy * scale,
                             ox + x * scale,  oy + cy * scale);
                cx = x;
                break;
            }
            case 'V':
            case 'v': {
                float y = 0.0f;
                if (!parse_float(&p, &y)) return;
                if (cmd == 'v') y += cy;
                poly_add_seg(poly,
                             ox + cx * scale, oy + cy * scale,
                             ox + cx * scale, oy + y * scale);
                cy = y;
                break;
            }
            case 'C':
            case 'c': {
                float x1 = 0, y1 = 0, x2 = 0, y2 = 0, x = 0, y = 0;
                if (!parse_float(&p, &x1) || !parse_float(&p, &y1) ||
                    !parse_float(&p, &x2) || !parse_float(&p, &y2) ||
                    !parse_float(&p, &x)  || !parse_float(&p, &y)) return;
                if (cmd == 'c') {
                    x1 += cx; y1 += cy;
                    x2 += cx; y2 += cy;
                    x  += cx; y  += cy;
                }
                poly_add_cubic(poly,
                               ox + cx * scale, oy + cy * scale,
                               ox + x1 * scale, oy + y1 * scale,
                               ox + x2 * scale, oy + y2 * scale,
                               ox + x  * scale, oy + y  * scale);
                last_c2x = x2;
                last_c2y = y2;
                cx = x;
                cy = y;
                break;
            }
            case 'S':
            case 's': {
                float x2 = 0, y2 = 0, x = 0, y = 0;
                if (!parse_float(&p, &x2) || !parse_float(&p, &y2) ||
                    !parse_float(&p, &x)  || !parse_float(&p, &y)) return;
                if (cmd == 's') {
                    x2 += cx; y2 += cy;
                    x  += cx; y  += cy;
                }
                float x1 = cx, y1 = cy;
                if (prev_cmd == 'C' || prev_cmd == 'c' || prev_cmd == 'S' || prev_cmd == 's') {
                    x1 = 2.0f * cx - last_c2x;
                    y1 = 2.0f * cy - last_c2y;
                }
                poly_add_cubic(poly,
                               ox + cx * scale, oy + cy * scale,
                               ox + x1 * scale, oy + y1 * scale,
                               ox + x2 * scale, oy + y2 * scale,
                               ox + x  * scale, oy + y  * scale);
                last_c2x = x2;
                last_c2y = y2;
                cx = x;
                cy = y;
                break;
            }
            case 'A':
            case 'a': {
                float rx = 0, ry = 0, rot = 0, x = 0, y = 0;
                int large_arc = 0, sweep = 0;
                if (!parse_float(&p, &rx) || !parse_float(&p, &ry) ||
                    !parse_float(&p, &rot) ||
                    !parse_flag(&p, &large_arc) || !parse_flag(&p, &sweep) ||
                    !parse_float(&p, &x) || !parse_float(&p, &y)) return;
                if (cmd == 'a') {
                    x += cx;
                    y += cy;
                }
                poly_add_svg_arc(poly,
                                 ox + cx * scale, oy + cy * scale,
                                 rx * scale, ry * scale,
                                 rot, large_arc, sweep,
                                 ox + x * scale, oy + y * scale);
                cx = x;
                cy = y;
                break;
            }
            default:
                p++;
                break;
        }
        prev_cmd = cmd;
    }

    if (close_for_fill) {
        poly_add_seg(poly,
                     ox + cx * scale, oy + cy * scale,
                     ox + sx * scale, oy + sy * scale);
    }
}

static float point_to_seg_dist(float px, float py, const icon_seg_t *s) {
    float vx = s->x1 - s->x0;
    float vy = s->y1 - s->y0;
    float wx = px - s->x0;
    float wy = py - s->y0;
    float c1 = wx * vx + wy * vy;
    if (c1 <= 0.0f) {
        return sqrtf(wx * wx + wy * wy);
    }
    float c2 = vx * vx + vy * vy;
    if (c2 <= c1) {
        float dx = px - s->x1;
        float dy = py - s->y1;
        return sqrtf(dx * dx + dy * dy);
    }
    float b = c1 / c2;
    float bx = s->x0 + b * vx;
    float by = s->y0 + b * vy;
    float dx = px - bx;
    float dy = py - by;
    return sqrtf(dx * dx + dy * dy);
}

static bool point_in_poly(float px, float py, const icon_poly_t *poly) {
    bool inside = false;
    for (size_t i = 0; i < poly->count; ++i) {
        float x0 = poly->segs[i].x0, y0 = poly->segs[i].y0;
        float x1 = poly->segs[i].x1, y1 = poly->segs[i].y1;
        if (((y0 > py) != (y1 > py)) &&
            (px < (x1 - x0) * (py - y0) / (y1 - y0) + x0)) {
            inside = !inside;
        }
    }
    return inside;
}

void dial_draw_icon(dial_fb_t *fb,
                    dial_icon_id_t id,
                    float x,
                    float y,
                    float size_px,
                    float stroke_w,
                    uint32_t rgb,
                    uint8_t alpha) {
    if (!fb || size_px <= 0.0f || alpha == 0) return;

    const dial_icon_def_t *def = dial_icon_get(id);
    const float scale = size_px / 24.0f;
    const float base_sw = (stroke_w > 0.0f) ? stroke_w : 1.75f;
    const float half_sw = (base_sw * scale) * 0.5f;

    int min_x = (int)floorf(x - half_sw - 2.0f);
    int min_y = (int)floorf(y - half_sw - 2.0f);
    int max_x = (int)ceilf(x + size_px + half_sw + 2.0f);
    int max_y = (int)ceilf(y + size_px + half_sw + 2.0f);

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= (int)fb->width)  max_x = (int)fb->width - 1;
    if (max_y >= (int)fb->height) max_y = (int)fb->height - 1;

    icon_poly_t polys[10];
    bool        is_fill[10];
    size_t      n_sub = def->subpath_count;
    if (n_sub > 10) n_sub = 10;

    for (size_t s = 0; s < n_sub; ++s) {
        const char *raw = def->subpaths[s];
        if (!raw) {
            polys[s].count = 0;
            is_fill[s] = false;
            continue;
        }
        if (raw[0] == 'F' && raw[1] == ':') {
            is_fill[s] = true;
            parse_svg_subpath(raw + 2, x, y, scale, &polys[s], true);
        } else {
            is_fill[s] = false;
            parse_svg_subpath(raw, x, y, scale, &polys[s], false);
        }
    }

    for (int py = min_y; py <= max_y; ++py) {
        const float cy = (float)py + 0.5f;
        for (int px = min_x; px <= max_x; ++px) {
            const float cx = (float)px + 0.5f;
            float max_cov = 0.0f;

            for (size_t s = 0; s < n_sub; ++s) {
                const icon_poly_t *poly = &polys[s];
                if (poly->count == 0) continue;

                float min_d = 1e9f;
                for (size_t k = 0; k < poly->count; ++k) {
                    float d = point_to_seg_dist(cx, cy, &poly->segs[k]);
                    if (d < min_d) min_d = d;
                }

                float cov = 0.0f;
                if (is_fill[s]) {
                    bool in = point_in_poly(cx, cy, poly);
                    float sd = in ? -min_d : min_d;
                    cov = 0.5f - sd;
                } else {
                    cov = (half_sw + 0.5f) - min_d;
                }

                if (cov > max_cov) {
                    max_cov = cov;
                }
            }

            if (max_cov > 0.0f) {
                if (max_cov > 1.0f) max_cov = 1.0f;
                uint8_t pix_a = (uint8_t)((float)alpha * max_cov + 0.5f);
                if (pix_a > 0) {
                    dial_draw_pixel_aa(fb, px, py, rgb, pix_a);
                }
            }
        }
    }
}

void dial_draw_icon_by_name(dial_fb_t *fb,
                            const char *name,
                            float x,
                            float y,
                            float size_px,
                            float stroke_w,
                            uint32_t rgb,
                            uint8_t alpha) {
    const dial_icon_def_t *def = dial_icon_by_name(name);
    dial_draw_icon(fb, def->id, x, y, size_px, stroke_w, rgb, alpha);
}

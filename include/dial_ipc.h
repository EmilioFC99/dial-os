#ifndef DIAL_IPC_H
#define DIAL_IPC_H

#include <stdint.h>
#include <stdbool.h>

/* POSIX Message Queue Endpoints & Device Nodes (Verified on FiiO Snowsky DISC V2.09..V2.57) */
#define DIAL_MQ_PLAYER_NAME     "/player"             /* Commands: UI -> Audio Engine */
#define DIAL_MQ_UI_NAME         "/ui"                 /* State & Metadata: Audio Engine -> UI */
#define DIAL_MQ_PLAYER          DIAL_MQ_PLAYER_NAME
#define DIAL_MQ_UI              DIAL_MQ_UI_NAME
#define DIAL_MQ_MAX_MSG_SIZE    8192
#define DIAL_MQ_MAX_MSGS        20

/* Physical Snowsky DISC Linux Device Nodes (ingenicfb + x2000_key + cst816t) */
#define DIAL_DEV_FB0            "/dev/fb0"            /* 360x360 (360x1080 virtual) 32bpp XRGB8888, 180° rotated */
#define DIAL_DEV_KEYS           "/dev/input/event0"   /* x2000_key physical GPIO buttons */
#define DIAL_DEV_TOUCH          "/dev/input/event1"   /* cst816t capacitive touch (MT Type-B, 180° rotated) */

/* Reverse-Engineered FiiO mq_player Command Tags (4 ASCII chars)
 * Wire frame format: TAG(4) + TOTAL_LEN(4hex) + [VALUE1(4hex)] + [optional payload]
 */
#define DIAL_CMD_PLAY_LIST      "0100"  /* Play item by list index */
#define DIAL_CMD_PLAY_MODE      "0102"  /* 0=Seq, 1=Shuffle, 2=Repeat1, 3=RepeatAll, 4=Single */
#define DIAL_CMD_SEEK           "0103"  /* Seek position in ms: 01030010<8hex_ms> */
#define DIAL_CMD_FAVORITE       "0104"  /* Favorite / Unfavorite toggle: 0104000C<4hex> */
#define DIAL_CMD_PLAY_PAUSE     "0201"  /* Toggle play/pause transport: 0201000C<4hex> */
#define DIAL_CMD_GET_STATE      "0202"  /* Request full player status snapshot: 02020008 */
#define DIAL_CMD_USB_GADGET     "0642"  /* 0=Local, 1=Mass Storage, 2=USB DAC, 5=USB Host Audio */
#define DIAL_CMD_GAIN_LEGACY    "0645"  /* V2.09 gain: 0=Low, 1=High */
#define DIAL_CMD_GAPLESS        "0647"  /* Gapless playback toggle: 0647000C<4hex> */
#define DIAL_CMD_GAIN_V257      "0649"  /* V2.28/V2.40/V2.57 gain: 0=Low, 1=High */
#define DIAL_CMD_DAC_FILTER_HW  "0653"  /* CS43131 reconstruction filter (0..4): 0653000C<4hex> */
#define DIAL_CMD_SET_SOURCE     "0657"  /* Work mode: 01=USBDAC, 02=SPDIF, 06=BTSink, 08=Local, 0A=AirPlay */
#define DIAL_CMD_SET_OUTPUT     "0666"  /* Output route: 2=BTSRC, 3=USBHost, 4=SPDIF, 6=Local DAC */
#define DIAL_CMD_SET_PEQ_BAND   "0678"  /* Parametric EQ band setter */
#define DIAL_CMD_SET_EQ         "0689"  /* Apply EQ preset (21-way preset engine) */
#define DIAL_CMD_BT_CODEC       "06b3"  /* Set Bluetooth codec: 06b3001D000<0..4><MAC> */
#define DIAL_CMD_BT_LDAC_QUAL   "06b4"  /* Set LDAC quality (0=mob/330, 1=std/660, 2=high/990) */
#define DIAL_CMD_GAIN_MODE      "06c1"  /* Toggle CS43131 analogue gain relay / BT init */
#define DIAL_CMD_DAC_FILTER     "06c2"  /* Set CS43131 reconstruction filter (0..4) */
#define DIAL_CMD_VOL_SET        "0715"  /* Set absolute hardware volume (0..120): 0715000C<4hex> */
#define DIAL_CMD_NEXT_TRACK     "0647"  /* Skip to next track in active queue */
#define DIAL_CMD_PREV_TRACK     "0649"  /* Skip to previous track in active queue */
#define DIAL_CMD_SET_VOLUME     DIAL_CMD_VOL_SET
#define DIAL_CMD_BT_RECONNECT   DIAL_CMD_BT_CODEC
#define DIAL_CMD_USB_CHARGE     DIAL_CMD_USB_GADGET
#define DIAL_CMD_DELETE_FILE    "0209"  /* Delete media file from SD card */
#define DIAL_CMD_EQ_PRESET      DIAL_CMD_SET_EQ

/* Bluetooth Codec Priority Ladder (Highest -> Lowest Fallback) */
typedef enum {
    DIAL_BT_CODEC_LDAC    = 4, /* Priority 1: 990 -> 660 -> 330 kbps */
    DIAL_BT_CODEC_APTX_HD = 3, /* Priority 2: 576 kbps 24-bit */
    DIAL_BT_CODEC_APTX    = 2, /* Priority 3: 352 kbps 16-bit */
    DIAL_BT_CODEC_AAC     = 1, /* Priority 4: 256 kbps VBR */
    DIAL_BT_CODEC_SBC     = 0  /* Priority 5: Mandatory A2DP baseline (Bitpool 53) */
} dial_bt_codec_t;

typedef enum {
    DIAL_LDAC_MAX = 0,  /* 990 kbps (Default high quality) */
    DIAL_LDAC_MID = 1,  /* 660 kbps (Step-down on RSSI < -78 dBm) */
    DIAL_LDAC_LOW = 2   /* 330 kbps */
} dial_ldac_quality_t;

/* Fast-Cache Bluetooth Peer Entry (Persisted in NVS / /mnt/sdcard/.dialos/bt_cache.bin)
 * Index 0 is always the Most Recently Connected (Default) peer.
 * On BT enable, DialOS issues a unicast Direct Page (HCI_Create_Connection) to
 * peer_cache[0].bd_addr before any inquiry scan (< 400 ms lock).
 */
#define DIAL_BT_CACHE_MAX_PEERS 8
#define DIAL_BT_DIRECT_PAGE_TIMEOUT_MS 1800

typedef struct {
    uint8_t             bd_addr[6];       /* 6-byte Bluetooth MAC address */
    char                name[64];         /* Friendly UTF-8 peer name */
    uint16_t            clock_offset;     /* Cached baseband clock offset for <400ms Direct Page */
    uint8_t             page_scan_mode;   /* Cached page scan repetition mode (R0/R1/R2) */
    uint8_t             remote_seid;      /* Cached AVDTP Stream Endpoint ID (skips discovery) */
    dial_bt_codec_t     preferred_codec;  /* Remembered codec for this device */
    dial_ldac_quality_t ldac_quality;     /* Remembered LDAC quality tier */
    uint8_t             saved_volume;     /* Remembered volume (0-100) restored on connect */
    uint32_t            last_connected_ts;/* MRU timestamp (highest = Default index 0) */
} dial_bt_peer_cache_t;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Encode a DialOS (opcode, payload) command into a real FiiO Snowsky DISC mq_player wire frame:
 *   - "0202"            -> "02020008"
 *   - "0201" + "play"   -> "0201000C0001"
 *   - "0201" + "pause"  -> "0201000C0000"
 *   - "0103" + "<ms>"   -> "01030010<8hex_ms>"
 *   - "0715" + "<vol>"  -> "0715000C<4hex_vol_0_120>"
 *   - generic 4-char tag-> "<TAG>000C<4hex_val>"
 */
static inline size_t dial_ipc_format_frame(char *out_frame,
                                           size_t cap,
                                           const char *opcode,
                                           const char *payload) {
    if (!out_frame || cap < 16 || !opcode) return 0;
    const char *p = payload ? payload : "";

    if (strcmp(opcode, DIAL_CMD_GET_STATE) == 0) {
        int n = snprintf(out_frame, cap, "02020008");
        return (n > 0) ? (size_t)n : 0;
    }
    if (strcmp(opcode, DIAL_CMD_PLAY_PAUSE) == 0) {
        unsigned val = (strcmp(p, "pause") == 0 || strcmp(p, "0") == 0) ? 0u : 1u;
        int n = snprintf(out_frame, cap, "0201000C%04X", val);
        return (n > 0) ? (size_t)n : 0;
    }
    if (strcmp(opcode, DIAL_CMD_SEEK) == 0) {
        unsigned long ms = strtoul(p, NULL, 10);
        int n = snprintf(out_frame, cap, "01030010%08lX", ms & 0xFFFFFFFFUL);
        return (n > 0) ? (size_t)n : 0;
    }
    if (strcmp(opcode, DIAL_CMD_VOL_SET) == 0) {
        long vol = strtol(p, NULL, 10);
        if (vol < 0) vol = 0;
        if (vol > 100) vol = 100;
        unsigned hw_vol_120 = (unsigned)((vol * 120 + 50) / 100);
        int n = snprintf(out_frame, cap, "0715000C%04X", hw_vol_120);
        return (n > 0) ? (size_t)n : 0;
    }
    if (strcmp(opcode, DIAL_CMD_GAIN_MODE) == 0 || strcmp(opcode, DIAL_CMD_GAIN_V257) == 0) {
        unsigned high = (strcmp(p, "high") == 0 || strcmp(p, "1") == 0) ? 1u : 0u;
        int n = snprintf(out_frame, cap, "0649000C%04X", high);
        return (n > 0) ? (size_t)n : 0;
    }
    if (strcmp(opcode, DIAL_CMD_DAC_FILTER) == 0 || strcmp(opcode, DIAL_CMD_DAC_FILTER_HW) == 0) {
        unsigned filt = (unsigned)strtoul(p, NULL, 10);
        int n = snprintf(out_frame, cap, "0653000C%04X", filt & 0xFFFFu);
        return (n > 0) ? (size_t)n : 0;
    }
    unsigned val = (unsigned)strtoul(p, NULL, 10);
    int n = snprintf(out_frame, cap, "%.4s000C%04X", opcode, val & 0xFFFFu);
    return (n > 0) ? (size_t)n : 0;
}

#endif /* DIAL_IPC_H */

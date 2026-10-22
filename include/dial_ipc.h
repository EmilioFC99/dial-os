#ifndef DIAL_IPC_H
#define DIAL_IPC_H

#include <stdint.h>
#include <stdbool.h>

/* POSIX Message Queue Endpoints (FiiO Snowsky DISC Backend) */
#define DIAL_MQ_PLAYER_NAME     "/player"  /* Commands: UI -> Audio Engine */
#define DIAL_MQ_UI_NAME         "/ui"      /* State & Metadata: Audio Engine -> UI */
#define DIAL_MQ_MAX_MSG_SIZE    8192
#define DIAL_MQ_MAX_MSGS        20

/* Reverse-Engineered FiiO Command Tags (4 ASCII chars) */
#define DIAL_CMD_PLAY_LIST      "0100"  /* Play item by list index */
#define DIAL_CMD_SEEK           "0103"  /* Seek position in milliseconds (from 0°-359° angle) */
#define DIAL_CMD_FAVORITE       "0104"  /* Favorite / Unfavorite toggle */
#define DIAL_CMD_PLAY_PAUSE     "0201"  /* Toggle play/pause transport */
#define DIAL_CMD_GET_STATE      "0202"  /* Request full player status snapshot */
#define DIAL_CMD_SET_SOURCE     "0657"  /* Change playback source / operating mode */
#define DIAL_CMD_SET_OUTPUT     "0666"  /* Switch output (Headphone, Coaxial, USB) */
#define DIAL_CMD_SET_EQ         "0689"  /* Apply EQ profile */
#define DIAL_CMD_BT_CODEC       "06b3"  /* Set Bluetooth codec (priority ladder) */
#define DIAL_CMD_BT_LDAC_QUAL   "06b4"  /* Set LDAC quality (990 / 660 / 330 kbps) */
#define DIAL_CMD_GAIN_MODE      "06c1"  /* Toggle CS43131 analogue gain relay (low / high) */
#define DIAL_CMD_DAC_FILTER     "06c2"  /* Set CS43131 reconstruction filter (0..4) */

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

#endif /* DIAL_IPC_H */

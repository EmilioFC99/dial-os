#ifndef DIAL_BT_H
#define DIAL_BT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "dial_ipc.h"
#include "dial_hal.h"

/* Bitmask of remote SEP codec capabilities */
#define DIAL_BT_CAP_SBC     (1u << 0)
#define DIAL_BT_CAP_AAC     (1u << 1)
#define DIAL_BT_CAP_APTX    (1u << 2)
#define DIAL_BT_CAP_APTX_HD (1u << 3)
#define DIAL_BT_CAP_LDAC    (1u << 4)

#define DIAL_CMD_BT_DIRECT_PAGE "0503" /* Unicast HCI_Create_Connection with cached clock_offset */

typedef struct {
    dial_bt_peer_cache_t peers[DIAL_BT_CACHE_MAX_PEERS];
    size_t               peer_count;
    bool                 bt_enabled;
    bool                 connected;
    bool                 inquiry_scan_started; /* Must remain false during <400ms Direct Page */
    size_t               active_peer_idx;
    uint16_t             last_page_latency_ms; /* < 400 ms on cached Direct Page hit */
    dial_bt_codec_t      active_codec;
    dial_ldac_quality_t  active_ldac_quality;
    uint8_t              restored_volume;
} dial_bt_manager_t;

/* Initialize manager with the 3 Fjäll SET-07 reference peers (WH-1000XM5 Default at index 0) */
void dial_bt_init(dial_bt_manager_t *mgr);

/* Enable Bluetooth and issue <400ms unicast Direct Page to peers[0] BEFORE any inquiry scan */
bool dial_bt_enable_fast_connect(dial_bt_manager_t *mgr, bool peer0_reachable, int8_t rssi_dbm);

/* Evaluate remote SEP codec mask against the priority ladder:
 * LDAC 990 -> LDAC 660/330 (RSSI < -78 dBm) -> aptX HD -> aptX -> AAC -> SBC */
void dial_bt_negotiate_codec_ladder(uint32_t remote_cap_mask,
                                    int8_t rssi_dbm,
                                    dial_bt_codec_t *out_codec,
                                    dial_ldac_quality_t *out_ldac_qual);

/* Connect to peer at `index`, promote it to MRU index 0 (Default), and restore its saved_volume */
bool dial_bt_connect_and_promote(dial_bt_manager_t *mgr, size_t index, uint32_t timestamp_sec);

/* Update saved volume for the currently connected peer (persisted per MAC address) */
void dial_bt_update_peer_volume(dial_bt_manager_t *mgr, uint8_t volume);

/* Persist and reload MRU peer cache to/from NVS file */
bool dial_bt_save_cache(const dial_bt_manager_t *mgr, const char *path);
bool dial_bt_load_cache(dial_bt_manager_t *mgr, const char *path);

/* Format short label for UI chips (e.g. "LDAC 990", "aptX HD", "aptX", "AAC", "SBC") */
const char *dial_bt_codec_label(dial_bt_codec_t codec, dial_ldac_quality_t ldac_qual);

#endif /* DIAL_BT_H */

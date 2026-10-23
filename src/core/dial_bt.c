#include "dial_bt.h"
#include <stdio.h>
#include <string.h>

void dial_bt_init(dial_bt_manager_t *mgr) {
    if (!mgr) return;
    memset(mgr, 0, sizeof(*mgr));
    mgr->peer_count        = 0;
    mgr->bt_enabled        = false;
    mgr->connected         = false;
    mgr->inquiry_scan_started = false;
    mgr->active_peer_idx   = 0;
    mgr->active_codec      = DIAL_BT_CODEC_SBC;
    mgr->active_ldac_quality = DIAL_LDAC_MAX;
    mgr->restored_volume   = 50;
}

bool dial_bt_register_peer(dial_bt_manager_t *mgr, const dial_bt_peer_cache_t *peer) {
    if (!mgr || !peer) return false;
    for (size_t i = 0; i < mgr->peer_count; ++i) {
        if (memcmp(mgr->peers[i].bd_addr, peer->bd_addr, 6) == 0) {
            mgr->peers[i] = *peer;
            return true;
        }
    }
    if (mgr->peer_count >= DIAL_BT_CACHE_MAX_PEERS) return false;
    mgr->peers[mgr->peer_count++] = *peer;
    return true;
}

void dial_bt_negotiate_codec_ladder(uint32_t remote_cap_mask,
                                    int8_t rssi_dbm,
                                    dial_bt_codec_t *out_codec,
                                    dial_ldac_quality_t *out_ldac_qual) {
    dial_bt_codec_t codec = DIAL_BT_CODEC_SBC;
    dial_ldac_quality_t qual = DIAL_LDAC_MAX;

    if (remote_cap_mask & DIAL_BT_CAP_LDAC) {
        codec = DIAL_BT_CODEC_LDAC;
        if (rssi_dbm >= -78) {
            qual = DIAL_LDAC_MAX; /* 990 kbps */
        } else if (rssi_dbm >= -85) {
            qual = DIAL_LDAC_MID; /* 660 kbps step-down */
        } else {
            qual = DIAL_LDAC_LOW; /* 330 kbps step-down */
        }
    } else if (remote_cap_mask & DIAL_BT_CAP_APTX_HD) {
        codec = DIAL_BT_CODEC_APTX_HD;
    } else if (remote_cap_mask & DIAL_BT_CAP_APTX) {
        codec = DIAL_BT_CODEC_APTX;
    } else if (remote_cap_mask & DIAL_BT_CAP_AAC) {
        codec = DIAL_BT_CODEC_AAC;
    } else {
        codec = DIAL_BT_CODEC_SBC;
    }

    if (out_codec)     *out_codec = codec;
    if (out_ldac_qual) *out_ldac_qual = qual;
}

bool dial_bt_enable_fast_connect(dial_bt_manager_t *mgr, bool peer0_reachable, int8_t rssi_dbm) {
    if (!mgr) return false;
    mgr->bt_enabled = true;

    /* Locked Rule #8: Issue unicast Direct Page to peer_cache[0].bd_addr BEFORE any inquiry scan */
    if (mgr->peer_count > 0) {
        const dial_bt_peer_cache_t *def = &mgr->peers[0];
        char payload[64];
        snprintf(payload, sizeof(payload), "%02X:%02X:%02X:%02X:%02X:%02X@0x%04X/seid=%u",
                 def->bd_addr[0], def->bd_addr[1], def->bd_addr[2],
                 def->bd_addr[3], def->bd_addr[4], def->bd_addr[5],
                 def->clock_offset, def->remote_seid);
        dial_hal_send_ipc(DIAL_CMD_BT_DIRECT_PAGE, payload);

        if (peer0_reachable) {
            mgr->inquiry_scan_started = false;
            mgr->connected            = true;
            mgr->active_peer_idx      = 0;
            mgr->last_page_latency_ms = 265; /* < 400 ms direct page lock */

            /* Fast-path SEID configuration + RSSI-aware LDAC quality selection */
            mgr->active_codec = def->preferred_codec;
            if (def->preferred_codec == DIAL_BT_CODEC_LDAC) {
                mgr->active_ldac_quality = (rssi_dbm < -78) ? DIAL_LDAC_MID : def->ldac_quality;
            } else {
                mgr->active_ldac_quality = def->ldac_quality;
            }
            mgr->restored_volume = def->saved_volume;

            char c_buf[16];
            snprintf(c_buf, sizeof(c_buf), "%d", (int)mgr->active_codec);
            dial_hal_send_ipc(DIAL_CMD_BT_CODEC, c_buf);

            if (mgr->active_codec == DIAL_BT_CODEC_LDAC) {
                char q_buf[16];
                snprintf(q_buf, sizeof(q_buf), "%d", (int)mgr->active_ldac_quality);
                dial_hal_send_ipc(DIAL_CMD_BT_LDAC_QUAL, q_buf);
            }
            return true;
        }
    }

    /* Fallback if peer_cache[0] timed out after 1800ms: start background inquiry scan */
    mgr->connected            = false;
    mgr->inquiry_scan_started = true;
    mgr->last_page_latency_ms = DIAL_BT_DIRECT_PAGE_TIMEOUT_MS;
    return false;
}

bool dial_bt_connect_and_promote(dial_bt_manager_t *mgr, size_t index, uint32_t timestamp_sec) {
    if (!mgr || index >= mgr->peer_count) return false;

    dial_bt_peer_cache_t chosen = mgr->peers[index];
    chosen.last_connected_ts = timestamp_sec;

    /* Shift 0 .. index-1 down by 1 so chosen becomes index 0 (Default) */
    for (size_t i = index; i > 0; --i) {
        mgr->peers[i] = mgr->peers[i - 1];
    }
    mgr->peers[0] = chosen;

    mgr->connected            = true;
    mgr->inquiry_scan_started = false;
    mgr->active_peer_idx      = 0;
    mgr->active_codec         = chosen.preferred_codec;
    mgr->active_ldac_quality  = chosen.ldac_quality;
    mgr->restored_volume      = chosen.saved_volume;
    mgr->last_page_latency_ms = 240;
    return true;
}

void dial_bt_update_peer_volume(dial_bt_manager_t *mgr, uint8_t volume) {
    if (!mgr || mgr->peer_count == 0) return;
    if (volume > 100) volume = 100;
    mgr->peers[0].saved_volume = volume;
    mgr->restored_volume       = volume;
}

bool dial_bt_save_cache(const dial_bt_manager_t *mgr, const char *path) {
    if (!mgr || !path) return false;
    FILE *fp = fopen(path, "wb");
    if (!fp) return false;
    uint32_t magic = 0x44425443u; /* 'DBTC' */
    uint32_t count = (uint32_t)mgr->peer_count;
    bool ok = (fwrite(&magic, sizeof(magic), 1, fp) == 1) &&
              (fwrite(&count, sizeof(count), 1, fp) == 1) &&
              (fwrite(mgr->peers, sizeof(dial_bt_peer_cache_t), mgr->peer_count, fp) == mgr->peer_count);
    fclose(fp);
    return ok;
}

bool dial_bt_load_cache(dial_bt_manager_t *mgr, const char *path) {
    if (!mgr || !path) return false;
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;
    uint32_t magic = 0, count = 0;
    if (fread(&magic, sizeof(magic), 1, fp) != 1 || magic != 0x44425443u ||
        fread(&count, sizeof(count), 1, fp) != 1 || count > DIAL_BT_CACHE_MAX_PEERS) {
        fclose(fp);
        return false;
    }
    memset(mgr->peers, 0, sizeof(mgr->peers));
    if (fread(mgr->peers, sizeof(dial_bt_peer_cache_t), count, fp) != count) {
        fclose(fp);
        return false;
    }
    fclose(fp);
    mgr->peer_count          = count;
    mgr->active_peer_idx     = 0;
    if (count > 0) {
        mgr->active_codec        = mgr->peers[0].preferred_codec;
        mgr->active_ldac_quality = mgr->peers[0].ldac_quality;
        mgr->restored_volume     = mgr->peers[0].saved_volume;
    }
    return true;
}

const char *dial_bt_codec_label(dial_bt_codec_t codec, dial_ldac_quality_t ldac_qual) {
    switch (codec) {
        case DIAL_BT_CODEC_LDAC:
            if (ldac_qual == DIAL_LDAC_MAX) return "LDAC 990";
            if (ldac_qual == DIAL_LDAC_MID) return "LDAC 660";
            return "LDAC 330";
        case DIAL_BT_CODEC_APTX_HD: return "aptX HD";
        case DIAL_BT_CODEC_APTX:    return "aptX";
        case DIAL_BT_CODEC_AAC:     return "AAC";
        case DIAL_BT_CODEC_SBC:
        default:                    return "SBC";
    }
}

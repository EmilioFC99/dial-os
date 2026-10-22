#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dial_bt.h"

int main(void) {
    assert(dial_hal_init(true));

    dial_bt_manager_t mgr;
    dial_bt_init(&mgr);

    /* 1. Verify Default (index 0) is WH-1000XM5 with LDAC 990 & saved_volume 78 */
    assert(mgr.peer_count == 3);
    assert(strcmp(mgr.peers[0].name, "WH-1000XM5") == 0);
    assert(mgr.peers[0].preferred_codec == DIAL_BT_CODEC_LDAC);
    assert(mgr.peers[0].ldac_quality == DIAL_LDAC_MAX);
    assert(mgr.peers[0].saved_volume == 78);

    /* 2. Enabling Bluetooth issues <400ms Direct Page to peers[0] BEFORE any inquiry scan */
    dial_hal_reset_stats();
    assert(dial_bt_enable_fast_connect(&mgr, true, -52));
    assert(mgr.connected == true);
    assert(mgr.inquiry_scan_started == false);
    assert(mgr.last_page_latency_ms < 400);
    assert(mgr.active_codec == DIAL_BT_CODEC_LDAC);
    assert(mgr.active_ldac_quality == DIAL_LDAC_MAX);
    assert(mgr.restored_volume == 78);

    dial_ipc_msg_t msg;
    assert(dial_hal_pop_ipc(&msg));
    assert(strcmp(msg.opcode, DIAL_CMD_BT_DIRECT_PAGE) == 0);
    assert(strstr(msg.payload, "94:DB:56:11:22:33") != NULL);

    /* 3. RSSI < -78 dBm steps LDAC down from 990 kbps to 660 kbps */
    assert(dial_bt_enable_fast_connect(&mgr, true, -81));
    assert(mgr.active_codec == DIAL_BT_CODEC_LDAC);
    assert(mgr.active_ldac_quality == DIAL_LDAC_MID);
    assert(strcmp(dial_bt_codec_label(mgr.active_codec, mgr.active_ldac_quality), "LDAC 660") == 0);

    /* 4. Unreachable Default peer falls back to inquiry scan after 1800ms timeout */
    assert(!dial_bt_enable_fast_connect(&mgr, false, -90));
    assert(mgr.connected == false);
    assert(mgr.inquiry_scan_started == true);
    assert(mgr.last_page_latency_ms == DIAL_BT_DIRECT_PAGE_TIMEOUT_MS);

    /* 5. Priority ladder: LDAC 990 -> LDAC 660/330 -> aptX HD -> aptX -> AAC -> SBC */
    dial_bt_codec_t c;
    dial_ldac_quality_t q;
    dial_bt_negotiate_codec_ladder(DIAL_BT_CAP_SBC | DIAL_BT_CAP_AAC | DIAL_BT_CAP_APTX | DIAL_BT_CAP_APTX_HD | DIAL_BT_CAP_LDAC, -60, &c, &q);
    assert(c == DIAL_BT_CODEC_LDAC && q == DIAL_LDAC_MAX);

    dial_bt_negotiate_codec_ladder(DIAL_BT_CAP_SBC | DIAL_BT_CAP_LDAC, -88, &c, &q);
    assert(c == DIAL_BT_CODEC_LDAC && q == DIAL_LDAC_LOW);

    dial_bt_negotiate_codec_ladder(DIAL_BT_CAP_SBC | DIAL_BT_CAP_AAC | DIAL_BT_CAP_APTX | DIAL_BT_CAP_APTX_HD, -60, &c, &q);
    assert(c == DIAL_BT_CODEC_APTX_HD);

    dial_bt_negotiate_codec_ladder(DIAL_BT_CAP_SBC | DIAL_BT_CAP_AAC | DIAL_BT_CAP_APTX, -60, &c, &q);
    assert(c == DIAL_BT_CODEC_APTX);

    dial_bt_negotiate_codec_ladder(DIAL_BT_CAP_SBC | DIAL_BT_CAP_AAC, -60, &c, &q);
    assert(c == DIAL_BT_CODEC_AAC);

    dial_bt_negotiate_codec_ladder(DIAL_BT_CAP_SBC, -60, &c, &q);
    assert(c == DIAL_BT_CODEC_SBC);

    /* 6. Connecting peer #1 (Sennheiser M4) promotes it to MRU index 0 (Default) & restores volume 72 */
    assert(dial_bt_connect_and_promote(&mgr, 1, 1729630000u));
    assert(strcmp(mgr.peers[0].name, "Sennheiser M4") == 0);
    assert(strcmp(mgr.peers[1].name, "WH-1000XM5") == 0);
    assert(mgr.active_codec == DIAL_BT_CODEC_APTX);
    assert(mgr.restored_volume == 72);

    /* 7. Update saved volume per MAC address & verify NVS persistence round-trip */
    dial_bt_update_peer_volume(&mgr, 84);
    assert(mgr.peers[0].saved_volume == 84);
    assert(dial_bt_save_cache(&mgr, "/tmp/dialos_bt_cache_test.bin"));

    dial_bt_manager_t loaded;
    memset(&loaded, 0, sizeof(loaded));
    assert(dial_bt_load_cache(&loaded, "/tmp/dialos_bt_cache_test.bin"));
    assert(loaded.peer_count == 3);
    assert(strcmp(loaded.peers[0].name, "Sennheiser M4") == 0);
    assert(loaded.peers[0].saved_volume == 84);
    assert(loaded.restored_volume == 84);

    dial_hal_shutdown();
    printf("[PASS] test_bt_cache: MRU Default peer, <400ms Direct Page, codec priority ladder & NVS cache verified.\n");
    return 0;
}

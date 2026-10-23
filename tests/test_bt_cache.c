#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "dial_bt.h"

int main(void) {
    dial_bt_manager_t mgr;
    dial_bt_init(&mgr);

    /* 1. Verify fresh init has zero fake peers and starts disconnected */
    assert(mgr.peer_count == 0);
    assert(!mgr.bt_enabled);
    assert(!mgr.connected);

    /* 2. Register two dynamic peers */
    dial_bt_peer_cache_t p0 = {0};
    const uint8_t mac0[6] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
    memcpy(p0.bd_addr, mac0, 6);
    strncpy(p0.name, "Peer-A", sizeof(p0.name) - 1);
    p0.clock_offset    = 0x4A2Cu;
    p0.remote_seid     = 5;
    p0.preferred_codec = DIAL_BT_CODEC_LDAC;
    p0.ldac_quality    = DIAL_LDAC_MAX;
    p0.saved_volume    = 78;
    assert(dial_bt_register_peer(&mgr, &p0));

    dial_bt_peer_cache_t p1 = {0};
    const uint8_t mac1[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
    memcpy(p1.bd_addr, mac1, 6);
    strncpy(p1.name, "Peer-B", sizeof(p1.name) - 1);
    p1.clock_offset    = 0x3110u;
    p1.remote_seid     = 3;
    p1.preferred_codec = DIAL_BT_CODEC_APTX;
    p1.ldac_quality    = DIAL_LDAC_MAX;
    p1.saved_volume    = 72;
    assert(dial_bt_register_peer(&mgr, &p1));
    assert(mgr.peer_count == 2);

    /* 3. Verify unicast Direct Page (<400 ms) BEFORE inquiry scan when peer 0 is reachable */
    dial_hal_reset_stats();
    bool fast_ok = dial_bt_enable_fast_connect(&mgr, true, -62);
    assert(fast_ok);
    assert(mgr.connected);
    assert(!mgr.inquiry_scan_started);
    assert(mgr.last_page_latency_ms < 400);
    assert(mgr.restored_volume == 78);

    /* 4. Verify codec negotiation priority ladder */
    dial_bt_codec_t c;
    dial_ldac_quality_t q;
    dial_bt_negotiate_codec_ladder(DIAL_BT_CAP_SBC | DIAL_BT_CAP_AAC | DIAL_BT_CAP_APTX | DIAL_BT_CAP_LDAC, -65, &c, &q);
    assert(c == DIAL_BT_CODEC_LDAC && q == DIAL_LDAC_MAX);

    /* 5. Verify MRU promotion and NVS persistence */
    assert(dial_bt_connect_and_promote(&mgr, 1, 1730000000u));
    assert(strcmp(mgr.peers[0].name, "Peer-B") == 0);
    assert(mgr.restored_volume == 72);

    const char *tmp_cache = "/tmp/dialos_test_bt_cache.bin";
    assert(dial_bt_save_cache(&mgr, tmp_cache));
    dial_bt_manager_t loaded;
    dial_bt_init(&loaded);
    assert(dial_bt_load_cache(&loaded, tmp_cache));
    assert(loaded.peer_count == 2);
    assert(strcmp(loaded.peers[0].name, "Peer-B") == 0);
    unlink(tmp_cache);

    printf("[PASS] test_bt_cache (zero fake peers on init, dynamic MRU & Direct Page verified)\n");
    return 0;
}

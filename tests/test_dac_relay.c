#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dial_dac.h"
#include "dial_hal.h"
#include "dial_ipc.h"

int main(void) {
    assert(dial_hal_init(true));
    dial_hal_reset_stats();

    dial_dac_state_t dac;
    dial_dac_init(&dac, 80u, false);
    assert(dac.phase == DIAL_DAC_SEQ_IDLE);
    assert(dac.relay_high_gain == false);
    assert(dac.current_hw_volume == 80u);

    /* 1. Request transition Low -> High (+6 dB) at t = 1000 ms */
    assert(dial_dac_request_gain(&dac, true, 1000u));
    assert(dac.phase == DIAL_DAC_SEQ_RAMP_DOWN);
    assert(dac.relay_high_gain == false); /* Relay MUST NOT flip before mute completes */

    /* At t = 1004 ms (4ms into 8ms ramp-down), volume is 50% (40), relay still false */
    assert(dial_dac_tick(&dac, 1004u) == DIAL_DAC_SEQ_RAMP_DOWN);
    assert(dac.current_hw_volume == 40u);
    assert(dac.relay_high_gain == false);
    assert(dac.relay_switch_count == 0u);

    /* At t = 1008 ms (8ms elapsed): volume hits 0, GPIO_PD14 relay switches to High */
    assert(dial_dac_tick(&dac, 1008u) == DIAL_DAC_SEQ_SETTLING);
    assert(dac.current_hw_volume == 0u);
    assert(dac.relay_high_gain == true);
    assert(dac.relay_switch_count == 1u);

    dial_ipc_msg_t msg;
    assert(dial_hal_pop_ipc(&msg));
    assert(strcmp(msg.opcode, DIAL_CMD_GAIN_MODE) == 0);
    assert(strcmp(msg.payload, "high") == 0);

    /* At t = 1015 ms (7ms into 12ms contact bounce settle): still muted at 0 */
    assert(dial_dac_tick(&dac, 1015u) == DIAL_DAC_SEQ_SETTLING);
    assert(dac.current_hw_volume == 0u);

    /* At t = 1024 ms (4ms into 8ms ramp-up): volume ramps back through 40 */
    assert(dial_dac_tick(&dac, 1024u) == DIAL_DAC_SEQ_RAMP_UP);
    assert(dac.current_hw_volume == 40u);

    /* At t = 1028 ms (28ms total): sequence completes and restores full volume 80 */
    assert(dial_dac_tick(&dac, 1028u) == DIAL_DAC_SEQ_IDLE);
    assert(dac.current_hw_volume == 80u);
    assert(dac.relay_high_gain == true);
    assert(dac.relay_switch_count == 1u);

    /* 2. Verify CS43131 reconstruction filter selection (SET-03) */
    assert(dial_dac_set_filter(&dac, DIAL_DAC_FILTER_SLOW_LINEAR));
    assert(dac.active_filter == DIAL_DAC_FILTER_SLOW_LINEAR);
    assert(dial_hal_pop_ipc(&msg));
    assert(strcmp(msg.opcode, DIAL_CMD_DAC_FILTER) == 0);
    assert(strcmp(msg.payload, "1") == 0);

    dial_hal_shutdown();
    printf("[PASS] test_dac_relay: 8ms/12ms/8ms CS43131 anti-pop relay sequencer & filter IPC verified.\n");
    return 0;
}

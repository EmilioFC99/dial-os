#include "dial_dac.h"
#include "dial_hal.h"
#include "dial_ipc.h"
#include <stdio.h>
#include <string.h>

void dial_dac_init(dial_dac_state_t *dac, uint8_t initial_vol, bool high_gain) {
    if (!dac) return;
    memset(dac, 0, sizeof(*dac));
    if (initial_vol > 100u) initial_vol = 100u;
    dac->phase              = DIAL_DAC_SEQ_IDLE;
    dac->relay_high_gain    = high_gain;
    dac->pending_high_gain  = high_gain;
    dac->user_volume        = initial_vol;
    dac->current_hw_volume  = initial_vol;
    dac->active_filter      = DIAL_DAC_FILTER_FAST_LINEAR;
}

bool dial_dac_request_gain(dial_dac_state_t *dac, bool high_gain, uint32_t now_ms) {
    if (!dac) return false;
    if (dac->phase == DIAL_DAC_SEQ_IDLE && dac->relay_high_gain == high_gain) {
        return false;
    }
    dac->pending_high_gain = high_gain;
    dac->seq_start_ms      = now_ms;
    dac->phase             = DIAL_DAC_SEQ_RAMP_DOWN;
    return true;
}

dial_dac_seq_phase_t dial_dac_tick(dial_dac_state_t *dac, uint32_t now_ms) {
    if (!dac || dac->phase == DIAL_DAC_SEQ_IDLE) {
        return DIAL_DAC_SEQ_IDLE;
    }

    uint32_t elapsed = (now_ms >= dac->seq_start_ms) ? (now_ms - dac->seq_start_ms) : 0u;

    if (elapsed < DIAL_DAC_RAMP_DOWN_MS) {
        /* Phase 1 (0..8 ms): Linearly ramp volume down to 0 */
        dac->phase = DIAL_DAC_SEQ_RAMP_DOWN;
        uint32_t rem = DIAL_DAC_RAMP_DOWN_MS - elapsed;
        dac->current_hw_volume = (uint8_t)(((uint32_t)dac->user_volume * rem) / DIAL_DAC_RAMP_DOWN_MS);
    } else if (elapsed < (DIAL_DAC_RAMP_DOWN_MS + DIAL_DAC_BOUNCE_WAIT_MS)) {
        /* Phase 2 (8..20 ms): Digital mute = 0; switch GPIO_PD14 relay and wait 12 ms */
        dac->current_hw_volume = 0u;
        if (dac->relay_high_gain != dac->pending_high_gain) {
            dac->relay_high_gain = dac->pending_high_gain;
            dac->relay_switch_count++;
            dial_hal_send_ipc(DIAL_CMD_GAIN_MODE, dac->relay_high_gain ? "high" : "low");
        }
        dac->phase = DIAL_DAC_SEQ_SETTLING;
    } else if (elapsed < DIAL_DAC_TOTAL_SEQ_MS) {
        /* Phase 3 (20..28 ms): Ensure relay flipped, ramp volume from 0 back to user_volume */
        if (dac->relay_high_gain != dac->pending_high_gain) {
            dac->relay_high_gain = dac->pending_high_gain;
            dac->relay_switch_count++;
            dial_hal_send_ipc(DIAL_CMD_GAIN_MODE, dac->relay_high_gain ? "high" : "low");
        }
        dac->phase = DIAL_DAC_SEQ_RAMP_UP;
        uint32_t up_elapsed = elapsed - (DIAL_DAC_RAMP_DOWN_MS + DIAL_DAC_BOUNCE_WAIT_MS);
        dac->current_hw_volume = (uint8_t)(((uint32_t)dac->user_volume * up_elapsed) / DIAL_DAC_RAMP_UP_MS);
    } else {
        /* Sequence complete (>= 28 ms) */
        if (dac->relay_high_gain != dac->pending_high_gain) {
            dac->relay_high_gain = dac->pending_high_gain;
            dac->relay_switch_count++;
            dial_hal_send_ipc(DIAL_CMD_GAIN_MODE, dac->relay_high_gain ? "high" : "low");
        }
        dac->current_hw_volume = dac->user_volume;
        dac->phase             = DIAL_DAC_SEQ_IDLE;
    }

    return dac->phase;
}

bool dial_dac_set_filter(dial_dac_state_t *dac, dial_dac_filter_t filter) {
    if (!dac || (int)filter < 0 || (int)filter > (int)DIAL_DAC_FILTER_NOS) {
        return false;
    }
    dac->active_filter = filter;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", (int)filter);
    return dial_hal_send_ipc(DIAL_CMD_DAC_FILTER, buf);
}

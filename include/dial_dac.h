/**
 * @file dial_dac.h
 * @brief CS43131 Dual DAC Anti-Pop Sequencer & Analogue Gain Relay Controller
 *
 * Implements the hardware safety sequence from Section 4 of
 * docs/DIAL_OS_SYSTEM_REQUIREMENTS.md:
 *   1. Ramp CS43131 digital volume from target down to 0 over 8 ms
 *   2. Switch GPIO_PD14 analogue gain relay (Low <-> High +6 dB)
 *   3. Hold muted for 12 ms while relay contact bounce settles
 *   4. Ramp CS43131 digital volume from 0 back to target over 8 ms
 * Total deterministic transition duration: 28 ms with zero audible pop/click.
 */
#ifndef DIAL_DAC_H
#define DIAL_DAC_H

#include <stdint.h>
#include <stdbool.h>

#define DIAL_DAC_RAMP_DOWN_MS   8u
#define DIAL_DAC_BOUNCE_WAIT_MS 12u
#define DIAL_DAC_RAMP_UP_MS     8u
#define DIAL_DAC_TOTAL_SEQ_MS   (DIAL_DAC_RAMP_DOWN_MS + DIAL_DAC_BOUNCE_WAIT_MS + DIAL_DAC_RAMP_UP_MS)

typedef enum {
    DIAL_DAC_SEQ_IDLE       = 0,
    DIAL_DAC_SEQ_RAMP_DOWN  = 1, /* 0..8 ms: volume ramps target -> 0 */
    DIAL_DAC_SEQ_SETTLING   = 2, /* 8..20 ms: relay flipped at t=8ms, muted settle */
    DIAL_DAC_SEQ_RAMP_UP    = 3  /* 20..28 ms: volume ramps 0 -> target */
} dial_dac_seq_phase_t;

typedef enum {
    DIAL_DAC_FILTER_FAST_LINEAR = 0,
    DIAL_DAC_FILTER_SLOW_LINEAR = 1,
    DIAL_DAC_FILTER_FAST_MIN    = 2,
    DIAL_DAC_FILTER_SLOW_MIN    = 3,
    DIAL_DAC_FILTER_NOS         = 4
} dial_dac_filter_t;

typedef struct {
    dial_dac_seq_phase_t phase;
    bool                 relay_high_gain;     /* Current physical GPIO_PD14 state */
    bool                 pending_high_gain;   /* Requested target GPIO_PD14 state */
    uint8_t              user_volume;         /* 0..100 user volume */
    uint8_t              current_hw_volume;   /* 0..100 instantaneous attenuated volume */
    uint32_t             seq_start_ms;
    uint32_t             relay_switch_count;
    dial_dac_filter_t    active_filter;
} dial_dac_state_t;

/* Initialize CS43131 DAC & relay sequencer */
void dial_dac_init(dial_dac_state_t *dac, uint8_t initial_vol, bool high_gain);

/* Request a Low <-> High (+6 dB) analogue gain transition (starts 28ms anti-pop sequence) */
bool dial_dac_request_gain(dial_dac_state_t *dac, bool high_gain, uint32_t now_ms);

/* Advance the 8ms / 12ms / 8ms sequencer at timestamp `now_ms` */
dial_dac_seq_phase_t dial_dac_tick(dial_dac_state_t *dac, uint32_t now_ms);

/* Select one of the 5 CS43131 reconstruction filters (SET-03) */
bool dial_dac_set_filter(dial_dac_state_t *dac, dial_dac_filter_t filter);

#endif /* DIAL_DAC_H */

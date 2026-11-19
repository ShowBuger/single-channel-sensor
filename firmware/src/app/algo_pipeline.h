#ifndef ALGO_PIPELINE_H
#define ALGO_PIPELINE_H

/* Portable algorithm API: no board, HAL, RTOS, I/O or mutable global state. */
#include <stdint.h>
#include <stdbool.h>

#define ALGO_PROFILE_LEGACY 0U
#define ALGO_PROFILE_V2     1U
#define ALGO_WINDOW_MAX     50U
#define ALGO_SPIKE_MED_MAX  5U
#define ALGO_SPIKE_MAD_MAX  50U
#define ALGO_STAGE_COUNT    10U
#define ALGO_CALIBRATION    (1U << 0)
#define ALGO_DEAD_BAND      (1U << 1)
#define ALGO_LOW_PASS       (1U << 2)
#define ALGO_COMPENSATION   (1U << 3)
#define ALGO_MAPPING        (1U << 4)
#define ALGO_STABILITY      (1U << 5)
#define ALGO_UNIPOLAR       (1U << 6)
#define ALGO_SPIKE          (1U << 7)

typedef struct { float x, y, z, t; } AlgoVector_t;
typedef struct {
    AlgoVector_t actual, reference;
    uint8_t valid;
} AlgoPair_t;
typedef struct {
    AlgoVector_t actual_offset, reference_offset, actual_std, reference_std;
    uint8_t calibrated;
} AlgoCalibration_t;

typedef struct {
    uint8_t profile;
    uint16_t window_ms, stable_ms, tau_ms, tau_lp_ms, calib_ms;
    uint16_t spike_med_ms, spike_mad_ms, spike_confirm_ms, spike_hold_ms;
    float spike_k, stab_k;
    float gain[3];
} AlgoConfig_t;
#ifdef ALGO_V2_ONLY
#define ALGO_DEFAULT_PROFILE ALGO_PROFILE_V2
#else
#define ALGO_DEFAULT_PROFILE ALGO_PROFILE_LEGACY
#endif
#define ALGO_CONFIG_DEFAULTS { .profile=ALGO_DEFAULT_PROFILE, .window_ms=500, \
    .stable_ms=1000, .tau_ms=950, .tau_lp_ms=220, .calib_ms=1000, \
    .spike_med_ms=50, .spike_mad_ms=500, .spike_confirm_ms=30, \
    .spike_hold_ms=300, .spike_k=5.0f, .stab_k=3.0f, .gain={1.0f,-1.0f,-1.0f} }

typedef struct {
    /* First three fields retain the old runtime parameter ABI for replay. */
    float dead_value, spike_multiplier, stability_threshold;
    AlgoConfig_t config;
} AlgoParams_t;

typedef struct {
    uint32_t actual_us, reference_us, seq;
    uint16_t rate_hz;
    uint8_t timed; /* timestamps are trigger/read-observation times, not DRDY */
    uint8_t triggered; /* both devices were host-triggered, otherwise skew is unknown */
    uint8_t error; /* 0=OK, 1=bus, 2=status fault, 3=timeout, 4=pair skew */
} AlgoTiming_t;

typedef struct {
    AlgoCalibration_t calibration;
    float mapping[3];
    int8_t actual_rotation[9], reference_rotation[9];
    uint16_t legacy_settle_ms;
    float quantum_uT;
} AlgoChannelCfg_t;

typedef struct {
    float med[ALGO_SPIKE_MED_MAX], residual[ALGO_SPIKE_MAD_MAX];
    uint16_t med_index, med_count, mad_index, mad_count, pending_count;
    float pending_min, pending_max, pending_ms;
    int8_t pending_sign;
} AlgoSpikeState_t;
typedef struct {
    uint32_t received, accepted, invalid, skewed, bus_errors, faults, timeouts;
    uint32_t spikes, steps, forced_accepts;
    uint32_t stage_runs[ALGO_STAGE_COUNT];
} AlgoCounters_t;
typedef struct {
    /* Legacy state deliberately keeps its evaluation/summation order. */
#ifndef ALGO_V2_ONLY
    float ref_history[3][3], velocity[3], acceleration[3];
    uint8_t ref_index, ref_count;
    float window[3][ALGO_WINDOW_MAX];
    float reference_window[3][ALGO_WINDOW_MAX];
    uint8_t reference_window_valid[ALGO_WINDOW_MAX];
    uint32_t window_time[ALGO_WINDOW_MAX];
    uint16_t window_index, window_count, window_size;
    uint16_t legacy_stable[3];
    float legacy_stable_ms[3];
#endif
    AlgoSpikeState_t spike[2][3];
    float previous_ref[3], lp[3], reference_lp[3];
    float stable_elapsed_ms, factor;
    uint32_t last_actual_us, last_reference_us;
    uint8_t clock_initialized, ref_initialized, lp_initialized, reference_lp_initialized, db_active[3];
    AlgoCounters_t counters;
} AlgoState_t;

typedef struct {
    AlgoPair_t value;
    AlgoVector_t reference_zero;
    float sigma_eff[3];
    int32_t pair_skew_us;
    uint8_t error; /* acquisition error, or 5 for invalid numeric input/output */
} AlgoOutput_t;

void Algo_DefaultParams(AlgoParams_t *params);
void Algo_Reset(AlgoState_t *state);
/* Optional debug snapshot; absent stages are not fabricated (legacy order differs). */
typedef struct {
    float actual[ALGO_STAGE_COUNT][3], reference[ALGO_STAGE_COUNT][3];
    uint32_t seq, timestamp_us;
    uint16_t present, executed;
    uint8_t profile, valid, error;
} AlgoTrace_t;
bool Algo_ProcessTrace(const AlgoPair_t *input, const AlgoTiming_t *timing,
                  const AlgoChannelCfg_t *channel, const AlgoParams_t *params,
                  uint16_t enable, bool reference_output,
                  AlgoState_t *state, AlgoOutput_t *output,
                  uint16_t bypass, AlgoTrace_t *trace);
bool Algo_Process(const AlgoPair_t *input, const AlgoTiming_t *timing,
                  const AlgoChannelCfg_t *channel, const AlgoParams_t *params,
                  uint16_t enable, bool reference_output,
                  AlgoState_t *state, AlgoOutput_t *output);
#endif

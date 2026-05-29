/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file synth_state.h
 * @brief Thread-safe shared state for the MIDI synthesizer
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#ifndef SYNTH_STATE_H
#define SYNTH_STATE_H

#include <zephyr/kernel.h>
#include <arm_math.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Shared synthesizer state
 */
struct synth_state {
	struct k_mutex lock;        /**< Mutex for thread-safe access */
	bool gate_open;             /**< Simple square envelope gate */
	uint8_t active_note;        /**< Last MIDI note triggered */
	q15_t velocity_scale;       /**< Amplitude scale factor (Q15) derived from velocity */
	uint32_t phase_inc;         /**< Current 32-bit phase increment */
	uint32_t phase_acc;         /**< Persistent 32-bit phase accumulator */
};

/**
 * @brief Global lookup table for MIDI Note to 32-bit Phase Increment.
 * Derivied from midi1 library frequencies during startup.
 */
extern uint32_t phase_inc_lut[128];

/**
 * @brief Global lookup table mapping MIDI Velocity (0-127) to Q15 amplitude scale.
 */
extern q15_t velocity_lut[128];

#endif /* SYNTH_STATE_H */

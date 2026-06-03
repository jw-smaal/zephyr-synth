/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file synth_engine.h
 * @brief Digital Signal Processing core for the synthesizer
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#ifndef SYNTH_ENGINE_H
#define SYNTH_ENGINE_H

#include <stdint.h>

/**
 * @brief Event types for the synthesizer message queue
 */
enum synth_event_type {
	SYNTH_EVT_NOTE_ON,
	SYNTH_EVT_NOTE_OFF,
};

/**
 * @brief Synthesizer event structure
 */
struct synth_event {
	enum synth_event_type type;
	uint8_t note;
	uint8_t velocity;
};

/**
 * @brief Submit an event to the synthesizer core
 *
 * @param evt Pointer to the event structure to submit
 */
void synth_submit_event(struct synth_event *evt);

/**
 * @brief Initialize the synthesizer DSP core
 */
void synth_engine_init(void);

/**
 * @brief Render a block of audio samples
 *
 * @param buffer Output buffer (interleaved stereo)
 * @param samples Number of samples per channel to render
 */
void synth_engine_render_block(int16_t *buffer, uint32_t samples);

#endif /* SYNTH_ENGINE_H */

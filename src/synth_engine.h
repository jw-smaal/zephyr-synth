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
#include "adsr.h"

/** @brief Maximum waveform layers per patch */
#define PATCH_MAX_LAYERS 2

/** @brief Maximum unison voices per note per layer */
#define PATCH_MAX_UNISON 4

/**
 * @brief Event types for the synthesizer message queue
 */
enum synth_event_type {
	SYNTH_EVT_NOTE_ON,
	SYNTH_EVT_NOTE_OFF,
	SYNTH_EVT_SET_PATCH, /**< Change the globally active patch */
};

/**
 * @brief Waveform identifiers.
 *
 * The order must match the wave_table[] pointer array in synth_engine.c.
 * New waveforms must be inserted before SYNTH_WAVE_COUNT.
 */
enum synth_wave {
	SYNTH_WAVE_SINE,
	SYNTH_WAVE_SAW,
	SYNTH_WAVE_SQUARE,
	SYNTH_WAVE_TRI,
	SYNTH_WAVE_COUNT, /**< Sentinel — number of waveforms; do not use as a waveform */
};

/**
 * @brief Synthesizer patch (preset) descriptor.
 *
 * Each patch fully specifies the timbre: waveform layers, octave offsets,
 * unison voice count, detune spread, and ADSR envelope timing.  The
 * effective polyphony is MAX_VOICES / (layer_count * unison_count).
 */
struct patch {
	const char       *name;                          /**< Human-readable preset name */
	uint8_t           layer_count;                   /**< Number of waveform layers (1..PATCH_MAX_LAYERS) */
	enum synth_wave   wave[PATCH_MAX_LAYERS];         /**< Waveform per layer */
	int8_t            octave_shift[PATCH_MAX_LAYERS]; /**< Semitone octave offset per layer */
	uint8_t           unison_count;                  /**< Detuned voices per note per layer (1..PATCH_MAX_UNISON) */
	int16_t           detune_cents;                  /**< Total detune spread in cents; 0 = no detune */
	struct adsr_param env;                           /**< Envelope parameters for this patch */
};

/**
 * @brief Synthesizer event structure passed through the message queue
 */
struct synth_event {
	enum synth_event_type type;
	uint8_t patch_idx; /**< Active patch index (NOTE_ON and SET_PATCH events) */
	uint8_t note;      /**< MIDI note number */
	uint8_t velocity;  /**< MIDI velocity */
};

/**
 * @brief Submit an event to the synthesizer core.
 *
 * Safe to call from any thread or ISR context.  Drops the event silently
 * if the internal queue is full, to avoid blocking the caller.
 *
 * @param evt Pointer to the event to submit
 */
void synth_submit_event(struct synth_event *evt);

/**
 * @brief Initialize the synthesizer DSP core
 */
void synth_engine_init(void);

/**
 * @brief Render a block of audio samples.
 *
 * Processes all pending events, advances ADSR envelopes, and fills the
 * output buffer with interleaved stereo Q1.15 samples.
 *
 * @param buffer  Output buffer (interleaved stereo, Q1.15)
 * @param samples Number of samples per channel to render
 */
void synth_engine_render_block(int16_t *buffer, uint32_t samples);

/**
 * @brief Return the index of the currently active patch.
 *
 * Safe to call from any thread context (uses atomic read).
 *
 * @return Patch index into the internal patch table
 */
int synth_get_active_patch_idx(void);

/**
 * @brief Return a pointer to a patch by index.
 *
 * @param idx Patch index
 * @return Pointer to the @ref patch, or NULL if out of range
 */
const struct patch *synth_get_patch(int idx);

/**
 * @brief Return the total number of available patches.
 *
 * @return Number of entries in the internal patch table
 */
int synth_get_patch_count(void);

#endif /* SYNTH_ENGINE_H */

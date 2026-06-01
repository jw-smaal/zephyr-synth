/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file synth_engine.c
 * @brief Digital Signal Processing core for the synthesizer
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include "synth_engine.h"
#include "synth_luts_generated.h"

LOG_MODULE_DECLARE(midi_synth, CONFIG_LOG_DEFAULT_LEVEL);

#define MAX_VOICES CONFIG_VOICECARD_VOICES

#if MAX_VOICES <= 1
  #define MIXER_SHIFT 0
#elif MAX_VOICES <= 2
  #define MIXER_SHIFT 1
#elif MAX_VOICES <= 4
  #define MIXER_SHIFT 2
#elif MAX_VOICES <= 8
  #define MIXER_SHIFT 3
#elif MAX_VOICES <= 16
  #define MIXER_SHIFT 4
#else
  #define MIXER_SHIFT 5
#endif

#define PHASE_TO_SINE_SHIFT 17

/* Event Queue for Synthesizer Core */
K_MSGQ_DEFINE(synth_evt_queue, sizeof(struct synth_event), 32, 4);

/**
 * @brief Represents a single "Voice Card"
 */
struct voice_card {
	bool gate_open;             /**< Simple square envelope gate */
	uint8_t note;               /**< MIDI note number */
	q15_t velocity_scale;       /**< Amplitude scale factor (Q15) */
	uint32_t phase_inc;         /**< 32-bit phase increment */
	uint32_t phase_acc;         /**< 32-bit phase accumulator */
	uint32_t age;               /**< Incremented on every Note On to track oldest note */
};

/**
 * @brief Internal synthesizer state
 */
struct synth_state {
	struct voice_card voices[MAX_VOICES];
	uint32_t note_counter;
};

static struct synth_state m_synth_state = {
	.note_counter = 0,
};

void synth_engine_init(void)
{
	for (int i = 0; i < MAX_VOICES; i++) {
		m_synth_state.voices[i].gate_open = false;
		m_synth_state.voices[i].phase_acc = 0;
		m_synth_state.voices[i].age = 0;
	}
	LOG_INF("Synth Engine Initialized");
}

void synth_submit_event(struct synth_event *evt)
{
	/* Ignore failures if queue is full to prevent blocking MIDI UART */
	k_msgq_put(&synth_evt_queue, evt, K_NO_WAIT);
}

static void handle_note_on(uint8_t note, uint8_t velocity)
{
	int voice_to_use = -1;
	uint32_t oldest_age = 0xFFFFFFFF;

	for (int i = 0; i < MAX_VOICES; i++) {
		if (!m_synth_state.voices[i].gate_open) {
			voice_to_use = i;
			break;
		}
	}

	if (voice_to_use == -1) {
		for (int i = 0; i < MAX_VOICES; i++) {
			if (m_synth_state.voices[i].age < oldest_age) {
				oldest_age = m_synth_state.voices[i].age;
				voice_to_use = i;
			}
		}
	}

	if (voice_to_use != -1) {
		m_synth_state.voices[voice_to_use].note = note;
		m_synth_state.voices[voice_to_use].phase_inc = phase_inc_lut[note & 0x7F];
		m_synth_state.voices[voice_to_use].velocity_scale = velocity_lut[velocity & 0x7F];
		m_synth_state.voices[voice_to_use].gate_open = true;
		m_synth_state.voices[voice_to_use].age = ++m_synth_state.note_counter;
	}
}

static void handle_note_off(uint8_t note)
{
	for (int i = 0; i < MAX_VOICES; i++) {
		if (m_synth_state.voices[i].note == note && m_synth_state.voices[i].gate_open) {
			m_synth_state.voices[i].gate_open = false;
		}
	}
}

static void process_events(void)
{
	struct synth_event evt;
	while (k_msgq_get(&synth_evt_queue, &evt, K_NO_WAIT) == 0) {
		if (evt.type == SYNTH_EVT_NOTE_ON) {
			handle_note_on(evt.note, evt.velocity);
		} else if (evt.type == SYNTH_EVT_NOTE_OFF) {
			handle_note_off(evt.note);
		}
	}
}

void synth_engine_render_block(int16_t *buffer, uint32_t samples)
{
	/* Process any pending events before rendering the block */
	process_events();

	for (uint32_t i = 0; i < samples; i++) {
		int32_t accumulator = 0;

		for (int v = 0; v < MAX_VOICES; v++) {
			if (m_synth_state.voices[v].gate_open) {
				q15_t sine = arm_sin_q15((q15_t)(m_synth_state.voices[v].phase_acc >> PHASE_TO_SINE_SHIFT));
				accumulator += (int32_t)(((int32_t)sine * m_synth_state.voices[v].velocity_scale) >> 15);
				m_synth_state.voices[v].phase_acc += m_synth_state.voices[v].phase_inc;
			} else {
				m_synth_state.voices[v].phase_acc = 0;
			}
		}

		q15_t mixed_sample = (q15_t)CLAMP(accumulator >> MIXER_SHIFT, -32768, 32767);
		buffer[i * 2] = mixed_sample;
		buffer[i * 2 + 1] = mixed_sample;
	}
}

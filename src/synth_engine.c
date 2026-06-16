/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file synth_engine.c
 * @brief Digital Signal Processing core for the synthesizer
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @date 2026-06-16
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include "synth_engine.h"
#include "adsr.h"

/* This LUT is generated during the build process */
#include "synth_luts_generated.h"

LOG_MODULE_DECLARE(midi_synth, CONFIG_LOG_DEFAULT_LEVEL);

/* Look at the Kconfig */
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
#elif MAX_VOICES <= 32
#define MIXER_SHIFT 5
#else
#define MIXER_SHIFT 5
#endif

/** @brief Control rate period in samples (determines ADSR update frequency) */
#define CONTROL_RATE_PERIOD 16U

/* Event Queue for Synthesizer Core */
K_MSGQ_DEFINE(synth_evt_queue, sizeof(struct synth_event), 32, 4);

/**
 * @brief Represents a single "Voice Card"
 */
struct voice_card {
	bool gate_open;       /**< Envelope gate status */
	struct adsr envelope; /**< Voice's ADSR state */
	enum synth_wave wave; /**< Active waveform for this voice */
	uint8_t note;         /**< MIDI note number */
	q15_t velocity_scale; /**< Amplitude scale factor (Q15) */
	uint32_t phase_inc;   /**< 32-bit phase increment */
	uint32_t phase_acc;   /**< 32-bit phase accumulator */
	uint32_t age;         /**< Age tracker for voice stealing */
	q15_t pan_l;          /**< Stereo left panning weight (Q15) */
	q15_t pan_r;          /**< Stereo right panning weight (Q15) */
};

/**
 * @brief Internal synthesizer state
 */
struct synth_state {
	struct voice_card voices[MAX_VOICES];
	uint32_t note_counter;
	atomic_t active_patch_idx; /**< Global active patch index (atomic) */
};

#define MS_TO_SAMPLES(ms) (((uint64_t)(ms) * CONFIG_AUDIO_SAMPLE_RATE) / 1000)

/**
 * @brief Presets table
 */
static const struct patch g_patches[] = {
	{
		.name = "SAW",
		.layer_count = 1,
		.wave = { SYNTH_WAVE_SAW },
		.octave_shift = { 0 },
		.unison_count = 1,
		.detune_cents = 0,
		.env = {
			.attack = MS_TO_SAMPLES(20),
			.decay = MS_TO_SAMPLES(50),
			.sustain = 0xffffffff,
			.release = MS_TO_SAMPLES(800),
			.end = 0,
		}
	},
	{
		.name = "SINE",
		.layer_count = 1,
		.wave = { SYNTH_WAVE_SINE },
		.octave_shift = { 0 },
		.unison_count = 1,
		.detune_cents = 0,
		.env = {
			.attack = MS_TO_SAMPLES(20),
			.decay = MS_TO_SAMPLES(50),
			.sustain = 0xffffffff,
			.release = MS_TO_SAMPLES(800),
			.end = 0,
		}
	},
	{
		.name = "SQUARE",
		.layer_count = 1,
		.wave = { SYNTH_WAVE_SQUARE },
		.octave_shift = { 0 },
		.unison_count = 1,
		.detune_cents = 0,
		.env = {
			.attack = MS_TO_SAMPLES(20),
			.decay = MS_TO_SAMPLES(50),
			.sustain = 0xffffffff,
			.release = MS_TO_SAMPLES(800),
			.end = 0,
		}
	},
	{
		.name = "TRIANGLE",
		.layer_count = 1,
		.wave = { SYNTH_WAVE_TRI },
		.octave_shift = { 0 },
		.unison_count = 1,
		.detune_cents = 0,
		.env = {
			.attack = MS_TO_SAMPLES(20),
			.decay = MS_TO_SAMPLES(50),
			.sustain = 0xffffffff,
			.release = MS_TO_SAMPLES(800),
			.end = 0,
		}
	},
	{
		.name = "SUPER SAW",
		.layer_count = 1,
		.wave = { SYNTH_WAVE_SAW },
		.octave_shift = { 0 },
		.unison_count = 3,
		.detune_cents = 30,
		.env = {
			.attack = MS_TO_SAMPLES(10),
			.decay = MS_TO_SAMPLES(80),
			.sustain = 0xffffffff,
			.release = MS_TO_SAMPLES(600),
			.end = 0,
		}
	},
	{
		.name = "STRINGS",
		.layer_count = 1,
		.wave = { SYNTH_WAVE_SAW },
		.octave_shift = { 0 },
		.unison_count = 2,
		.detune_cents = 10,
		.env = {
			.attack = MS_TO_SAMPLES(800),
			.decay = MS_TO_SAMPLES(200),
			.sustain = 0xffffffff,
			.release = MS_TO_SAMPLES(1200),
			.end = 0,
		}
	},
	{
		.name = "SAW+SUB",
		.layer_count = 2,
		.wave = { SYNTH_WAVE_SAW, SYNTH_WAVE_SINE },
		.octave_shift = { 0, -1 },
		.unison_count = 1,
		.detune_cents = 0,
		.env = {
			.attack = MS_TO_SAMPLES(20),
			.decay = MS_TO_SAMPLES(50),
			.sustain = 0xffffffff,
			.release = MS_TO_SAMPLES(800),
			.end = 0,
		}
	}
};

static const int16_t *const wave_table[SYNTH_WAVE_COUNT] = {
	[SYNTH_WAVE_SINE]   = wave_lut_sine,
	[SYNTH_WAVE_SAW]    = wave_lut_saw,
	[SYNTH_WAVE_SQUARE] = wave_lut_square,
	[SYNTH_WAVE_TRI]    = wave_lut_tri,
};

static struct synth_state m_synth_state = {
	.note_counter = 0,
	.active_patch_idx = ATOMIC_INIT(0),
};

#define SUSTAIN_LEVEL 32000U

void synth_engine_init(void)
{
	for (int i = 0; i < MAX_VOICES; i++) {
		m_synth_state.voices[i].gate_open = false;
		m_synth_state.voices[i].wave = SYNTH_WAVE_SAW;
		m_synth_state.voices[i].phase_acc = 0;
		m_synth_state.voices[i].age = 0;
		m_synth_state.voices[i].envelope.state = END;
		m_synth_state.voices[i].envelope.sustain_level = SUSTAIN_LEVEL;
		m_synth_state.voices[i].envelope.lifetime = 0;
		m_synth_state.voices[i].envelope.initial_lifetime = 0;
		m_synth_state.voices[i].envelope.current_gain = 0;
		m_synth_state.voices[i].pan_l = 32767;
		m_synth_state.voices[i].pan_r = 32767;
	}
	atomic_set(&m_synth_state.active_patch_idx, 0);
	LOG_INF("Synth Engine Initialized");
}

void synth_submit_event(struct synth_event *evt)
{
	/* Ignore failures if queue is full to prevent blocking MIDI UART */
	k_msgq_put(&synth_evt_queue, evt, K_NO_WAIT);
}

static int count_active_voices(void)
{
	int active = 0;
	for (int i = 0; i < MAX_VOICES; i++) {
		if (m_synth_state.voices[i].envelope.state != END) {
			active++;
		}
	}
	return active;
}

static void handle_note_on(uint8_t note, uint8_t velocity, uint8_t patch_idx)
{
	if (patch_idx >= ARRAY_SIZE(g_patches)) {
		return;
	}

	const struct patch *p = &g_patches[patch_idx];
	uint8_t total_voices_needed = p->layer_count * p->unison_count;

	if (total_voices_needed == 0) {
		return;
	}

	for (uint8_t l = 0; l < p->layer_count; l++) {
		int shifted_note = (int)note + (int)p->octave_shift[l] * 12;
		if (shifted_note < 0) {
			shifted_note = 0;
		} else if (shifted_note > 127) {
			shifted_note = 127;
		}

		uint32_t base_phase_inc = phase_inc_lut[shifted_note];

		for (uint8_t u = 0; u < p->unison_count; u++) {
			int voice_to_use = -1;
			int stolen_voice = -1;
			bool needs_reset = false;

			/* Pass 1: find a fully silent (END) slot */
			for (int i = 0; i < MAX_VOICES; i++) {
				if (m_synth_state.voices[i].envelope.state == END) {
					voice_to_use = i;
					m_synth_state.voices[i].envelope.state = ATTACK;
					m_synth_state.voices[i].envelope.lifetime = p->env.attack;
					m_synth_state.voices[i].envelope.initial_lifetime = p->env.attack;
					m_synth_state.voices[i].envelope.current_gain = 0;
					break;
				}
			}

			/* Pass 2: find the quietest voice in RELEASE */
			if (voice_to_use == -1) {
				int32_t min_gain = 32768;

				for (int i = 0; i < MAX_VOICES; i++) {
					if (m_synth_state.voices[i].envelope.state == RELEASE &&
					    (int32_t)m_synth_state.voices[i].envelope.current_gain < min_gain) {
						min_gain = m_synth_state.voices[i].envelope.current_gain;
						stolen_voice = i;
					}
				}
				if (stolen_voice != -1) {
					voice_to_use = stolen_voice;
					needs_reset = true;
				}
			}

			/* Pass 3: last resort — steal the oldest held note */
			if (voice_to_use == -1) {
				uint32_t oldest_age = 0xFFFFFFFF;

				for (int i = 0; i < MAX_VOICES; i++) {
					if (m_synth_state.voices[i].age <= oldest_age) {
						oldest_age = m_synth_state.voices[i].age;
						stolen_voice = i;
					}
				}
				if (stolen_voice != -1) {
					voice_to_use = stolen_voice;
					needs_reset = true;
				}
			}

			/* Reset envelope state if voice was stolen */
			if (needs_reset && voice_to_use != -1) {
				m_synth_state.voices[voice_to_use].envelope.state = ATTACK;
				m_synth_state.voices[voice_to_use].envelope.lifetime = p->env.attack;
				m_synth_state.voices[voice_to_use].envelope.initial_lifetime = p->env.attack;
				m_synth_state.voices[voice_to_use].envelope.current_gain = 0;
			}

			if (voice_to_use != -1) {
				int32_t cents = 0;
				if (p->unison_count > 1) {
					cents = -p->detune_cents / 2 + (p->detune_cents * u) / (p->unison_count - 1);
				}

				uint32_t phase_inc_detuned = (uint32_t)(((uint64_t)base_phase_inc * (1731 + cents)) / 1731);

				q15_t pan_l, pan_r;
				if (p->unison_count > 1) {
					pan_r = (q15_t)((u * 32767) / (p->unison_count - 1));
					pan_l = 32767 - pan_r;
				} else {
					pan_l = 32767;
					pan_r = 32767;
				}

				m_synth_state.voices[voice_to_use].note = note;
				m_synth_state.voices[voice_to_use].wave = p->wave[l];
				m_synth_state.voices[voice_to_use].phase_inc = phase_inc_detuned;
				m_synth_state.voices[voice_to_use].phase_acc = 0;
				m_synth_state.voices[voice_to_use].velocity_scale = velocity_lut[velocity & 0x7F];
				m_synth_state.voices[voice_to_use].gate_open = true;
				m_synth_state.voices[voice_to_use].age = ++m_synth_state.note_counter;
				m_synth_state.voices[voice_to_use].pan_l = pan_l;
				m_synth_state.voices[voice_to_use].pan_r = pan_r;

				LOG_INF("Voice On:  Note %d -> Slot %d (Active: %d/%d)",
					note, voice_to_use, count_active_voices(), MAX_VOICES);
			}
		}
	}
}

static void handle_note_off(uint8_t note)
{
	int patch_idx = (int)atomic_get(&m_synth_state.active_patch_idx);
	const struct patch *active_patch = &g_patches[patch_idx];

	for (int i = 0; i < MAX_VOICES; i++) {
		struct voice_card *v_ptr = &m_synth_state.voices[i];
		if (v_ptr->note == note && v_ptr->envelope.state != END) {
			/* Sample current gain to start release phase smoothly */
			v_ptr->envelope.start_gain = v_ptr->envelope.current_gain;
			v_ptr->envelope.state = RELEASE;
			v_ptr->envelope.lifetime = active_patch->env.release;
			v_ptr->envelope.initial_lifetime = active_patch->env.release;
			LOG_DBG("Voice Off: Note %d -> Slot %d (Active: %d/%d)",
				note, i, count_active_voices(), MAX_VOICES);
		}
		if (v_ptr->envelope.state == END) {
			v_ptr->gate_open = false;
		}
	}
}

static void handle_set_patch(uint8_t patch_idx)
{
	if (patch_idx >= ARRAY_SIZE(g_patches)) {
		return;
	}

	atomic_set(&m_synth_state.active_patch_idx, (atomic_val_t)patch_idx);

	const struct patch *p = &g_patches[patch_idx];

	/* Propagate new first-layer waveform immediately to active sounding voices */
	for (int i = 0; i < MAX_VOICES; i++) {
		if (m_synth_state.voices[i].envelope.state != END) {
			m_synth_state.voices[i].wave = p->wave[0];
		}
	}
	LOG_INF("Patch -> %s", p->name);
}

static void process_events(void)
{
	struct synth_event evt;
	while (k_msgq_get(&synth_evt_queue, &evt, K_NO_WAIT) == 0) {
		if (evt.type == SYNTH_EVT_NOTE_ON) {
			handle_note_on(evt.note, evt.velocity, evt.patch_idx);
		} else if (evt.type == SYNTH_EVT_NOTE_OFF) {
			handle_note_off(evt.note);
		} else if (evt.type == SYNTH_EVT_SET_PATCH) {
			handle_set_patch(evt.patch_idx);
		}
	}
}

int synth_get_active_patch_idx(void)
{
	return (int)atomic_get(&m_synth_state.active_patch_idx);
}

const struct patch *synth_get_patch(int idx)
{
	if (idx < 0 || idx >= ARRAY_SIZE(g_patches)) {
		return NULL;
	}
	return &g_patches[idx];
}

int synth_get_patch_count(void)
{
	return (int)ARRAY_SIZE(g_patches);
}

void synth_engine_render_block(int16_t *buffer, uint32_t samples)
{
	/* Process any pending events before rendering the block */
	process_events();

	/* Get the active patch to use its envelope parameters */
	int patch_idx = (int)atomic_get(&m_synth_state.active_patch_idx);
	const struct patch *active_patch = &g_patches[patch_idx];

	uint32_t rendered = 0;
	while (rendered < samples) {
		uint32_t chunk = MIN(CONTROL_RATE_PERIOD, samples - rendered);

		/* Process the ADSR values for all voices at the control rate */
		for (int v = 0; v < MAX_VOICES; v++) {
			adsr_process(&m_synth_state.voices[v].envelope, active_patch->env, chunk, true);
		}

		/* Render audio samples for the current control period */
		for (uint32_t i = 0; i < chunk; i++) {
			int32_t accumulator_l = 0;
			int32_t accumulator_r = 0;
			uint32_t buf_idx = rendered + i;

			for (int v = 0; v < MAX_VOICES; v++) {
				struct voice_card *v_ptr = &m_synth_state.voices[v];

				if (v_ptr->envelope.state != END) {
					uint16_t lut_idx = (uint16_t)(v_ptr->phase_acc >> WAVE_LUT_SHIFT);
					q15_t raw_sample = wave_table[v_ptr->wave][lut_idx];

					v_ptr->phase_acc += v_ptr->phase_inc;

					int32_t sample_vol = (v_ptr->velocity_scale * v_ptr->envelope.current_gain) >> 15;
					int32_t mono_sample = ((int32_t)raw_sample * sample_vol) >> 15;

					accumulator_l += (mono_sample * v_ptr->pan_l) >> 15;
					accumulator_r += (mono_sample * v_ptr->pan_r) >> 15;
				} else {
					v_ptr->phase_acc = 0;
				}
			}

			/* Clamp the stereo channels and write to interleaved buffer */
			buffer[buf_idx * 2] = (q15_t)CLAMP(accumulator_l >> MIXER_SHIFT, -32768, 32767);
			buffer[buf_idx * 2 + 1] = (q15_t)CLAMP(accumulator_r >> MIXER_SHIFT, -32768, 32767);
		}

		rendered += chunk;
	}
}

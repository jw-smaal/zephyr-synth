/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file midi_handler.c
 * @brief MIDI 1.0 UART processing thread and callbacks
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include <zephyr/midi1/note.h>
#include "synth_state.h"

LOG_MODULE_DECLARE(midi_synth, CONFIG_LOG_DEFAULT_LEVEL);

extern struct synth_state g_synth_state;

/**
 * @brief Callback for MIDI Note On events
 */
static void midi_note_on_cb(uint8_t channel, uint8_t note, uint8_t velocity)
{
	int voice_to_use = -1;
	uint32_t oldest_age = 0xFFFFFFFF;

	/* Treat velocity 0 as Note Off */
	if (velocity == 0) {
		k_mutex_lock(&g_synth_state.lock, K_FOREVER);
		for (int i = 0; i < MAX_VOICES; i++) {
			if (g_synth_state.voices[i].note == note && g_synth_state.voices[i].gate_open) {
				g_synth_state.voices[i].gate_open = false;
			}
		}
		k_mutex_unlock(&g_synth_state.lock);
		return;
	}

	k_mutex_lock(&g_synth_state.lock, K_FOREVER);
	
	/* 1. Look for an idle voice */
	for (int i = 0; i < MAX_VOICES; i++) {
		if (!g_synth_state.voices[i].gate_open) {
			voice_to_use = i;
			break;
		}
	}

	/* 2. If no idle voice, steal the oldest one */
	if (voice_to_use == -1) {
		for (int i = 0; i < MAX_VOICES; i++) {
			if (g_synth_state.voices[i].age < oldest_age) {
				oldest_age = g_synth_state.voices[i].age;
				voice_to_use = i;
			}
		}
	}

	/* Trigger new note on the allocated voice */
	if (voice_to_use != -1) {
		g_synth_state.voices[voice_to_use].note = note;
		g_synth_state.voices[voice_to_use].phase_inc = phase_inc_lut[note & 0x7F];
		g_synth_state.voices[voice_to_use].velocity_scale = velocity_lut[velocity & 0x7F];
		g_synth_state.voices[voice_to_use].gate_open = true;
		g_synth_state.voices[voice_to_use].age = ++g_synth_state.note_counter;
	}

	k_mutex_unlock(&g_synth_state.lock);

	LOG_INF("Note On  | Ch: %2d | Note: %3d (%-4s) | Vel: %3d | Voice: %d",
		channel + 1, note, harm_note_to_text_with_octave(note, false), velocity, voice_to_use);
}

/**
 * @brief Callback for MIDI Note Off events
 */
static void midi_note_off_cb(uint8_t channel, uint8_t note, uint8_t velocity)
{
	ARG_UNUSED(velocity);

	k_mutex_lock(&g_synth_state.lock, K_FOREVER);
	/* Gate off all instances of this note (usually just one) */
	for (int i = 0; i < MAX_VOICES; i++) {
		if (g_synth_state.voices[i].note == note && g_synth_state.voices[i].gate_open) {
			g_synth_state.voices[i].gate_open = false;
		}
	}
	k_mutex_unlock(&g_synth_state.lock);

	LOG_INF("Note Off | Ch: %2d | Note: %3d (%-4s)",
		channel + 1, note, harm_note_to_text_with_octave(note, false));
}

/**
 * @brief MIDI Processing Thread
 */
void midi_thread_fn(void *p1, void *p2, void *p3)
{
	const struct device *midi_dev = DEVICE_DT_GET(DT_NODELABEL(midi0));
	struct midi1_serial_callbacks midi_cb = {
		.note_on = midi_note_on_cb,
		.note_off = midi_note_off_cb,
	};

	if (!device_is_ready(midi_dev)) {
		LOG_ERR("MIDI device not ready");
		return;
	}

	midi1_serial_register_callbacks(midi_dev, &midi_cb);

	LOG_INF("MIDI Thread Started");

	while (1) {
		/* Blocking call that parses incoming UART bytes */
		midi1_serial_receiveparser(midi_dev);
	}
}

K_THREAD_DEFINE(midi_tid, 1024, midi_thread_fn, NULL, NULL, NULL, 5, 0, K_TICKS_FOREVER);

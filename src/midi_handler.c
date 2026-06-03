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
#include "synth_engine.h"
#include "midi_handler.h"

LOG_MODULE_DECLARE(midi_synth, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @brief Callback for MIDI Note On events
 */
static void midi_note_on_cb(uint8_t channel, uint8_t note, uint8_t velocity)
{
	struct synth_event evt = {
		.type = SYNTH_EVT_NOTE_ON,
		.note = note,
		.velocity = velocity,
		.wave = SYNTH_WAVE_SAW,
	};

	synth_submit_event(&evt);

	LOG_INF("Note On  | Ch: %2d | Note: %3d (%-4s) | Vel: %3d", channel + 1, note,
		harm_note_to_text_with_octave(note, false), velocity);
}

/**
 * @brief Callback for MIDI Note Off events
 */
static void midi_note_off_cb(uint8_t channel, uint8_t note, uint8_t velocity)
{
	struct synth_event evt = {
		.type = SYNTH_EVT_NOTE_OFF,
		.note = note,
		.velocity = velocity,
	};

	synth_submit_event(&evt);

	LOG_INF("Note Off | Ch: %2d | Note: %3d (%-4s)", channel + 1, note,
		harm_note_to_text_with_octave(note, false));
}

static struct midi1_serial_callbacks midi_cb = {
	.note_on = midi_note_on_cb,
	.note_off = midi_note_off_cb,
};

/**
 * @brief MIDI Processing Thread
 */
static void midi_thread_fn(void *p1, void *p2, void *p3)
{
	const struct device *midi_dev = DEVICE_DT_GET(DT_NODELABEL(midi0));

	LOG_INF("MIDI Thread Started");

	while (1) {
		/* Blocking call that parses incoming UART bytes */
		midi1_serial_receiveparser(midi_dev);
	}
}

K_THREAD_DEFINE(midi_tid, 1024, midi_thread_fn, NULL, NULL, NULL, 5, 0, K_TICKS_FOREVER);

int midi_system_init(void)
{
	LOG_INF("MIDI system init");
	const struct device *midi_dev = DEVICE_DT_GET(DT_NODELABEL(midi0));

	if (!device_is_ready(midi_dev)) {
		LOG_ERR("MIDI device not ready");
		return -ENODEV;
	}

	midi1_serial_register_callbacks(midi_dev, &midi_cb);

	k_thread_start(midi_tid);

	return 0;
}

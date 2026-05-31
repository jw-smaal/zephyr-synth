/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief MIDI Sine Wave Synthesizer Main Entry Point
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/midi1/note.h>
#include <math.h>
#include "synth_state.h"

LOG_MODULE_REGISTER(midi_synth, CONFIG_LOG_DEFAULT_LEVEL);

extern k_tid_t const audio_tid;
extern k_tid_t const midi_tid;
extern k_tid_t const rx_sink_tid;

/* Memory slabs defined in audio.c */
extern struct k_mem_slab audio_slab;
K_MEM_SLAB_DEFINE_STATIC(rx_slab, 512, 4, 32); /**< Dummy for clock generator */

static const struct device *const i2s_dev = DEVICE_DT_GET(DT_ALIAS(i2s_tx));

/**
 * @brief Shared synthesizer state
 */
struct synth_state g_synth_state = {
	.note_counter = 0,
};

uint32_t phase_inc_lut[128];
q15_t velocity_lut[128];

/**
 * @brief Initialize the Lookup Tables
 * Pre-calculates phase increments and velocity scaling to eliminate float math at runtime.
 */
static void init_luts(void)
{
	/* Phase Increment Initialization */
	const float phase_factor = 4294967296.0f / CONFIG_AUDIO_SAMPLE_RATE;

	for (uint8_t note = 0; note < 128; note++) {
		float freq = harm_note_to_freq(note);
		
		/* 
		 * We cast through uint32_t first to prevent C from saturating 
		 * out-of-bounds float conversions for high frequencies.
		 */
		phase_inc_lut[note] = (uint32_t)(freq * phase_factor);
	}
	
	/* Velocity Initialization (Exponential mapping: Vel 127 = 0dB, Vel 1 = -30dB) */
	velocity_lut[0] = 0;
	for (uint8_t vel = 1; vel < 128; vel++) {
		/* Map velocity [1, 127] to dB [-30, 0] */
		float db = -30.0f + (((float)(vel - 1) / 126.0f) * 30.0f);
		/* Convert dB to linear amplitude */
		float linear = powf(10.0f, db / 20.0f);
		/* Convert to Q15 */
		velocity_lut[vel] = (q15_t)(linear * 32767.0f);
	}

	/* Initialize Voice Cards */
	for (int i = 0; i < MAX_VOICES; i++) {
		g_synth_state.voices[i].gate_open = false;
		g_synth_state.voices[i].phase_acc = 0;
		g_synth_state.voices[i].age = 0;
	}

	LOG_INF("LUTs initialized for %d Hz sample rate", 
		CONFIG_AUDIO_SAMPLE_RATE);
}

/**
 * @brief RX Sink Thread
 * Drains the dummy RX queue to keep clock generation running without ENOMEM.
 */
void rx_sink_thread_fn(void *p1, void *p2, void *p3)
{
	void *rx_buffer;
	size_t size;
	int ret;

	ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

	while (1) {
		ret = i2s_read(i2s_dev, &rx_buffer, &size);
		if (ret == 0) {
			k_mem_slab_free(&rx_slab, rx_buffer);
		} else {
			/* If no data yet, yield briefly */
			k_sleep(K_MSEC(1));
		}
	}
}

K_THREAD_DEFINE(rx_sink_tid, 1024, rx_sink_thread_fn, NULL, NULL, NULL, -1, 0, K_TICKS_FOREVER);

int main(void)
{
	struct i2s_config conf;
	int ret;

	/* Application Banner */
	LOG_INF("\n\n");
	LOG_INF("##############################################");
	LOG_INF("# MIDI Polyphonic Sine Synthesizer");
	LOG_INF("# By: Jan-Willem Smaal <usenet@gispen.org");
	LOG_INF("# Hardware: %-26s", CONFIG_BOARD_TARGET);
	LOG_INF("# Build:    %-26s", __DATE__ " " __TIME__);
	LOG_INF("##############################################");
	LOG_INF("Initializing system components...");

	if (!device_is_ready(i2s_dev)) {
		LOG_ERR("I2S device not ready");
		return -ENODEV;
	}

	/* Configure TX */
	conf.word_size = CONFIG_AUDIO_BIT_WIDTH;
	conf.channels = 2;
	conf.format = I2S_FMT_DATA_FORMAT_I2S;
	conf.options = I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER;
	conf.frame_clk_freq = CONFIG_AUDIO_SAMPLE_RATE;
	conf.block_size = 128 * 2 * (CONFIG_AUDIO_BIT_WIDTH / 8);
	conf.timeout = 2000;
	conf.mem_slab = &audio_slab;

	ret = i2s_configure(i2s_dev, I2S_DIR_TX, &conf);
	if (ret < 0) {
		LOG_ERR("I2S TX configure failed: %d", ret);
		return ret;
	}

	/* Configure RX for clock generation (NXP SAI requirement) */
	conf.mem_slab = &rx_slab;
	ret = i2s_configure(i2s_dev, I2S_DIR_RX, &conf);
	if (ret < 0) {
		LOG_ERR("I2S RX configure failed: %d", ret);
		return ret;
	}

	/* Setup threading primitives */
	k_mutex_init(&g_synth_state.lock);

	/* Pre-calculate phase increments and velocity scaling to eliminate float math at runtime */
	init_luts();

	/* Start Clocks (via RX) */
	ret = i2s_trigger(i2s_dev, I2S_DIR_RX, I2S_TRIGGER_START);
	if (ret < 0) {
		LOG_WRN("I2S RX (Clocks) start failed: %d", ret);
	}

	/* Hardware ready, launch threads */
	k_thread_start(midi_tid);
	k_thread_start(audio_tid);
	k_thread_start(rx_sink_tid);
	
	LOG_INF("System operational.");

	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}

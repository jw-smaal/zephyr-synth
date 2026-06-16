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
#include "audio.h"
#include "synth_engine.h"
#include "midi_handler.h"
#include "waveform_selector.h"

LOG_MODULE_REGISTER(midi_synth, CONFIG_LOG_DEFAULT_LEVEL);

K_MEM_SLAB_DEFINE_STATIC(rx_slab, 512, 4, 32); /**< Dummy for clock generator */

static const struct device *const i2s_dev = DEVICE_DT_GET(DT_ALIAS(i2s_tx));

/**
 * @brief RX Sink Thread
 * Drains the dummy RX queue to keep clock generation running without ENOMEM.
 */
static void rx_sink_thread_fn(void *p1, void *p2, void *p3)
{
	void *rx_buffer;
	size_t size;
	int ret;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

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
	LOG_INF("# By: Jan-Willem Smaal <usenet@gispen.org>");
	LOG_INF("# Hardware: %-26s", CONFIG_BOARD_TARGET);
	LOG_INF("# Build:    %-26s", __DATE__ " " __TIME__);
	LOG_INF("##############################################");
	LOG_INF("Initializing system components...");

	if (!device_is_ready(i2s_dev)) {
		LOG_ERR("I2S device not ready for RX clock generation");
		return -ENODEV;
	}

	/* Configure RX for clock generation (NXP SAI requirement) */
	conf.word_size = CONFIG_AUDIO_BIT_WIDTH;
	conf.channels = 2;
	conf.format = I2S_FMT_DATA_FORMAT_I2S;
	conf.options = I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER;
	conf.frame_clk_freq = CONFIG_AUDIO_SAMPLE_RATE;
	conf.block_size = 128 * 2 * (CONFIG_AUDIO_BIT_WIDTH / 8);
	conf.timeout = 2000;
	conf.mem_slab = &rx_slab;

	ret = i2s_configure(i2s_dev, I2S_DIR_RX, &conf);
	if (ret < 0) {
		LOG_ERR("I2S RX configure failed: %d", ret);
		return ret;
	}

	/* Initialize Synthesizer DSP */
	synth_engine_init();

	/* Initialize Audio System */
	ret = audio_system_init();
	if (ret < 0) {
		LOG_ERR("Audio system initialization failed: %d", ret);
		return ret;
	}

	/* Initialize MIDI System */
	ret = midi_system_init();
	if (ret < 0) {
		LOG_ERR("MIDI system initialization failed: %d", ret);
		return ret;
	}

	/* Initialize waveform selector (SW2) */
	ret = waveform_selector_init();
	if (ret < 0) {
		LOG_ERR("Waveform selector initialization failed: %d", ret);
		return ret;
	}

	/* Start Clocks (via RX) */
	ret = i2s_trigger(i2s_dev, I2S_DIR_RX, I2S_TRIGGER_START);
	if (ret < 0) {
		LOG_WRN("I2S RX (Clocks) start failed: %d", ret);
	}
	k_thread_start(rx_sink_tid);

	LOG_INF("System operational.");

	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}

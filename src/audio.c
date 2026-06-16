/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file audio.c
 * @brief Real-time audio generation thread
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/logging/log.h>
#include "audio.h"
#include "synth_engine.h"

LOG_MODULE_DECLARE(midi_synth, CONFIG_LOG_DEFAULT_LEVEL);

/* --- Audio Constants --- */
#define SAMPLES_PER_BLOCK 128
#define CHANNELS          2
#define FRAME_SIZE        (SAMPLES_PER_BLOCK * CHANNELS * sizeof(int16_t))

/* Memory slab for I2S TX blocks */
K_MEM_SLAB_DEFINE_STATIC(audio_slab, FRAME_SIZE, 16, 32);

static const struct device *const i2s_dev = DEVICE_DT_GET(DT_ALIAS(i2s_tx));

static int configure_tx(void)
{
	struct i2s_config conf;

	conf.word_size = CONFIG_AUDIO_BIT_WIDTH;
	conf.channels = 2;
	conf.format = I2S_FMT_DATA_FORMAT_I2S;
	conf.options = I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER;
	conf.frame_clk_freq = CONFIG_AUDIO_SAMPLE_RATE;
	conf.block_size = SAMPLES_PER_BLOCK * CHANNELS * sizeof(int16_t);
	conf.timeout = 50;
	conf.mem_slab = &audio_slab;

	return i2s_configure(i2s_dev, I2S_DIR_TX, &conf);
}

/**
 * @brief Audio Generation Thread
 */
static void audio_thread_fn(void *p1, void *p2, void *p3)
{
	int ret;
	void *tx_buffer;
	bool started = false;

	LOG_INF("Audio Thread Started");

	while (1) {
		ret = k_mem_slab_alloc(&audio_slab, &tx_buffer, K_FOREVER);
		if (ret < 0) {
			continue;
		}

		/* Fill buffer from DSP engine */
		synth_engine_render_block((int16_t *)tx_buffer, SAMPLES_PER_BLOCK);

		ret = i2s_write(i2s_dev, tx_buffer, FRAME_SIZE);
		if (ret < 0) {
			k_mem_slab_free(&audio_slab, tx_buffer);
			/*
			 * Any error (including -EAGAIN from a timeout or -EIO from
			 * an underrun) means the I2S TX pipeline has stopped.
			 * Issue DROP to flush it back to a known idle state,
			 * reset the started flag so the next successful write
			 * re-triggers the peripheral, and continue.
			 * The thread must never exit permanently.
			 */
			LOG_WRN("I2S write error %d — dropping TX pipeline, restarting",
				ret);
			i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_DROP);
			ret = configure_tx();
			if (ret < 0) {
				LOG_ERR("I2S TX reconfigure failed: %d", ret);
			}
			started = false;
			continue;
		}

		if (!started) {
			ret = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
			if (ret < 0) {
				LOG_ERR("I2S TX start failed: %d", ret);
				return;
			}
			started = true;
		}
	}
}

K_THREAD_DEFINE(audio_tid, 2048, audio_thread_fn, NULL, NULL, NULL, 1, 0, K_TICKS_FOREVER);

int audio_system_init(void)
{
	int ret;

	if (!device_is_ready(i2s_dev)) {
		LOG_ERR("I2S device not ready");
		return -ENODEV;
	}

	ret = configure_tx();
	if (ret < 0) {
		LOG_ERR("I2S TX configure failed: %d", ret);
		return ret;
	}

	k_thread_start(audio_tid);

	return 0;
}

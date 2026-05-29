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
#include <arm_math.h>
#include "synth_state.h"

LOG_MODULE_DECLARE(midi_synth, CONFIG_LOG_DEFAULT_LEVEL);

/* --- Audio Constants --- */
#define SAMPLES_PER_BLOCK 128
#define CHANNELS 2
#define FRAME_SIZE (SAMPLES_PER_BLOCK * CHANNELS * sizeof(int16_t))
#define PHASE_TO_SINE_SHIFT 17

/* Memory slab for I2S TX blocks */
K_MEM_SLAB_DEFINE(audio_slab, FRAME_SIZE, 16, 32);

extern struct synth_state g_synth_state;
static const struct device *const i2s_dev = DEVICE_DT_GET(DT_ALIAS(i2s_tx));

/**
 * @brief Fill a buffer by summing all active voices
 */
static void fill_audio_block(struct synth_state *state, int16_t *buffer)
{
	struct voice_card local_voices[MAX_VOICES];

	/* Snapshot all voice states at once to minimize mutex contention */
	k_mutex_lock(&state->lock, K_FOREVER);
	memcpy(local_voices, state->voices, sizeof(local_voices));
	k_mutex_unlock(&state->lock);

	for (uint32_t i = 0; i < SAMPLES_PER_BLOCK; i++) {
		int32_t accumulator = 0;

		for (int v = 0; v < MAX_VOICES; v++) {
			if (local_voices[v].gate_open) {
				/* Generate sine for this voice */
				q15_t sine = arm_sin_q15((q15_t)(local_voices[v].phase_acc >> PHASE_TO_SINE_SHIFT));
				
				/* Apply velocity scaling and add to 32-bit accumulator */
				accumulator += (int32_t)(((int32_t)sine * local_voices[v].velocity_scale) >> 15);
				
				/* Advance phase for this voice's local snapshot */
				local_voices[v].phase_acc += local_voices[v].phase_inc;
			} else {
				local_voices[v].phase_acc = 0;
			}
		}

		/* 
		 * Scale down the sum to prevent clipping. 
		 * With 4 voices, we shift right by 2 (divide by 4).
		 */
		q15_t mixed_sample = (q15_t)(accumulator >> 2);

		/* Interleave Stereo */
		buffer[i * 2] = mixed_sample;
		buffer[i * 2 + 1] = mixed_sample;
	}

	/* Write the updated phase accumulators back to the shared state */
	k_mutex_lock(&state->lock, K_FOREVER);
	for (int v = 0; v < MAX_VOICES; v++) {
		state->voices[v].phase_acc = local_voices[v].phase_acc;
	}
	k_mutex_unlock(&state->lock);
}

/**
 * @brief Audio Generation Thread
 */
void audio_thread_fn(void *p1, void *p2, void *p3)
{
	int ret;
	void *tx_buffer;
	bool started = false;

	if (!device_is_ready(i2s_dev)) {
		LOG_ERR("I2S device not ready");
		return;
	}

	LOG_INF("Audio Thread Started");

	while (1) {
		/* Allocate memory block for current frame */
		ret = k_mem_slab_alloc(&audio_slab, &tx_buffer, K_FOREVER);
		if (ret < 0) {
			continue;
		}

		/* Generate audio data */
		fill_audio_block(&g_synth_state, (int16_t *)tx_buffer);

		/* Write to I2S driver */
		ret = i2s_write(i2s_dev, tx_buffer, FRAME_SIZE);
		if (ret < 0) {
			k_mem_slab_free(&audio_slab, tx_buffer);
			if (ret != -EAGAIN) {
				LOG_ERR("I2S write failed: %d", ret);
				break;
			}
			/* On EAGAIN, we just dropped the frame. Proceed. */
			continue;
		}

		/* Handle NXP SAI Startup Sequence: Queue 1 block then trigger START */
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

K_THREAD_DEFINE(audio_tid, 2048, audio_thread_fn, NULL, NULL, NULL, -2, 0, K_TICKS_FOREVER);

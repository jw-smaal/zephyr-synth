/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file adsr.c
 * @brief ADSR implementation
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */
#include "adsr.h"

void adsr_trigger_on(struct adsr *padsr, struct adsr_param param)
{
	padsr->state = ATTACK;
	padsr->lifetime = param.attack;
	padsr->initial_lifetime = param.attack;
	padsr->start_gain = 0;
	padsr->gain_accum = 0;

	if (param.attack > 0) {
		padsr->gain_step = ((int32_t)32767 << 16) / (int32_t)param.attack;
	} else {
		padsr->gain_step = 0;
		padsr->gain_accum = (int32_t)32767 << 16;
	}
	padsr->current_gain = (q15_t)(padsr->gain_accum >> 16);
}

void adsr_trigger_off(struct adsr *padsr, struct adsr_param param)
{
	padsr->state = RELEASE;
	padsr->lifetime = param.release;
	padsr->initial_lifetime = param.release;
	padsr->start_gain = padsr->current_gain;
	padsr->gain_accum = (int32_t)padsr->start_gain << 16;

	if (param.release > 0) {
		padsr->gain_step = -((int32_t)padsr->start_gain << 16) / (int32_t)param.release;
	} else {
		padsr->gain_step = 0;
		padsr->gain_accum = 0;
	}
	padsr->current_gain = (q15_t)(padsr->gain_accum >> 16);
}

enum adsr_state adsr_process(struct adsr *padsr, struct adsr_param param, uint32_t delta_samples,
			     bool use_log)
{
	enum adsr_state ret_state = padsr->state;
	uint32_t active_delta = delta_samples;

	/* Don't do anything when we are in the END state */
	if (ret_state == END) {
		padsr->lifetime = 0;
		return ret_state;
	}

	/* State transition when lifetime expires */
	while (padsr->lifetime == 0 && padsr->state != END) {
		padsr->start_gain = padsr->current_gain;
		switch (padsr->state) {
		case ATTACK:
			padsr->state = DECAY;
			padsr->lifetime = param.decay;
			padsr->initial_lifetime = param.decay;
			padsr->gain_accum = (int32_t)padsr->start_gain << 16;
			if (param.decay > 0) {
				int32_t target_gain = padsr->sustain_level;
				padsr->gain_step = ((target_gain - padsr->start_gain) << 16) / (int32_t)param.decay;
			} else {
				padsr->gain_step = 0;
				padsr->gain_accum = (int32_t)padsr->sustain_level << 16;
			}
			padsr->current_gain = (q15_t)(padsr->gain_accum >> 16);
			break;
		case DECAY:
			padsr->state = SUSTAIN;
			padsr->lifetime = param.sustain;
			padsr->initial_lifetime = param.sustain;
			padsr->gain_accum = (int32_t)padsr->sustain_level << 16;
			padsr->gain_step = 0;
			padsr->current_gain = padsr->sustain_level;
			break;
		case SUSTAIN:
			if (param.sustain > 0) {
				padsr->state = RELEASE;
				padsr->lifetime = param.release;
				padsr->initial_lifetime = param.release;
				padsr->gain_accum = (int32_t)padsr->current_gain << 16;
				if (param.release > 0) {
					padsr->gain_step = -((int32_t)padsr->current_gain << 16) / (int32_t)param.release;
				} else {
					padsr->gain_step = 0;
					padsr->gain_accum = 0;
				}
				padsr->current_gain = (q15_t)(padsr->gain_accum >> 16);
			} else {
				goto transition_done;
			}
			break;
		case RELEASE:
			padsr->state = END;
			padsr->lifetime = 0;
			padsr->initial_lifetime = 0;
			padsr->gain_accum = 0;
			padsr->gain_step = 0;
			padsr->current_gain = 0;
			break;
		default:
			goto transition_done;
		}
	}
transition_done:

	if (padsr->state == END) {
		padsr->lifetime = 0;
		return ret_state;
	}

	/* Subtract the delta from the lifetime */
	if (active_delta >= padsr->lifetime) {
		active_delta = padsr->lifetime;
		padsr->lifetime = 0;
	} else {
		padsr->lifetime = padsr->lifetime - active_delta;
	}

	/* Perform incremental step accumulation */
	switch (padsr->state) {
	case ATTACK:
	case DECAY:
	case RELEASE:
		padsr->gain_accum += padsr->gain_step * active_delta;
		/* Boundary clamping */
		if (padsr->state == ATTACK) {
			if (padsr->gain_accum > ((int32_t)32767 << 16)) {
				padsr->gain_accum = (int32_t)32767 << 16;
			}
		} else if (padsr->state == DECAY) {
			if (padsr->gain_step < 0) {
				if (padsr->gain_accum < ((int32_t)padsr->sustain_level << 16)) {
					padsr->gain_accum = (int32_t)padsr->sustain_level << 16;
				}
			} else if (padsr->gain_step > 0) {
				if (padsr->gain_accum > ((int32_t)padsr->sustain_level << 16)) {
					padsr->gain_accum = (int32_t)padsr->sustain_level << 16;
				}
			}
		} else if (padsr->state == RELEASE) {
			if (padsr->gain_accum < 0) {
				padsr->gain_accum = 0;
			}
		}
		padsr->current_gain = (q15_t)(padsr->gain_accum >> 16);
		break;
	case SUSTAIN:
		padsr->current_gain = padsr->sustain_level;
		break;
	case END:
		padsr->current_gain = 0;
		break;
	}

	if (use_log) {
		padsr->current_gain =
			(q15_t)(((int32_t)padsr->current_gain * padsr->current_gain) >> 15);
	}

	return ret_state;
}

/* EOF */

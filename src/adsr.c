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

enum adsr_state adsr_process(struct adsr *padsr, struct adsr_param param, uint32_t delta_samples,
			     bool use_log)
{
	enum adsr_state ret_state = padsr->state;

	/* Don't do anything when we are in the END state*/
	if (ret_state == END) {
		padsr->lifetime = 0;
		return ret_state;
	} else {
		/* Let's substract the delta from the lifetime */
		if (delta_samples >= padsr->lifetime) {
			padsr->lifetime = 0;
		} else {
			padsr->lifetime = padsr->lifetime - delta_samples;
		}
	}

	/* State transition when lifetime expires */
	if (padsr->lifetime == 0) {
		padsr->start_gain = padsr->current_gain;
		switch (padsr->state) {
		case ATTACK:
			padsr->state = DECAY;
			padsr->lifetime = param.decay;
			padsr->initial_lifetime = param.decay;
			break;
		case DECAY:
			padsr->state = SUSTAIN;
			padsr->lifetime = param.sustain;
			padsr->initial_lifetime = param.sustain;
			break;
		case SUSTAIN:
			/*
			 * So this stays valid as long
			 * as the note is held untill the
			 * "maximum" sustain lenght and
			 * then it moves to release.
			 */
			if (param.sustain > 0) {
				padsr->state = RELEASE;
				padsr->lifetime = param.release;
				padsr->initial_lifetime = param.release;
			}
			break;
		case RELEASE:
			padsr->state = END;
			padsr->lifetime = 0;
			padsr->initial_lifetime = 0;
			break;
		case END:
			padsr->lifetime = 0;
			padsr->initial_lifetime = 0;
			break;
		};
	}

	/*
	 * Interpolate amplitude value of the current state.  We actually need two types
	 * of ADSR enveloppes linear and logarithmic (for amplitude).
	 */
	switch (padsr->state) {
	case ATTACK:
		/* Start at zero ramp up to FULL_LEVEL */
		padsr->current_gain =
			(q15_t)(((padsr->initial_lifetime - padsr->lifetime) * 32767U) /
				padsr->initial_lifetime);
		break;
	case DECAY: {
		/* Start at start_gain go down to SUSTAIN level */
		if (padsr->start_gain > padsr->sustain_level) {
			uint32_t distance = padsr->start_gain - padsr->sustain_level;
			padsr->current_gain = padsr->sustain_level +
					      (q15_t)(((uint64_t)distance * padsr->lifetime) /
						      padsr->initial_lifetime);
		} else {
			padsr->current_gain = padsr->sustain_level;
		}
		break;
	}
	case SUSTAIN:
		/* start at SUSTAIN level */
		padsr->current_gain = padsr->sustain_level;
		break;
	case RELEASE:
		/* Start at start_gain and end at zero */
		padsr->current_gain = (q15_t)(((uint64_t)padsr->lifetime * padsr->start_gain) /
					      padsr->initial_lifetime);
		break;
	case END:
		padsr->current_gain = 0;
		break;
	};

	if (use_log) {
		/* TODO: implement log shift */
		padsr->current_gain =
			(q15_t)(((int32_t)padsr->current_gain * padsr->current_gain) >> 15);
	}

	return ret_state;
}

/* EOF */

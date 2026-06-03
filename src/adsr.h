/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file adsr.h
 * @brief ADSR implementation
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */
#ifndef ADSR_H
#define ADSR_H
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/dsp/types.h>

/*
 * ADSR background: 
 * So for every voice that is sounding each should have it's
 * own "counter" on how long we are in the certain state.
 * When the lifetime of the state expires we need to move to the next
 * state. The state of the 'adsr' dicates the amount of attenuation of
 * the periodic wave. The length is a fixed value so it's independent of note frequency.  
 * For volume based ADSR we need logarithmic calculations for things
 * like pitch and filter cutoff etc.. we need linear.   
 */

struct adsr_param {
	uint32_t attack;
	uint32_t decay;
	uint32_t sustain;
	uint32_t release;
	uint32_t end;
};

enum adsr_state {
	ATTACK,
	DECAY,
	SUSTAIN,
	RELEASE,
	END,
};

struct adsr {
	enum adsr_state state;
	q15_t sustain_level;
	uint32_t lifetime; /* counter for the remaining lifetime */
	uint32_t initial_lifetime;
	q15_t current_gain; /* This is calculated inside adsr_process */
};

enum adsr_state adsr_process(struct adsr *padsr, 
			     struct adsr_param adsr_param, 
			     uint32_t delta_samples,
			     bool use_log);


#endif
/* EOF */
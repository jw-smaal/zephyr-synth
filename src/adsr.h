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

/*
 * So for every voice that is sounding each should have it's
 * own "counter" on how long we are in the certain state.
 * When the lifetime of the state expires we need to move to the next
 * state.
 * the state of the 'adsr' dicates the amount of attenuation of
 * the period wave.
 * The length is a fixed value so it's independent of note frequency
 * etc...
 */


struct adsr_parameters {
	uint8_t attack;
	uint8_t decay;
	uint8_t sustain;
	uint8_t release;
	uint8_t end;
};


enum adsr_state {
	ATTACK,
	DECAY,
	SUSTAIN,
	RELEASE,
	END,
};

#define FULL_LEVEL    UINT8_MAX
#define SUSTAIN_LEVEL 200
struct adsr {
	enum adsr_state state;
	uint8_t sustain_level;
	uint32_t lifetime; /* counter for the remaining lifetime */
	uint32_t initial_lifetime;
	q15_t current_gain; /* This is calculated inside adsr_process */
};

enum adsr_state adsr_process(struct adsr * padsr, uint32_t delta);




#endif

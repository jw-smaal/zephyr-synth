/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file adsr.c
 * @brief ADSR implementation
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */
#include <stdint.h>
#include "adsr.h"

enum adsr_state adsr_process(struct adsr* padsr, uint32_t delta)
{
	enum adsr_state ret_state = padsr->state;
	
	if(ret_state == END) {
		padsr->lifetime = 0;
		return  ret_state;
	}
	
	/* State transition when lifetime expires */
	if (padsr->lifetime == 0) {
		switch(padsr->state) {
			case ATTACK:
				padsr->state = DECAY;
				break;
			case DECAY:
				padsr->state = SUSTAIN;
				break;
			case SUSTAIN:
				/*
				 * So this stays valid as long
				 * as the note is held untill the
				 * "maximum" sustain lenght and
				 * then it moves to release.
				 */
				padsr->state = RELEASE;
				break;
			case RELEASE:
				padsr->state = END;
				break;
			case END:
				break;
		};
		
	}
	
	/* Interpolate amplitude value of the current state */
	switch(padsr->state) {
		case ATTACK:
			/* Start at zero ramp up to FULL_LEVEL */
			break;
		case DECAY:
			/* Start at FULL_LEVEL go down to SUSTAIN level */
			break;
		case SUSTAIN:
			/* start at SUSTAIN level */
			break;
		case RELEASE:
			/* Start at SUSTAIN level and end at zero */
			break;
		case END:
			break;
	};
	
	return ret_state;
}


/* EOF */

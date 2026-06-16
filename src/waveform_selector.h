/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file waveform_selector.h
 * @brief SW2 push-button waveform selector API
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @date 2026-06-16
 */

#ifndef WAVEFORM_SELECTOR_H
#define WAVEFORM_SELECTOR_H

/**
 * @brief Initialise the waveform selector.
 *
 * Configures SW2 as an interrupt-driven input.  Each press cycles the
 * global synthesiser waveform through the values defined in
 * @ref synth_wave, wrapping back to the first after the last.
 *
 * @return 0 on success, negative error code on failure.
 */
int waveform_selector_init(void);

#endif /* WAVEFORM_SELECTOR_H */

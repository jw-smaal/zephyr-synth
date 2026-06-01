/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file midi_handler.h
 * @brief MIDI input system API
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#ifndef MIDI_HANDLER_H
#define MIDI_HANDLER_H

/**
 * @brief Initialize the MIDI input system
 * 
 * @return 0 on success, negative error code on failure
 */
int midi_system_init(void);

#endif /* MIDI_HANDLER_H */
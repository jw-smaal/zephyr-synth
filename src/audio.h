/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file audio.h
 * @brief Audio system API
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#ifndef AUDIO_H
#define AUDIO_H

/**
 * @brief Initialize the audio system
 * 
 * @return 0 on success, negative error code on failure
 */
int audio_system_init(void);

#endif /* AUDIO_H */
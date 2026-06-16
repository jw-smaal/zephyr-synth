/*
 * Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file waveform_selector.c
 * @brief SW2 push-button waveform selector
 *
 * Each press of SW2 posts a SYNTH_EVT_SET_WAVE event to the synthesiser
 * message queue, cycling through the synth_wave enum in order.  Adding a
 * new waveform only requires extending the enum; no change is needed here.
 *
 * The GPIO callback runs in ISR context and only posts to the message queue,
 * keeping the ISR path short and lock-free.
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @date 2026-06-16
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "synth_engine.h"
#include "waveform_selector.h"

LOG_MODULE_DECLARE(midi_synth, CONFIG_LOG_DEFAULT_LEVEL);

/* SW2 is defined in the board DTSI; enabled via the overlay. */
#define SW2_NODE DT_ALIAS(sw2)

BUILD_ASSERT(DT_NODE_EXISTS(SW2_NODE),
	     "sw2 alias not found in devicetree — check the board overlay");

static const struct gpio_dt_spec sw2 = GPIO_DT_SPEC_GET(SW2_NODE, gpios);

static struct gpio_callback sw2_cb_data;

/**
 * @brief ISR callback for SW2 press.
 *
 * Reads the current active waveform, advances to the next value in
 * the @ref synth_wave enum (wrapping at SYNTH_WAVE_COUNT), then posts
 * a SYNTH_EVT_SET_WAVE event to the synthesiser queue.
 */
static void sw2_pressed(const struct device *dev,
			struct gpio_callback *cb,
			uint32_t pins)
{
	struct synth_event evt;
	int next;

	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	next = (synth_get_active_patch_idx() + 1) % synth_get_patch_count();

	evt.type = SYNTH_EVT_SET_PATCH;
	evt.patch_idx = (uint8_t)next;
	evt.note = 0;
	evt.velocity = 0;

	synth_submit_event(&evt);
}

int waveform_selector_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&sw2)) {
		LOG_ERR("SW2 GPIO device not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&sw2, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("SW2 gpio_pin_configure_dt failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&sw2, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret < 0) {
		LOG_ERR("SW2 interrupt configure failed: %d", ret);
		return ret;
	}

	gpio_init_callback(&sw2_cb_data, sw2_pressed, BIT(sw2.pin));
	gpio_add_callback(sw2.port, &sw2_cb_data);

	LOG_INF("Waveform selector ready (SW2 = gpio%d pin %d)",
		sw2.port->name[strlen(sw2.port->name) - 1], sw2.pin);

	return 0;
}

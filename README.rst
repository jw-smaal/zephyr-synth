MIDI Polyphonic Synthesizer for NXP MCXN947
###########################################

A polyphonic synthesizer controlled via MIDI 1.0 over UART, with hardware
waveform selection via the on-board SW2 push-button. The project demonstrates
a multi-threaded architecture separating MIDI message parsing, hardware input
handling, and real-time audio generation on the FRDM-MCXN947.

Features
********

*   **Polyphony**: Supports up to 16 simultaneous voices (configurable via ``CONFIG_VOICECARD_VOICES``).
*   **Multiple Waveforms**: Sine and Sawtooth oscillators. SW2 cycles through available waveforms at runtime.
*   **ADSR Envelope**: Per-voice Attack-Decay-Sustain-Release envelope with smooth transitions and zero-duration guards.
*   **DDS Synthesis**: Direct Digital Synthesis using CMSIS-DSP FastMath.
*   **Fixed-Point Math**: All runtime audio calculations use Q15 fixed-point arithmetic.
*   **Real-Time Safety**: Non-blocking I2S queue management with frame dropping on congestion.
*   **Prioritised Voice Stealing**: Three-pass algorithm — free slots first, then quietest releasing voice, then oldest held note.

Prerequisites
*************

This project depends on the out-of-tree **zephyr-midi1** module for MIDI
protocol parsing and harmonic utilities. Ensure it is correctly registered
in your west workspace.

Hardware Setup
**************

*   **Audio Output**: PCM5102A DAC connected to SAI1.
*   **MIDI Input**: 31.25 kBaud UART connected to ``flexcomm2_lpuart2`` (on MikroBUS header).
*   **Waveform Select**: SW2 push-button (on-board, GPIO0 pin 23).

Pin Mapping (I2S / SAI1):
==========================

+-----------------------+---------------+-----------------------+
| FRDM-MCXN947 (SAI1)   | Signal        | PCM5102A DAC          |
+=======================+===============+=======================+
| J1 Pin 1 (PIO3_16)    | BCLK          | BCK                   |
+-----------------------+---------------+-----------------------+
| J1 Pin 11 (PIO3_17)   | LRCK          | LCK                   |
+-----------------------+---------------+-----------------------+
| J1 Pin 5 (PIO3_20)    | DATA          | DIN                   |
+-----------------------+---------------+-----------------------+

Controls
========

+--------+------------------+---------------------------------------------+
| Button | Location         | Function                                    |
+========+==================+=============================================+
| SW2    | On-board         | Cycle waveform: SINE → SAW → (future) → ... |
+--------+------------------+---------------------------------------------+

Building and Running
********************

To build for the FRDM-MCXN947:

.. code-block:: bash

    west build -b frdm_mcxn947/mcxn947/cpu0 -p always

To flash:

.. code-block:: bash

    west flash

Software Architecture
*********************

The system consists of four execution contexts:

1.  **MIDI Thread (Priority 5)**: Utilises the ``midi1_serial`` driver to parse
    incoming UART bytes. On Note On/Off events, constructs a ``synth_event`` and
    posts it to the synthesiser message queue. The waveform field of each Note On
    event is populated from ``synth_get_active_wave()``.

2.  **Audio Thread (Priority -2)**: Allocates 128-sample frames from an internal
    memory slab, invokes ``synth_engine_render_block()`` to fill the buffer with
    Q15 audio samples, then transmits the frame via I2S DMA.

3.  **RX Sink Thread (Priority -1)**: Drains the dummy I2S RX queue to keep the
    NXP SAI clock tree running (bit clock swap hardware requirement).

4.  **SW2 GPIO ISR (interrupt context)**: Fires on the falling edge of SW2.
    Posts a ``SYNTH_EVT_SET_WAVE`` event to the synthesiser queue and returns
    immediately. The waveform change is applied by the audio thread on its next
    block boundary.

Phase increments are retrieved from a 128-entry lookup table (``synth_luts_generated.h``)
generated at build time by ``scripts/gen_luts.py`` using the configured sample rate.

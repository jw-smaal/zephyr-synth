MIDI Polyphonic Synthesizer for NXP MCXN947
###########################################

A polyphonic synthesizer controlled via MIDI 1.0 over UART. This project demonstrates a multi-threaded architecture separating MIDI message parsing from real-time audio generation on the FRDM-MCXN947.

Features
********

*   **Polyphony**: Supports up to 16 simultaneous voices with age-based voice stealing.
*   **Multiple Waveforms**: Sine and Sawtooth oscillators with per-voice selection.
*   **ADSR Envelope**: Per-voice Attack-Decay-Sustain-Release envelope with smooth transitions.
*   **DDS Synthesis**: Direct Digital Synthesis using CMSIS-DSP FastMath.
*   **Fixed-Point Math**: All runtime audio calculations utilize Q15 fixed-point arithmetic.
*   **Real-Time Safety**: Non-blocking I2S queue management with frame dropping on congestion.

Prerequisites
*************

This project depends on the out-of-tree **zephyr-midi1** module for MIDI protocol parsing and harmonic utilities. Ensure it is correctly registered in your west workspace.

Hardware Setup
**************

*   **Audio Output**: PCM5102A DAC connected to SAI1.
*   **MIDI Input**: 3.125 kBaud UART connected to ``flexcomm2_lpuart2`` (on MIKRObus header).

Pin Mapping (I2S):
==================

+-----------------------+---------------+-----------------------+
| FRDM-MCXN947 (SAI1)   | Signal        | PCM5102A DAC          |
+=======================+===============+=======================+
| J1 Pin 1 (PIO3_16)    | BCLK          | BCK                   |
+-----------------------+---------------+-----------------------+
| J1 Pin 11 (PIO3_17)   | LRCK          | LCK                   |
+-----------------------+---------------+-----------------------+
| J1 Pin 5 (PIO3_20)    | DATA          | DIN                   |
+-----------------------+---------------+-----------------------+

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

The system consists of two primary threads:

1.  **MIDI Thread (Priority 5)**: Utilises the ``midi1_serial`` driver to parse incoming UART bytes. Updates the shared synthesizer state upon receiving Note On or Note Off events.
2.  **Audio Thread (Priority -2)**: Generates 128-sample frames of sine data. It queries the shared state for the active note and gate status, then transmits the frame via DMA.

Phase increments are retrieved from a 128-entry lookup table initialized during the boot sequence using frequencies from the ``zephyr-midi1`` library.

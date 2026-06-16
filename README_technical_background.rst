MIDI Polyphonic Synthesizer Technical Background
################################################

This document provides a technical overview of the software architecture and
internal design of the MIDI polyphonic synthesizer.

Thread Architecture
*******************

The application is structured around three active execution threads and one
interrupt service routine that coordinate through message queues and drivers.

1. RX Sink Thread
=================

* Source File: `src/main.c <file:///Users/smaal/zephyr-workspace/midi-synth/src/main.c>`_
* Entry Point: `rx_sink_thread_fn <file:///Users/smaal/zephyr-workspace/midi-synth/src/main.c#L29-L48>`_
* Priority: -1 (cooperative)
* Description: Initialises the I2S RX channel and drains incoming dummy audio
  samples. This maintains clock generation (bit clock and frame sync) required
  by the transmitter, satisfying the NXP SAI bit clock swap driver requirement.

2. Audio Generation Thread
==========================

* Source File: `src/audio.c <file:///Users/smaal/zephyr-workspace/midi-synth/src/audio.c>`_
* Entry Point: `audio_thread_fn <file:///Users/smaal/zephyr-workspace/midi-synth/src/audio.c#L33-L69>`_
* Priority: -2 (time-critical cooperative)
* Description: Allocates audio frames from the internal memory slab. Invokes
  `synth_engine_render_block <file:///Users/smaal/zephyr-workspace/midi-synth/src/synth_engine.c>`_
  to generate audio samples and transmits the resulting block using I2S DMA.
  One block is queued before issuing the transmitter trigger command to prevent
  startup failures (NXP SAI I2S startup sequence requirement).

3. MIDI Input Processing Thread
===============================

* Source File: `src/midi_handler.c <file:///Users/smaal/zephyr-workspace/midi-synth/src/midi_handler.c>`_
* Entry Point: `midi_thread_fn <file:///Users/smaal/zephyr-workspace/midi-synth/src/midi_handler.c#L64-L74>`_
* Priority: 5 (preemptible)
* Description: Runs a blocking loop invoking ``midi1_serial_receiveparser`` over
  the UART serial interface. On receiving Note On or Note Off events, the driver
  callbacks construct ``synth_event`` structures and push them onto the
  synthesiser message queue. The waveform field of each Note On event is
  populated by calling ``synth_get_active_wave()``, so new notes always use the
  waveform currently selected by SW2.

4. SW2 GPIO ISR (Waveform Selector)
=====================================

* Source File: `src/waveform_selector.c <file:///Users/smaal/zephyr-workspace/midi-synth/src/waveform_selector.c>`_
* Entry Point: ``sw2_pressed`` (GPIO interrupt callback)
* Context: Interrupt (ISR)
* Description: Fires on the falling edge of the SW2 push-button (GPIO0 pin 23,
  active-low with pull-up). Computes the next waveform by incrementing the
  current ``synth_wave`` value modulo ``SYNTH_WAVE_COUNT``, then posts a
  ``SYNTH_EVT_SET_WAVE`` event to the synthesiser message queue. The ISR itself
  performs no direct state mutation; all state changes occur in the audio thread
  context via ``process_events()``. Adding a new waveform requires only extending
  the ``synth_wave`` enum; no change to the selector module is necessary.

DSP & Waveform Generation Engine
*********************************

The core audio synthesis module is implemented in
`src/synth_engine.c <file:///Users/smaal/zephyr-workspace/midi-synth/src/synth_engine.c>`_.

* Structure: ``struct voice_card`` represents a single synthesis channel with
  the following attributes:

  * ``note``: The MIDI note identifier.
  * ``wave``: The waveform assigned to this voice (``SYNTH_WAVE_SINE`` or ``SYNTH_WAVE_SAW``).
  * ``phase_inc``: The 32-bit phase increment from the frequency lookup table.
  * ``velocity_scale``: The Q15 amplitude scaling factor.
  * ``envelope``: The per-voice ADSR state machine.
  * ``age``: Monotonically increasing counter used to identify the oldest active voice.

* Global state: ``struct synth_state`` holds the voice array, the note counter,
  and ``active_wave`` — the globally selected waveform applied to new notes.

* Voice Stealing: ``handle_note_on`` assigns incoming notes using a three-pass
  priority algorithm:

  1. **Pass 1** — a slot in the ``END`` state (fully silent, no artefact).
  2. **Pass 2** — the ``RELEASE``-phase voice with the lowest ``current_gain``
     (nearest to silence, minimal audible click).
  3. **Pass 3** — the voice with the lowest ``age`` counter (oldest held note,
     last resort).

  Any stolen voice (passes 2 and 3) has its envelope re-initialised to
  ``ATTACK`` before the new note parameters are applied.

* Waveform switching: ``handle_set_wave`` updates ``active_wave`` and propagates
  the new waveform to all currently active voices immediately, so the timbre
  change takes effect on the next rendered block.

* Rendering: ``synth_engine_render_block`` processes pending events, advances
  per-voice ADSR envelopes once per block, then iterates over all samples in the
  block. For each active voice it:

  * Computes a sine sample via ``arm_sin_q15`` or a sawtooth sample by
    truncating the upper 16 bits of the 32-bit phase accumulator.
  * Scales the sample by velocity and envelope gain (both Q15).
  * Accumulates all voice contributions into a 32-bit sum.
  * Applies a right-shift (``MIXER_SHIFT``, derived from ``MAX_VOICES``) and
    clamps to the Q15 range before writing to the interleaved stereo output buffer.

ADSR Envelope Generator
***********************

The envelope module is defined in
`src/adsr.h <file:///Users/smaal/zephyr-workspace/midi-synth/src/adsr.h>`_
and implemented in
`src/adsr.c <file:///Users/smaal/zephyr-workspace/midi-synth/src/adsr.c>`_.

* Structure: ``struct adsr`` tracks the phase state, remaining lifetime,
  initial lifetime, start gain, current gain, and sustain level.
* Function: ``adsr_process`` advances the envelope by ``delta_samples`` (one
  block length). It decrements the lifetime counter, handles state transitions
  when the counter reaches zero, and computes the interpolated ``current_gain``
  for the block. All three interpolation paths (ATTACK, DECAY, RELEASE) include
  a zero-guard on the ``initial_lifetime`` divisor to prevent division by zero
  when a phase duration is configured to 0 ms.
* States: ``ATTACK`` (linear ramp to peak), ``DECAY`` (linear fall to sustain
  level), ``SUSTAIN`` (constant level until Note Off or maximum sustain duration),
  ``RELEASE`` (linear fall to zero), and ``END`` (silent).
* Amplitude scaling: When ``use_log`` is set, ``current_gain`` is squared
  (``gain² >> 15``) to approximate a perceptually linear volume response.

Waveform Selector Module
*************************

* Source File: `src/waveform_selector.c <file:///Users/smaal/zephyr-workspace/midi-synth/src/waveform_selector.c>`_
* Header: `src/waveform_selector.h <file:///Users/smaal/zephyr-workspace/midi-synth/src/waveform_selector.h>`_
* Description: Configures SW2 (``DT_ALIAS(sw2)``) as an interrupt-driven GPIO
  input. Each press advances the global waveform selection by one step, wrapping
  at ``SYNTH_WAVE_COUNT``. The ISR posts a ``SYNTH_EVT_SET_WAVE`` event to the
  synthesiser queue and returns immediately, keeping the interrupt path short
  and free of direct state writes. The devicetree alias ``sw2`` is defined in
  the board overlay and maps to ``user_button_2`` (GPIO0 pin 23, active-low
  with hardware pull-up) on the FRDM-MCXN947.

=============================
Synthesizer Engine Roadmap
=============================

This document outlines the architectural roadmap and future enhancements for the Zephyr-based MIDI synthesizer.

Current Focus
=============

Control-Rate vs. Audio-Rate Decoupling (Multi-Rate DSP)
------------------------------------------------------
Decouple audio-rate signal generation (oscillator phase accumulation and wavetable lookups) from control-rate parameter modulation (ADSR envelopes, LFOs, and MIDI controller inputs). Envelopes are updated once every N samples (control period) to minimize CPU overhead.

Future Work
===========

1. Multi-Timbral Routing
------------------------
Map the active patch index to the MIDI channel (0–15) to support multi-timbral synthesis. This allows assigning independent patches and polyphony limits per MIDI channel.

2. Wavetable Morphing (Vector Synthesis)
----------------------------------------
Define a 2D wavetable array for patches, enabling real-time interpolation between different single-cycle tables (e.g., sine to sawtooth transition) modulated by LFOs or envelopes.

3. Modular Modulation Matrix
----------------------------
Implement a routing table to dynamically connect modulation sources (Envelopes, LFOs, MIDI CCs) to destination parameters (Pitch, Panning, Amplitude, Filter Cutoff) with configurable scaling factors.

4. Portamento / Pitch Glide
---------------------------
Slew active phase increments toward new target frequencies over a configurable time period during Note On events, replacing immediate frequency steps with smooth transitions.

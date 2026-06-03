# Synth Engine Enhancement: Roadmap

## Objective
Refine the synthesizer's performance and expressive capabilities through advanced voice management and orchestral presets.

## Tasks

### 1. Refine the "Strings" Patch
- [ ] **Slow Envelopes**: Create a `g_strings_param` with:
    - Attack: 1000ms
    - Decay: 500ms
    - Sustain: 0 (Infinite)
    - Release: 1500ms
- [ ] **Sawtooth Strings**: Default the "Strings" patch to use the Sawtooth waveform for a richer, more orchestral sound.

### 2. Polish & Optimization
- [ ] **Advanced Voice Stealing**: Implement prioritized search in `handle_note_on`:
    1. Empty slots (`END`).
    2. Notes in `RELEASE` phase (starting with quietest).
    3. Oldest held notes.
- [ ] **Sustain Pedal Handling**: Add support for MIDI CC 64 to hold notes in SUSTAIN phase until pedal release.

---
*Status: Polyphonic Sawtooth Engine verified. Moving to advanced expression and presets.*

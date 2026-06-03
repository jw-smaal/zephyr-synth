# Synth Engine Enhancement: Waveforms & Presets

## Objective
Expand the synthesizer's sonic capabilities by adding multiple waveforms and refining the "Patch" system.

## Tasks

### 1. Implement Sawtooth Waveform
- [ ] **Define Waveform Types**: Add `enum synth_wave_type` to `synth_engine.h`.
- [ ] **State Update**: Add `waveform` field to `struct voice_card` in `synth_engine.c`.
- [ ] **Mixer Branching**: Update the rendering loop to generate either Sine or Sawtooth based on the voice's waveform setting.
    - Sawtooth Formula: `sample = (q15_t)(phase_acc >> 16)`
- [ ] **Global Setting**: Create a global variable or shell command to toggle between Sine and Sawtooth for new notes.

### 2. Refine the "Strings" Patch
- [ ] **Slow Envelopes**: Create a `g_strings_param` with:
    - Attack: 1000ms
    - Decay: 500ms
    - Sustain: 0 (Infinite)
    - Release: 1500ms
- [ ] **Sawtooth Strings**: Default the "Strings" patch to use the Sawtooth waveform for a richer, more orchestral sound.

### 3. Polish & Optimization
- [ ] **Polyphony Expansion**: Increase `CONFIG_VOICECARD_VOICES` to 16 or 32 in `prj.conf`.
- [ ] **Voice Stealing Refinement**: Prioritize stealing voices in the `RELEASE` state.

---
*Status: ADSR Complete. Moving to Waveform implementation.*

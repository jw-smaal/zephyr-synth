# ADSR Implementation Assignment

## Objective
Implement a per-voice ADSR envelope generator that attenuates the sine wave based on the note's lifecycle (Attack, Decay, Sustain, Release).

## Tasks

### 1. Update `adsr_process` Signature & Timing
- [ ] Modify `adsr_process` in `adsr.h` and `adsr.c` to accept `uint32_t delta_samples`.
- [ ] Update the `lifetime` decrement logic to subtract `delta_samples`.
- [ ] Handle state transitions when `lifetime` hits zero (e.g., Attack -> Decay).

### 2. Implement Gain Interpolation (`adsr.c`)
- [ ] **ATTACK**: Calculate `current_gain` as a linear ramp from 0 to `FULL_LEVEL` (32767).
    - Formula: `Gain = (1.0 - (lifetime / initial_lifetime)) * 32767`
- [ ] **DECAY**: Calculate linear ramp from `FULL_LEVEL` down to `sustain_level`.
- [ ] **SUSTAIN**: Set `current_gain` to a constant `sustain_level`.
- [ ] **RELEASE**: Calculate linear ramp from `sustain_level` down to 0.
    - Formula: `Gain = (lifetime / initial_lifetime) * sustain_level`

### 3. Integrate with Synth Engine (`synth_engine.c`)
- [ ] **Tick the Envelopes**: In `synth_engine_render_block`, loop through each voice and call `adsr_process(&voice->envelope, samples)` before the rendering loop.
- [ ] **Note Off -> Release**: Update `handle_note_off` to trigger the `RELEASE` state of the envelope instead of setting `gate_open = false`.
- [ ] **Active Voice Logic**: Update the mixing loop to check if `envelope.state != END` instead of checking the `gate_open` boolean.
- [ ] **Apply Attenuation**: Multiply the `velocity_scale` by the `envelope.current_gain` to get the final sample volume.

### 4. Initialization
- [ ] Update `handle_note_on` to initialize the `initial_lifetime` and `lifetime` for the `ATTACK` phase.
- [ ] Define a macro `MS_TO_SAMPLES(ms)` to convert time parameters into sample counts.

---
*Note: Aim for block-accurate updates (once per 128 samples) first as it is more CPU efficient.*

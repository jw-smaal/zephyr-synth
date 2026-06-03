# ADSR Implementation Assignment

## Objective
Implement a per-voice ADSR envelope generator with "Piano-style" (time-limited) sustain support.

## Tasks

### 1. Update `adsr_process` Signature & Timing
- [x] Modify `adsr_process` in `adsr.h` and `adsr.c` to accept `uint32_t delta_samples`.
- [x] Update the `lifetime` decrement logic to subtract `delta_samples`.
- [x] **State Sync**: In `adsr_process`, handle state transitions when `lifetime` hits zero.
- [x] **Initial Lifetime**: Restore and reset `initial_lifetime` whenever the state changes.
- [x] **Curve Selection**: Add a `bool use_log` parameter to `adsr_process`.

### 2. Implement Gain Interpolation (`adsr.c`)
- [x] **ATTACK**: Gain = `((initial - lifetime) * 32767U) / initial`
- [x] **DECAY**: Linear ramp from 32767 down to `sustain_level`.
- [x] **SUSTAIN (Piano Mode)**: Hold `sustain_level` for `param.sustain` samples, then transition to **RELEASE** automatically.
- [x] **RELEASE**: Linear ramp from `sustain_level` down to 0.
- [x] **Log Curve**: Implement the "Square Trick" when `use_log` is true: `Gain = (Gain * Gain) >> 15`.

### 3. Integrate with Synth Engine (`synth_engine.c`)
- [x] **Fix Syntax**: Fix spelling (`g_param.rekease`) and missing commas in `adsr_process` calls.
- [x] **Move the Tick**: Move `adsr_process` call out of the sample loop and into the voice loop (once per 128 samples).
- [x] **Note Off Trigger**: Update `handle_note_off` to force a transition to **RELEASE**.
- [x] **Active Voice Logic**: Render only if `envelope.state != END`.
- [x] **Apply Attenuation**: Multiply sine by `velocity_scale` AND `envelope.current_gain`.

### 4. Initialization
- [x] Fix `synth_engine_init` to access `envelope.lifetime` correctly.
- [x] Define `MS_TO_SAMPLES(ms)` with `uint64_t` cast.

---
*Status: ADSR implementation 100% complete!*

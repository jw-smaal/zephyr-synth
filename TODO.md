# Synth Engine Enhancement: Roadmap

## Objective
Refine the synthesiser's performance and expressive capabilities through
advanced voice management, hardware controls, and orchestral presets.

## Completed

- [x] **Polyphonic Sawtooth Engine**: Verified working on FRDM-MCXN947.
- [x] **ADSR Division-by-Zero Guard**: Zero-duration phases (attack, decay,
      release) now snap to their target gain instead of faulting.
- [x] **ADSR Reset on Voice Steal**: Stolen voices are always re-initialised to
      ATTACK, consistent with the free-slot path.
- [x] **`SAMPLES_PER_BLOCK` Consistency**: `audio.c` `conf.block_size` now uses
      the named macro instead of a bare literal.
- [x] **Three-Pass Voice Stealing**: `handle_note_on` now prioritises free
      slots, then the quietest releasing voice, then the oldest held note.
- [x] **SW2 Waveform Selector**: SW2 push-button cycles the global waveform
      (SINE → SAW → …) via a GPIO ISR that posts to the synthesiser queue.
      New notes from MIDI always use the currently selected waveform.

## Pending

### 1. Refine the "Strings" Patch
- [ ] **Slow Envelopes**: Create a `g_strings_param` with:
    - Attack: 1000 ms
    - Decay: 500 ms
    - Sustain: infinite (`0xffffffff`)
    - Release: 1500 ms
- [ ] **Sawtooth Strings**: Default the "Strings" patch to `SYNTH_WAVE_SAW`.

### 2. Additional Waveforms
- [ ] **Square Wave**: Add `SYNTH_WAVE_SQUARE` to the `synth_wave` enum and
      implement the render path in `synth_engine_render_block`. SW2 cycling
      and MIDI note-on will pick it up automatically.
- [ ] **Triangle Wave**: As above for `SYNTH_WAVE_TRIANGLE`.

### 3. Polish & Expression
- [ ] **Sustain Pedal**: Add support for MIDI CC 64 to hold notes in the
      SUSTAIN phase until pedal release.
- [ ] **Per-Patch ADSR**: Allow waveform selection (via SW2) to also switch
      the active `adsr_param` set (e.g. short attack for SINE, slow attack
      for SAW strings).
- [ ] **Logarithmic ADSR**: Replace the squared-gain approximation in
      `adsr_process` with a proper lookup-table based logarithmic curve.

---
*Status: Three-pass voice stealing and SW2 waveform selector implemented.
Moving to additional waveforms and patch presets.*

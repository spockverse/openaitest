# Input-driven distortion revision

## Investigation before algorithm changes

A real processor test loaded two WAVs through the existing background decoder,
profile exchange and processBlock. MUTATE0 outputs were identical; MUTATE100
outputs differed (normalized waveform difference 0.62). The route was connected.
However, envelope normalization and clipped derivative-brightness descriptors
collapsed many timbres. Previous waveform-difference tests could reward carrier
phase changes without demonstrating useful timbre differences.

The old sound was not sample playback or an oscillator summed onto the input.
It nevertheless imposed elapsed-time vowel sweeps and two fixed-frequency ring
carriers. Those can perceptually dominate input identity and repeat across patches.
They have been removed, rather than hidden by a gate.

## Signal path

Input -> protected low split -> 4x minimum-phase interpolation/filtering -> smooth
asymmetric rational waveshaping and input-derived harmonic folding -> anti-alias
filter/downsample -> intentional bit/rate reduction -> DC removal -> three
input-responsive resonant bands -> protected-low recombination -> wet/output ->
existing 5ms dry power crossfade.

There are no free-running carrier phases, vowel LFOs or tone generators in the
audio path. A waveform-dependent sine of atan(input) is a bounded transfer curve,
not an oscillator. Envelope attack/release, transient contrast and live brightness
move the resonances. Steady periodic inputs can settle; they are not forced through
a recurring vowel cycle. Digital silence fades residual history smoothly over the final 20ms of an 80ms tail, then clears it.
Minimum-phase oversampling introduces filtering phase, not a fixed block delay.
There is no added lookahead or new host parameter. Deliberate crusher aliasing is
preserved; the nonlinear shaper's unwanted folding is treated separately.

## ROT DNA

WAV analysis remains off the audio thread. Adaptive FFT windows (at most 24Hz bin
spacing) measure a 12-band spectral envelope, three separated resonance peaks,
flatness, harmonic concentration, even/odd balance, crest and centroid. These are
heuristic descriptors, not a classifier or exact fundamental/formant estimator.
Profile data shapes asymmetry, harmonic excitation, band emphasis, bandwidth and
resonant frequencies. Derived weights are prepared off the audio thread. Audio
processing reads immutable descriptors without allocation or locking.

LOOP/RETRIGGER retain the sample envelope as bounded coefficient modulation,
weighted by live articulation. The sample is never replayed. MUTATE blends static
spectral identity as well as modulation; a quiet section of the donor does not
silently remove its entire distortion profile.

Optional dnaSpectrum state embeds 20 descriptors. Exact new-state recall is tested.
Older envelope/brightness-only states remain loadable and retain all controls and
DNA display; the editor prompts reloading the original WAV for richer analysis.
New saves then need no external WAV. Plugin IDs, parameter IDs/order, presets,
editor geometry, power bypass and UI artwork remain unchanged.

## Verification and renders, before Windows build

- Sine, saw, short stabs, sustained low-pass bass and silence.
- Loaded profiles and no profile: zero influence at MUTATE0, distinct matched-level
  outputs at 50/100, and distinct timbre with identical envelope/brightness arrays.
- Actual WAV-worker -> embedded descriptors -> processor output and state recall.
- No-DNA output invariant to arbitrary transport-time changes. Fundamental 55Hz
  remains, with generated 165Hz harmonics. No continuing loop in gaps, including
  with all donor profiles at MUTATE100.
- Stab RMS-envelope correlation: 0.977. Selected folded products relative to the
  fundamental in a controlled 9kHz nonlinear test: 0.314 at 1x vs 0.076 at 4x
  (about 12.3dB reduction). This does not establish alias-free operation everywhere.
- Metallic/vocal-like/percussive profile pairwise matched-level waveform differences:
  0.246, 0.259, 0.349 relative RMS. Spectra corroborate this: metallic fixture has
  about 2.83% power at 1–4kHz vs vocal-like 0.94%; vocal-like has 11.71% at 200–1000Hz
  vs metallic 5.06%; percussive has 0.55% at 4–20kHz vs metallic 0.076%.
- Ten seconds of stereo DSP with a real spectral profile took about 0.17 seconds
  locally. This is a microbenchmark, not an FL Studio CPU measurement.
- Existing multi-rate automation/reset/mono/stereo, exact dry bypass, legacy states,
  processor integration and real VST3 binary tests pass locally.

The prior Corrosion reference measurements remain useful: almost unchanged RMS,
retained sub, additional upper harmonics/resonances and original rhythmic structure.
They motivate the input-derived exciter and moving resonances, not an inferred copy
of a proprietary algorithm. Listening files are level-matched synthetic probes,
including a synthesized vocal-like donor, not a genuine human voice. Local extra
renders process the supplied reference inputs; private recordings are not committed.
Automated measurements are not a guarantee of subjective yoi quality. FL Studio
listening is still required. Audition WAVs were generated/shared before Windows CI.

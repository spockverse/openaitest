# Digital-malfunction / ring-modulation audition — experimental

These files contain SYNTHETIC audio only. No supplied recordings, private-input
renders, or extracted video audio are committed or included in this artifact.

00-dry and quadrant/drive montages use four inputs in order: sine sub, filtered
bass, FM growl, already-distorted saw. Each lasts 2 s, with 250 ms gaps. Segments
are RMS matched to 0.12. Quadrants 0/1/2/3 mean TL/TR/BL/BR, at X/Y +/-0.55.
Compare each quadrant's ring-OFF and ring-ON files first. Quantization and hold
remain active in both. DRIVE 0/50/100 comparisons use top-right; DRIVE changes
carrier stepping and input-derived even-harmonic excitation, not output gain.
DNA 0/1/2 = synthetic metallic/vocal-like/percussive profiles. Each DNA montage
plays MUTATE 0, then 50, then 100 on the identical filtered input, DRIVE 0.

The prototype uses true bipolar multiplication in a parallel ring branch.
A smoothed low-band period estimate follows stable monophonic input; when it is
unreliable, a bounded delayed-input modulator replaces the oscillator gradually.
Carrier state is continuous, frequency targets are bounded, and carrier/descriptor
coefficients update at a 256-sample control interval. XY targets are cached.
No autonomous audible oscillator, sample playback, resonance sweep or wavefolder.
A low-pass estimate of processing error is subtracted, retaining the original
low-frequency excitation instead of adding a bass EQ boost. Quantization/hold
and carrier stepping supply the digital artifacts; there is no treble boost.

These are audition hypotheses, not an approved engine. The tracker is experimental
and is not a general polyphonic pitch detector. DNA here demonstrates profile-
dependent modulation/quantizer control; full existing spectral-bank integration,
host automation smoothing and START/END/SPEED/mode integration are deferred.
The production plugin, UI, IDs, state formats and existing DSP versions are untouched.
Basic finite-output, dry-centre, zero-DNA equality and silence checks passed.
No actual processBlock allocation guard, ASIO callback benchmark or plugin build
is claimed for this standalone offline prototype. Those follow sound approval.

Reference analysis: supplied before/after files are each approximately 3.344 s.
Overall RMS differs about 0.08 dB for the bright pair and 0.21 dB for the filtered
pair. Measured 30–180 Hz spectral shares: bright 68.88% -> 67.85%, filtered
88.41% -> 85.93%; at matched overall level these correspond to approximately
-0.07/-0.12 dB changes in band-energy share. This is not absolute fundamental gain.
The source clips are not phase-aligned; low-band correlation has periodic lag
ambiguity. Exact plugin settings, source identity and transient alignment cannot
be inferred from these audio pairs alone. Numerical similarity is not a listening
approval, and no attempt to reproduce proprietary DSP is asserted.

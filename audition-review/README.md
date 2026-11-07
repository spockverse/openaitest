# WUBROT listening review — experimental, not a plugin build

Start with 00-inputs.wav, then compare matching -v042 / -candidate files.
Every six-input montage contains: sine sub, low-pass bass, synthetic FM growl,
already-distorted saw, rhythmic stabs, triangle sub. Each lasts 2 seconds followed
by 250 ms silence. Each segment is RMS matched to 0.12; matching does not prove
perceptual loudness equality. No limiter is applied during matching.

Quadrants 0/1/2/3 = top-left/top-right/bottom-left/bottom-right. Mid/high use
radii 0.5/0.9, DRIVE zero, no DNA. Drive files compare 0/50/100 at X .55 / Y -.45.
DNA montages compare MUTATE 0, then 50, then 100 on the identical low-pass source:
0 metallic, 1 vocal-like, 2 noisy percussion. All references are synthesized.

Candidate changes are exploratory: uniform quantization versus companded levels,
convex dual-clock sampling versus primary hold, modest pre-quantizer saturation,
original excitation plus filtered distortion difference, and broad fixed spectral
bands whose gains follow the existing DNA timeline. No moving high-Q peak sweep.
The candidate is not yet integrated or optimized for the real-time callback.
Its per-sample control calculations must be cached/smoothed before integration.
No UI, parameter IDs, presets, project recall, or production DSP were changed.

Reference observations (revised with user-confirmed dry opening): supplied video
is 52.2 s, AAC stereo 44.1 kHz. The opening is DRY, not continuously processed.
Private dry 0–10 s and later 18–52.2 s extracts were isolated for analysis only.
The 6.0–6.5 s dry template was aligned by normalized 30–180 Hz waveform correlation
against later windows. Best matches start near 20.721, 33.136, 42.245 and 46.387 s;
correlations are 0.890/0.956/0.970/0.853. After whole-excerpt RMS matching, sub-band
energy differences are approximately -0.089/-0.042/-0.065/-0.064 dB, and 180–700 Hz
body differences approximately -0.74/-0.70/-1.01/-0.70 dB. Upper-band energy changes
much more. Large relative high-band dB differences have a near-silent dry denominator
and must NOT be interpreted as recommended gain boosts or a quality score.

The video shows Amount about 82% around 20–25 s and 100% later; visible XY locations
also change. We cannot attribute those changes exclusively to XY. Exact normalized
coordinates, isolated plugin input/output stems, identical note envelopes and host
routing/automation are not available. Correlation aligns similar low-frequency
phrases, not proven identical performances. These RMS-matched band comparisons are
not absolute plugin transfer gains or a sample-accurate transient-retention test.
AAC encoding further limits high-frequency/true-peak interpretation. Detailed
alignment diagnostics are in aligned-reference-analysis.json. No user reference
audio, video, extracted clips or processed derivatives are distributed.

Validation: synthetic silence, exact dry centre, MUTATE-zero/no-DNA equality and
finite bounded output checked for this offline candidate. Metrics are diagnostic,
not evidence that its sound is musically better. No full Windows build, release,
production integration or audio-thread certification has been performed for it.

Listening decisions needed: bass weight versus dry; useful crunch versus scratch;
distinct quadrant personalities; DRIVE progression; DNA character without whistle.

# Control-impact correction: processing model 4

This is a targeted extension of the existing quantizer/hold, four-times nonlinear
residual stage and three-resonance bank. No new oscillator, sample playback,
real-time FFT, routing, latency, UI or parameter was introduced. Worker analysis,
128-frame temporal DNA, START/END/SPEED/mode clock and profile ownership are unchanged.

## Diagnosis and correction

The unchanged HTML DRIVE input sends raw 0–36 values, APVTS stores 0–36 dB and the
processor forwards that value. The Windows WebView test now sends actual input
events at 0/18/36/0 and checks both raw values and normalized 0/0.5/1/0 values.
The processor reference test independently checks the parameter-to-DSP path.

v0.2.41 divided the entire shaped residual, including its folding component, by
excitation gain. Increasing DRIVE therefore also suppressed generated harmonics.
A dominant protected fundamental, additive clean resonance path and output
compensation further masked the change. Increasing a gain knob alone would not
correct that balance.

Model 4 keeps the compensated saturating residual but gives its input-derived
folding component a separate bounded scale with gentler gain normalization.
DRIVE uses the full existing range through a smooth nonlinear excitation curve;
zero adds no DRIVE excitation. XY and DNA can still excite their own processing
at DRIVE zero. More of the input fundamental can enter this path away from the
unchanged dry centre, and compensation no longer removes the added harmonic
energy. The same fraction of protected input is removed and re-added, so this
does not create another source signal.

Both nonlinear residual terms still have zero small-signal slope. The unfiltered
linear excitation is retained; the earlier full-signal filtered/dry cancellation
is not restored. Existing oversampling and intentional crusher artifacts remain.

The existing signed character interpolator has a stronger continuous response
for shaping/reconstruction/resonance in model 4. **The coordinate convention,
bit-depth/rate targets, distance curve and centre gate are identical to model 3.**
Warm-region body uses lower targets in the existing resonance bank and additional
input-derived asymmetry. There are no switched quadrant chains.

MUTATE's spectral contrast now rises sooner, reaching noticeable reference-dependent
weighting at its midpoint. High mutation progressively replaces part of the clean
carrier with the existing profile-filtered response, strengthens normalized spectral
contrast and bounded persistent resonance, and drives sample-dependent folding.
Frequency, width and temporal descriptors still come from the loaded reference;
reference loudness is not an output-gain control. No DNA and MUTATE zero produce
identical core output from equivalent initial states.

An exact-boundary test found peak 1.163 on a 0.9-peak saw at far right, DRIVE 36,
without DNA. Extra smooth crest headroom is now limited to bit settings below 4,
increasing toward 2 bits. This avoids attenuating the main DRIVE comparison points
or adding a post-output limiter. The final tested full-boundary maximum is 0.956;
interior input-level sweeps peak at 0.910. It changes level conditioning, not quantization
levels, hold rate or phase.

## Verification and listening

`Control-impact.md` in the AudioComparisons artifact reports the pre-regression
model 2, v0.2.41 model 3 and revised model 4 using identical synthetic inputs.
It includes DRIVE 0/18/36 harmonic ratios, absolute harmonic energy, output
RMS/peaks and spectral distribution, plus multiple DNA references at MUTATE
0/50/100, normalized spectral comparisons and equal-distance quadrants.
The two-harmonic near-centre case remains in that report; no spectral-distance
threshold is treated as evidence of musical quality.

For example, on the sine test at X=0.45/Y=0:

| Path | DRIVE 0 THD | DRIVE 18 THD | DRIVE 36 THD |
|---|---|---|---|
| Pre-regression model 2 | 2.29% | 10.52% | 19.84% |
| v0.2.41 model 3 | 2.66% | 2.59% | 3.52% |
| Revised model 4 | 8.82% | 36.17% | 48.76% |

The revised harmonic RMS rises from about 0.0189 to 0.0771 to 0.1044 while output
RMS remains about 0.215/0.227/0.239. Thus DRIVE changes generated harmonics rather
than simply multiplying the complete output. These figures are control checks,
not a quality score or a claim that the musical result has been approved.

Auditions are whole-file RMS matched to 0.12 without a limiter. They use synthetic
stabs and synthetic metallic/vowel/percussion reference profiles. No user recordings
or derivatives are distributed. Model-2/model-3/model-4 comparisons and the four
quadrant examples are named in the accompanying README.

Regressions cover frozen v0.2.41 sample-exact automation, earlier processing paths,
neutral/dry and zero-DNA behavior, silence and note gaps, source pitch, low and high
input levels, exact XY boundaries, reference-level invariance, worker replacement,
hold-phase continuity, callback C++ allocation/reclamation and project/preset recall.
All 12 local native CTests passed; the updated browser fresh-state test passed at
Small, Medium and Large. Actual Windows VST3/WebView2 checks are distinct from browser tests and FL Studio.
**FL Studio/ASIO behavior and subjective listening still require user evaluation.**

## Cost, state and workflow

The same shared topology and approximately 250 Hz descriptor/control mappings are
retained. Fixed control arrays grow by a few floats; profile storage is unchanged.
No locks, allocations, state serialization, filesystem calls or UI calls were added
to audio processing. More harmonics are intentional; no additional oversampling
or SIMD/lookup architecture was added.

The existing paced native callback benchmark compares model 3 and model 4 in the
same Release executable. Model 3 is verified against frozen v0.2.41 DSP, so an
unchanged historical processor does not need another full build. This measures
processing-path cost, not an exact old binary's total application cost. Mean,
p95/p99, observed maxima, hardware, memory and editor-open/closed results are in
the Performance artifact. Local editor-closed data is in `benchmarks/impact`.
Shared-runner outliers and lack of ASIO-device scheduling prevent hard real-time
claims. Existing older measurements remain available rather than being regenerated.

Fresh instances use processing/preset container 4. Saved models 1/2/3 retain their
original sound, coordinates and automation. Analysis stays at version 2 and no
new parameter ID is added. Test the correction with a fresh INIT instance; loading
an old preset deliberately restores its old processing model. State plumbing only
extends accepted version bounds; preset management operations are unchanged.

Existing regression assertions still run, with independent native tests scheduled
concurrently locally. Historical audition regeneration is opt-in using
`WUBROT_LEGACY_AUDITIONS`; requested new auditions are generated normally. Delivery
uses one Windows development workflow after local checks, without a public release.

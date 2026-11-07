# Sine-carrier audition candidates — isolated experiment

Synthetic audio only. No user recordings or their derivatives are included.
Production WUBROT is unchanged; these are hypotheses for listening, not an approved engine.

Start with 00-dry, 01-v042-baseline, then A-97Hz, B-97Hz, C-97Hz.
Every file contains five 2-second inputs separated by 250 ms silence:
filtered bass, complex FM bass, harmonic/wavetable-like bass, already-distorted
saw, pure 55 Hz sine. Each individual segment is RMS matched to 0.12;
this does not guarantee equal perceived loudness. WAVs are mono 48 kHz PCM16.

A: bipolar sine multiplication blended with the original input.
B: complementary low-frequency foundation (240 Hz one-pole split), sine
modulation and broad bounded saturation above that split. A 6 kHz low-pass
conditions the processing difference, not the entire input. No high-Q sweeps
or claimed true formant synthesis.
C: complementary low foundation plus uniform quantization and fractional
sample-and-hold. Compare ring-before-quantize against quantize-before-ring.

97 Hz files use depth 55%, 8 bits and 6 kHz hold where applicable.
277 Hz files use depth 70%, 5 bits and 2 kHz hold where applicable;
C additionally uses a seven-level stepped sine carrier. These are character
comparisons: frequency, depth and C degradation change together, not an
isolated frequency experiment. A/B retain a continuous sine carrier.
Carrier frequency has a restrained input-envelope-dependent variation,
bounded and smoothed. There is no pitch tracker or rhythmic LFO.

Baseline: existing model 4 / v0.2.42 processing, X=0.55, Y=-0.45,
DRIVE=0, MUTATE=0, no DNA. This is a fixed baseline, not equivalent controls.
No new XY mapping, DRIVE mapping, DNA/MUTATE integration or plugin build.

Engineering observations, not listening approval: full-band A can remove
fundamental energy; B/C preserve it substantially better. On the matched
55 Hz sine, fundamental amplitudes were dry 0.170, baseline 0.145,
A 0.088–0.129, B 0.168–0.169, C approximately 0.169.
This makes B/C candidates worth auditioning for bass retention; it does not
prove their metallic character or musical usefulness. No preferred sound
has been selected by the developer on the basis of these numbers.

Private before/after references were analyzed and both dry inputs rendered
through A/B/C locally. Whole-file low-band energy shares stayed substantial
in the supplied processed references. MP3 encoding, imperfect note/phase
alignment and unknown processing settings prevent exact transfer-function
claims or inferring Corrosion's proprietary algorithm. Private renders remain
local and are excluded from this download and repository.

Checks: finite bounded output, exact silence at 44.1/48/96 kHz, A zero-depth
dry equality, and oscillator continuity across 64/257 sample partitions.
This is not FL Studio validation or a processBlock allocation/latency test.
prototype-cost.csv measures offline kernel loop time including stores/checks,
excluding file I/O and normalization. It is not a real-time callback benchmark.
Full host/performance/integration validation follows selection of a sound.

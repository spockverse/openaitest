# WUBROT input-driven DSP audition

Start with these four WAVs. Each uses the IDENTICAL low-pass bass input and is
matched to a common RMS level, with headroom reserved for transient examples:

1. 01-no-DNA-matched.wav
2. 2-metallic-DNA100-matched.wav
3. 3-vocal-like-DNA100-matched.wav
4. 4-percussive-DNA100-matched.wav

The source WAVs are clearly named DNA-source-*-synthetic.wav. The vocal-like
fixture is synthesized harmonic/formant material, not a human vocal recording.
These fixtures are reproducible probes, not proof of how every real sample sounds.

listen-sine, listen-saw, listen-stab and listen-lowpass dry/processed pairs show
the processing on different sources. listen-DNA-0/1/2/3-50 and -100 files cover
metallic, vocal-like, percussion and sustained tonal profiles at MUTATE 50/100.
All listen/numbered audition files share approximately -23.3 dBFS RMS; no peak
limiter or per-file clipping is used. Unprefixed source/processed files retain
actual levels. Settings: DRIVE20dB, 9 bits, 16kHz effective rate, 100% wet,
output -6dB, low protection ON. Input-derived filter movement is enabled.

There is NO carrier oscillator or clock-driven vowel LFO in this engine.
A 4x filtered nonlinear curve generates harmonics from the input, then a live
input envelope/transient/brightness follower moves resonant filtering. Intentional
bit/rate artifacts follow the oversampled shaper. Filter history decays and clears
within 80ms of digital silence. No audio profile data is ever played back.

DNA profiles now retain a 12-band spectrum, three resonance frequencies, spectral
flatness, harmonic/even balance, crest and brightness, plus the existing envelope.
MUTATE blends those into the curve, harmonic emphasis and resonances. Sample LOOP
and RETRIGGER affect bounded coefficient movement in response to live articulation.
New spectral data saves with the project. Old envelope-only projects remain valid;
reload the original WAV once to create the richer profile. Plugin identity and
parameter IDs are unchanged. Power bypass remains exact dry after a 5ms crossfade.

Tests verify actual WAV-worker -> profile -> processor routing, MUTATE zero equality,
matched-level sample/timbre differences, input pitch/harmonics, transport-time
invariance without DNA, short-note gaps/silence and reduced folded products in a
controlled 9kHz shaper test. Metrics do not guarantee a subjective Corrosion match.
Real FL Studio listening remains necessary.

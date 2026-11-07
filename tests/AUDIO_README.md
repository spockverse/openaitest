# Centred XY audition (new mapping)

Start with `xy-centre-dry-synthetic-stabs.wav`: this is the unchanged synthetic input.
Compare `xy-horizontal-crunch-drive0.wav`, `xy-vertical-rate-drive0.wav` and
`xy-combined-drive0.wav` on that same source. For a fair DRIVE comparison use
`xy-combined-drive0-matched.wav` and `xy-combined-drive36-matched.wav`.
The three `xy-dna-*-drive0-matched.wav` files use the identical bass and different
synthetic analysis donors, all at MUTATE 100%, DRIVE 0, matched to 0.12 RMS.
The donor recordings are not played or distributed. These tests measure changes;
they do not establish subjective sound quality. Audition them in your normal setup.
All files below without the `xy-` prefix exercise the retained legacy sound model.

# WUBROT clean crusher and sample-region audition

All files generated for distribution are synthetic. No user reference recordings
or derivatives are included. Vocal-like fixtures are synthesized, not human vocals.

Start with clock-test-before-raw-hold-matched.wav and
clock-test-after-reconstruction-matched.wav: same 100Hz source at 4bits/1.6kHz,
matched RMS. The rebuilt DAC stage reduces unwanted clock images, while a separate
DC test confirms four-bit quantization still operates. Extreme XY remains lo-fi.

listen-upper-right-* files cover sine/saw/stab/lowpass at about 3.82bits/1.602kHz,
DRIVE0/36 with a synthetic donor. These approximate the supplied screenshot, not
an exact reproduction of its unavailable loaded WAV profile.

region-source-synthetic.wav has vocal-like and metallic halves. Compare
region-first-half-matched.wav / region-last-half-matched.wav on identical bass.
region-speed-* files demonstrate the five speeds on a selected modulation region.
The incoming note pitch is not transposed. LOOP and MIDI/transient RETRIGGER operate
on selected region time. Profile spectra are reanalysed from the selected samples.

01–4 compare no DNA / metallic / vocal-like / percussion on identical low-pass bass.
listen-* copies share a conservative common RMS, chosen to leave transient headroom;
clock-test files are matched separately. Raw files retain output levels. No limiter
or gain increase is used to fake an improvement. Samples aren't an audio layer.

New editable projects retain the user-loaded WAV in project state (20MB source
limit; first 30 seconds editable), along with the active analysis and region/speed.
This increases project size. Legacy analysis-only states remain playable; reload
the original WAV to enable editing. WAV replacement retains normalized selection.
No factory content or reference recording is embedded in plugin resources.

The existing power button, main UI scale, identity and parameter IDs are preserved.
Three new parameters are appended: regionStart, regionEnd, dnaSpeed. Off remains
exact dry after a 5ms crossfade. Tone/DNA coefficients smooth region changes.

Automated tests check selected-region spectra, reopening and editing without an
external file, MUTATE0 equality, five speeds, LOOP/end/retrigger timing, silence,
clock rejection, pitch, rhythmic envelopes, peaks and existing compatibility.
Real FL Studio listening and wrapper validation still need the user test.

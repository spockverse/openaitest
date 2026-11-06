# WUBROT tonal-engine audition

Start with the listen-*.wav files: these share a target RMS of -15.9 dBFS.
Compare dry, no DNA, smooth DNA at 100% MUTATE and metallic DNA at 100%.
Every render uses the identical four-second 55 Hz sine/saw bass. The DNA
fixtures are synthetic sounds supplied here for reproducibility, not playback layers.
The numbered files retain actual output levels. Settings: DRIVE 20 dB,
9 bits, 16 kHz effective rate, 100% wet, output -6 dB, low protection on.

The carrier is multiplied with the input. Two resonant formants sweep oo/ah/ee.
X still controls bits and also carrier range/movement speed. Y still controls
sample rate and also tonal position. DRIVE engages the tonal stages as well as
saturation. MUTATE controls sample-derived envelope/brightness contributions
to carrier frequency/depth, vowel position and resonance. No additional controls.

LOOP repeats the analysis; RETRIGGER restarts it on transients/MIDI notes.
Older saved envelope-only profiles load with neutral brightness; reloading the
original WAV enables the new analysis. New projects embed both descriptors.
Plugin identity and parameter automation IDs are unchanged. The sound changes.

Automated checks verify 211/411 Hz sidebands from a 100 Hz input and 311 Hz
carrier, no leaked carrier, moving formant peaks, distinct DNA outputs,
MUTATE-zero equality, silence, bounded peaks and state recall. These tests do
not establish a subjective match to Corrosion or replace FL Studio listening.

Update: a second input-multiplied upper-mid carrier adds texture from subdued input. Engine On is an appended automatable parameter. OFF uses a 5ms dry/processed crossfade, then bit-exact unity dry, independent of output/mix settings. Controls and DNA keep running during bypass.

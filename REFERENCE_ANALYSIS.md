# Additional riddim references: measurements and DSP decisions

Analyzed all four user MP3s before changing processing. Files are not committed.
All decode to 3.3437 seconds, stereo. Measured 48 kHz decoded PCM; STFT 4096,
hop 1024; averaged power across channels to avoid phase cancellation. Energy
shares are relative spectral power, not amplitude or perceived loudness. Motion
is standard deviation of centroid in frames above -20 dB relative frame power.
MP3 encoding and unknown reference settings limit inference about the processor.

| Metric | Full dry | Full processed | Low-pass dry | Low-pass processed |
|---|---:|---:|---:|---:|
| RMS dBFS | -19.05 | -18.96 | -18.19 | -17.99 |
| Peak dBFS | -5.18 | -1.55 | -6.39 | -5.49 |
| Spectral centroid Hz | 234 | 514 | 97 | 157 |
| Centroid motion std Hz | 115 | 308 | 22 | 52 |
| Energy below 200 Hz % | 85.68 | 85.12 | 97.86 | 94.76 |
| Energy 1–4 kHz % | 2.93 | 2.68 | 0.0021 | 3.17 |
| Energy 4–12 kHz % | 0.86 | 3.78 | 0.000028 | 0.24 |

Full bass: average level barely changes, but upper-band energy and spectral
movement increase. Peak level rises more than RMS, so simply raising gain or
adding more clipping is a poor substitute. The strongest low harmonics remain.

Low-pass bass: the important result is newly generated upper-mid energy, not
just amplification of the existing low harmonics. Prominent new peaks cluster
around 1324–1383 and 1570–1664 Hz. The original roughly 35-Hz-spaced harmonic
series remains visible. This is consistent with harmonics/sidebands being
emphasized in resonant regions. It does not prove ring modulation, a particular
filter topology, or a proprietary algorithm. Greater centroid movement is a
useful proxy for spectral animation, not direct proof of a vowel or carrier.

## WUBROT decisions

Keep protected lows, existing bit/rate controls and moving formants. Add a
second inharmonic, input-multiplied carrier at 700 + 2.4 * main carrier Hz,
limited relative to sample rate. A quieter second harmonic enriches its texture.
Its contribution depends on DRIVE, XY and DNA character. It creates upper-mid
content from subdued input, rather than applying only an EQ lift or summing a
free-running oscillator/noise source. Formants then focus that generated content.
No reference algorithm was copied, and no subjective match is claimed.

ROT DNA retains independent envelope and frequency-content descriptors. Tests
now also hold envelopes identical while changing analyzed timbre and require
substantial output differences. At MUTATE zero all DNA influence remains absent.
LOOP/RETRIGGER and embedded spectral profile recall remain intact.

Engine On is a new parameter appended after all existing parameters. Existing
IDs and order remain unchanged. The footer toggle uses a sample-rate-independent
5ms linear crossfade. OFF is the exact input, independent of output gain. DSP
continues running so toggling preserves modulation and filter history. Tests
compare the entire transition against an always-running wet reference and the
original dry signal at 44.1/48/96 kHz, mono/stereo, repeated reversals and +6 dB
output. Old states without Engine On default to ON, including after an OFF state.

Local browser tests check button events, automation feedback and clean footer
alignment at all three sizes. Processor tests check dry bit identity, project
recall and retained DNA. Native VST3 binary smoke tests do not substitute for
FL Studio playback and visual validation.

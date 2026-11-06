# Development update: metallic carrier, talking formants and spectral DNA

- Added genuine ring modulation and two moving, peak-normalized resonant formants after the crusher. DRIVE engages these stages, XY maps carrier range and vowel movement, and the existing bit-depth/rate mapping remains intact. Carrier/filter controls are smoothed; the carrier is never mixed directly into audio.
- ROT DNA previously retained only a normalized volume envelope, and its bit/rate offsets could hit their floors. Added sample-rate-compensated derivative-energy brightness analysis. DNA now modulates carrier frequency/depth, vowel position/resonance and digital degradation. Reduced redundant bit/rate offsets to keep tonal movement legible.
- MUTATE zero is tested sample-independent. LOOP/RETRIGGER remain available. No UI changes, parameter/identity changes or public release. Optional embedded dnaCharacter state restores exactly; older envelope-only states use neutral brightness and remain loadable.
- Added measured sideband, static/moving-formant, DNA-difference, silence and peak tests plus reproducible, level-matched audio renders as a separate Actions artifact. Existing automation, multiple-source, multi-rate, processor recall and VST3 binary tests pass locally; Windows Actions runs the same suites.
- On the synthetic test bass, previous vs new/no-DNA spectral-centroid movement measured 10.2 vs 59.9 Hz standard deviation. RMS was -7.3 vs -13.7 dBFS, so this is not an output-gain increase. Two DNA fixtures gave normalized RMS difference 1.17. Metrics demonstrate a change, not a subjective guarantee of yoi quality.
- User MP3 references were measured locally, not committed: both are sub-heavy, with >99% of measured STFT energy below 200 Hz. Durations differ, so direct unaligned waveform subtraction would be misleading. No Corrosion implementation was copied.

# Development update: header sizing and stronger crush character

- Replaced the footer size dropdown with a header SIZE popup containing exactly Small, Medium and Large buttons. Preserved native editor dimensions and saved size recall.
- Aligned ENGINE ACTIVE left, sample rate/stereo centrally and animation status right.
- Added signal-dependent spectral emphasis before the antialiased saturation stage. XY now increases midrange crunch, bright grit and harmonic density, with subtle coefficient movement. Existing stereo differences are narrowed or emphasized by the XY mapping; mono stays mono. No audio oscillator, noise layer or sample playback is added. Low-end protection remains active.
- ROT DNA stays optional; MUTATE, DRIVE and LOOP/RETRIGGER retain their existing roles. No parameter IDs, plugin IDs, program names or state schema were changed. Existing projects load, though the approved distortion improvements intentionally change the processed sound.

Validation: DSP tests cover sustained bass, stabs, drum transients and clean saws without DNA, harmonic generation, bounded peaks, silence, sample modulation, automation and reset at 44.1/48/96 kHz. Processor and real VST3 binary smoke tests cover state and audio integration. Browser checks cover three sizes, popup bounds/selection/Escape, native bridge, restored selection, footer alignment and XY coordinates. Windows Actions runs native processor/editor and VST3 binary tests. Real FL Studio listening and host-window testing still require a Windows tester.

Browser test: install Playwright, then run `node tests/SizePopup.cjs`. Optional WUBROT_PLAYWRIGHT and WUBROT_CHROMIUM select local installations.

Download the successful GitHub Actions run's WUBROT-0.2.<run number>-dev-Windows-x64 artifact. Close FL Studio, extract the ZIP, and replace the entire WUBROT.vst3 bundle in C:\Program Files\Common Files\VST3. Keep a copy of the previous ZIP for rollback. Actions artifacts expire after 90 days. No public release or installer is created.

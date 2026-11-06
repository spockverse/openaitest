# Development update: header sizing and stronger crush character

- Replaced the footer size dropdown with a header SIZE popup containing exactly Small, Medium and Large buttons. Preserved native editor dimensions and saved size recall.
- Aligned ENGINE ACTIVE left, sample rate/stereo centrally and animation status right.
- Added signal-dependent spectral emphasis before the antialiased saturation stage. XY now increases midrange crunch, bright grit and harmonic density, with subtle coefficient movement. Existing stereo differences are narrowed or emphasized by the XY mapping; mono stays mono. No audio oscillator, noise layer or sample playback is added. Low-end protection remains active.
- ROT DNA stays optional; MUTATE, DRIVE and LOOP/RETRIGGER retain their existing roles. No parameter IDs, plugin IDs, program names or state schema were changed. Existing projects load, though the approved distortion improvements intentionally change the processed sound.

Validation: DSP tests cover sustained bass, stabs, drum transients and clean saws without DNA, harmonic generation, bounded peaks, silence, sample modulation, automation and reset at 44.1/48/96 kHz. Processor and real VST3 binary smoke tests cover state and audio integration. Browser checks cover three sizes, popup bounds/selection/Escape, native bridge, restored selection, footer alignment and XY coordinates. Windows Actions runs native processor/editor and VST3 binary tests. Real FL Studio listening and host-window testing still require a Windows tester.

Browser test: install Playwright, then run `node tests/SizePopup.cjs`. Optional WUBROT_PLAYWRIGHT and WUBROT_CHROMIUM select local installations.

Download the successful GitHub Actions run's WUBROT-0.2.<run number>-dev-Windows-x64 artifact. Close FL Studio, extract the ZIP, and replace the entire WUBROT.vst3 bundle in C:\Program Files\Common Files\VST3. Keep a copy of the previous ZIP for rollback. Actions artifacts expire after 90 days. No public release or installer is created.

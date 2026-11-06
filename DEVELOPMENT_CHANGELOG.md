# Preset browser and portable user library

- Replaced the native preset select with a floating black/purple browser: FACTORY/MY PRESETS tabs, live case-insensitive search, keyboard navigation, 170 ms opening/closing transitions, reduced motion and bounded DOM rendering for large libraries. Active name and an always-visible dirty indicator remain in the existing header position.
- Added named Save Preset/Save/Save As, rename/delete, native import/export pickers, conflict and delete prompts, Open Preset Folder and Save/Discard/Cancel switching. A processor-side guard catches unsaved changes that arrive between UI refreshes.
- User presets live in the current OS-resolved Documents/Wubware Audio/WUBROT/Presets folder. Versioned .wubpreset files use validated temporary writes/atomic commits and case-insensitive conflicts; factory files are never managed as user files.
- Portable presets embed exact active DNA and full temporal/sliced spectral analysis, without original audio. Saved sounds recall exactly; new regions can be edited from the analysis archive and are explicitly labeled as analysis-only. Project states remain independent of preset files and retain the existing original-source behavior when applicable. See PRESETS.md for the format and the distinction between exact saved-region recall and analysis-based new-region editing.
- Added pinned browser regression checks and browser-only screenshot artifacts to Windows Actions. All six local CTest suites pass, including new CRUD/state/audio/rapid-restore tests. All six browser/motion suites pass, with preset workflows at three sizes and existing XY/footer checks at 100–200% display scaling. Native Windows/FL Studio interaction, pickers, redirected/OneDrive Documents and ACL behavior still require user-side validation.
- Distortion/DNA algorithm files and all three compiled VST3 class IDs are unchanged. No new automation parameters, installed filename change, installer or public release. Bundled factory settings are preserved; their factory WAVs have not yet been supplied.

# Branding polish: ink-edge subtitle alignment and neon wordmark bloom

- Aligned WUBWARE AUDIO with the vector lettering's visible left edge, accounting for the existing SVG inset at every scale. Subtitle wording, vertical spacing and header/control positions are preserved.
- Added a static, alpha-based pink-purple inner halo and soft outer bloom. Original SVG paths, cuts, fill and proportions are unchanged; visible overflow preserves the glow outside the SVG box.
- Verified byte-identical SVG artwork, subtitle/visible-ink alignment, unchanged control bounds, screenshots and rendered bloom clearance at all three sizes and 100/125/150/200% browser display scales. Existing region-editor checks pass. Actual FL Studio UI verification remains user-side.
- No DSP, parameters, controls, project state, native dimensions or plugin-identity changes. Portable Windows development build only; no public release.

# UI polish: suppress browser context menus

- Disabled native WebView2 default context menus in the pinned JUCE build and canceled document context-menu defaults from the HTML head. Policy is local to WUBROT's embedded browser; no hooks into FL Studio's wrapper or global keyboard suppression.
- Right-clicks no longer start knob or region-marker drag gestures. Left-click, keyboard, accessibility, sample loading, layout, DSP and plugin identifiers are preserved.
- Verified browser right-click defaults on canvas, SVG, images, knobs, markers, power, upload and header/panel elements at all three sizes. Left drags, balanced knob automation gestures, keyboard edits/Tab, sample-loader routes and region editing pass; XY checks pass at 100–200% display scale. The native policy applies idempotently to the pinned JUCE source. Actual FL Studio/WebView2 UI interaction remains a user-side check.
- Portable Windows development build only; no public release.

# Branding update: reference wordmark and WUBWARE AUDIO

- Replaced the stretched, glowing text title with embedded SVG outlines traced from the supplied geometric wordmark, including its horizontal cuts. All letters, including the final T, fit inside the logo bounds with padding. No external font or asset dependency.
- Header subtitle and VST3 manufacturer display name now read WUBWARE AUDIO. Omitted the registered-trademark symbol because registration has not been confirmed.
- Product/bundle name, bundle ID, manufacturer/plugin codes, parameter IDs, project state and existing storage paths remain unchanged. No DSP, sample editor, control or jelly-physics changes.
- Portable Windows development artifacts only; no public release or installer.
- Verified header screenshots at all three sizes and glyph containment/spacing at 100/125/150/200% browser display scales. Region-editor/XY checks pass. All five local processor, audio, content and VST3 suites pass; all three compiled VST3 class IDs match the previous binary. Actual FL Studio UI validation remains a user-side check.

# Compatibility update: audio drops, native browse and synchronized profile cursor

- Added JUCE native file-drop targets, browser payload variants/local-path forwarding, a native WAV/MP3 picker behind the upload button, header-based decoding and explicit errors. MP3 decoding and project-source reopening supported.
- Profile clock now pauses with stopped host transport and silent input; handles seeks/loops and stale timestamps. RETRIGGER keeps attack/MIDI behavior and holds END after completion. The UI follows lightweight real DSP positions without autonomous interpolation.
- Distortion formulas, sound-design stages, presets, IDs, UI layout and branding remain unchanged. Only the requested modulation-clock behavior changes. See LOADING_TRANSPORT.md for validation and native Windows delivery limitations.
- Added native callback-versus-byte loading tests, PCM16/24/float32 format/rate coverage, synthetic MP3 decoding/recall, processor transport tests and browser loader tests. No public release.

# Visual-only update: attached orb corona and keyboard focus

- Separated the stable corona positioning anchor from the breathing transform; orb and flare now share one canvas-coordinate point. No shader, jelly physics, pointer-to-parameter mapping or DSP code changed.
- Flare growth pivots at its stems instead of scaling the stems outward from the orb. Long flares shorten gently near the rim, with square SVG layers and visible overflow rather than hard clipping. Existing purple corona artwork, rim and reflections are retained.
- Removed explicit green focus styles from pointer interactions. Keyboard navigation retains subtle purple outlines/glow on controls and the field; the green engine status and intended active badges remain unchanged.
- Browser regression tests measure the anchor and corona-rim centres within 0.1 CSS pixel of the canvas orb, attached flare roots, motion/paused states, cardinal/diagonal boundaries, unchanged XY dragging and keyboard/pointer focus at three UI sizes and multiple display densities. Windows FL Studio compositor behavior still needs the user check.

# Development update: clean quantizer reconstruction and ROT DNA mini-editor

- Added input anti-alias filtering, fractional clock capture and output reconstruction to rate reduction. About 29dB lower clock-image ratio in the controlled 4bit/1.6kHz test. Quantization remains active; very low bit depths intentionally stay coarse.
- DNA no longer sweeps XY bit/rate values. Oversampled extra drive now follows reconstruction; removed a separate envelope-dependent output gain. DRIVE0 isolates the bare crusher.
- Added mockup-style cyan region handles/highlight/playhead, compact five-speed selector with mini step encoders, and synchronized LOOP/RETRIGGER controls inside ROT DNA. Main XY/knobs/footer geometry unchanged.
- Exact selected samples are reanalysed off the audio thread. Changes to spectral shaping are smoothed. Silent regions disable DNA. New user-loaded sources are embedded only in user project state for offline editing/recall, not plugin resources or build artifacts. Legacy states load; reload original WAV for editing. First 30s/20MB source limits remain.
- Appended three automatable parameters without altering existing IDs/order. Region/speed, actual analysed bounds and active profile save; restores retain the editable source. Five speeds and LOOP/RETRIGGER timing, region contrast and recall, clean clock behavior and browser interaction/layout checks added.
- Synthetic auditions generated before Windows build. Uploaded reference recordings are analysis-only and excluded from every distributed file. No public release.

# Development update: input-derived tonal grit and spectral ROT DNA

- Removed elapsed-time vowel sweeps and both free-running ring carriers. Live envelope, transient contrast and brightness now control coloration of the incoming waveform's own harmonics; silence and note gaps cannot play an independent yoi layer.
- Added filtered 4x nonlinear processing, smoother asymmetric shaping, input-derived harmonic excitation and gentler drive mapping. Removed the extra post-crusher clipper; intentional bit/rate artifacts remain separate.
- Replaced envelope/brightness-only DNA with spectral bands, resonance peaks, harmonic balance, flatness and dynamics controlling curve and filtering. Verified the original loading route before changing algorithms; added real decoder/publication/processor routing checks.
- Added level-matched no-DNA/metallic/vocal-like/percussive comparisons, 50/100 MUTATE cases, sine/saw/stab/lowpass pairs, no-clock-motion and gap-silence checks, rhythm/pitch preservation checks and an alias-folding comparison. See DSP_INPUT_DRIVEN.md.
- New spectral descriptors save with projects. Existing projects/automation/identity remain compatible; reload legacy DNA WAVs once to enable the richer profile. No UI, power, sizing or preset redesign. No public release.

# Development update: header power and complete footer containment

- Moved the circular illuminated power control beside the preset selector, matching the latest reference. Reinstated the read-only footer ENGINE ACTIVE / ENGINE BYPASSED status. The existing automatable Engine On parameter and click-free DSP bypass are unchanged.
- Fixed the underlying height mismatch: 1200x1000 could not contain the main interface, footer and lower border, especially with multiline DNA status. The web canvas is now 1200x1040, with matching logical JUCE editor dimensions: Small 600x520, Medium 804x697, Large 996x864. Main controls retain their previous width/scale; no global shrinking, scrollbars or letterboxing.
- Centralized native dimensions in EditorGeometry.h for construction, size switching and restored bounds. Heights round up to avoid fractional clipping; JUCE/WebView2 apply Windows DPI once. Old saved size indices remain valid.
- Bounded long DNA diagnostics to three lines with the full message on hover, preventing them from pushing the footer out of the window.
- Browser tests cover all three sizes at DPR 1/1.25/1.5/1.75/2, short and oversized messages, both power states, footer margins, no overflow, circular header placement, automation feedback, popup bounds and unchanged XY scale/dragging. Five local C++/processor/VST3 suites pass. Windows CI checks editor dimensions and reopening; actual FL Studio wrapper/display-scale testing remains a user validation step.

# Development update: dry power bypass and upper-mid texture

- Analyzed both additional reference pairs before modifying the DSP. See REFERENCE_ANALYSIS.md for measurements, interpretation and limits.
- Added a second signal-multiplied upper-mid carrier to create expressive high-frequency texture from subdued input, with no separate oscillator/noise playback.
- Replaced ENGINE ACTIVE with a footer ENGINE ON/OFF power button. Automatable appended Engine On parameter, default ON, saved in project state. OFF crossfades over 5ms to bit-exact dry independent of output gain. All settings and DSP history remain intact.
- Tested continuous crossfade math against a running wet reference, exact bypass, rapid toggles, mono/stereo, 44.1/48/96kHz, legacy state defaults, project recall and three-size browser alignment.
- Added a timbre-only ROT DNA comparison with identical envelopes to verify meaningful frequency-content influence independently of rhythm.

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

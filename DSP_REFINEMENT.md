# Signed XY and temporal DNA (processing model 3)

This extends the existing crusher, four-times nonlinear stage and three-resonance bank. It does not run four parallel distortion engines, play reference audio, add a carrier, use a real-time FFT, add oversampling, or change the controls. Fresh defaults, XY coordinates (screen Y is positive downward), axis orientation, radial strength, centre gate and five-millisecond power bypass stay as in v0.2.37. Old models 1 and 2 retain their exact processing paths.

## Findings before editing

The four mirrored XY settings were identical because both reduction axes used absolute values. DNA supplied one full-region spectrum plus a time-varying amplitude/brightness curve. The archive had 64 spectral slices, but region selection averaged them down to one spectrum before playback.

The crusher used fourth-order input and output low-pass filters, both around 0.43 times the hold rate. That deliberately suppressed clock images, but removed much of the sought digital texture at low rates. A separate issue was the nonlinear stage: a filtered four-times path was cross-mixed against an unfiltered copy. In an isolated nominally linear stage test (not the whole plugin), the old 45/55 mix measured gain 0.992 at 1 kHz, 0.537 at 8 kHz and 0.0457 at 12 kHz. This demonstrated cancellation/attenuation rather than a lack of treble gain.

Paced actual `processBlock()` baseline timings and synthetic quadrant renders were captured before Source changes. `benchmarks/v037-linux-before-edits.json` preserves that initial measurement. Windows CI additionally compiles the exact pre-change commit `b6851c0f7a8f86a451f694b762e52d539e49c94f`, with the same benchmark harness as the new code.

## Signed regions, one topology

The established distance curves still provide bit depth, hold rate and radial activity. A smooth signed vector interpolator adjusts independent bounded character parameters. X remains responsible for bit depth and Y for hold rate: character offsets cannot introduce rate reduction when Y is zero or bit reduction when X is zero.

| Direction | Quantization/rate tendency | Reconstruction and character |
|---|---|---|
| Top-left | Coarser quantization, relatively defined hold rate | Harmonic crunch, moderate reconstruction |
| Top-right | Coarse, lower effective hold rate | Stronger input-derived folding, less clock-image suppression |
| Bottom-left | Finer quantization, higher effective hold rate | More reconstruction, gentle warm conditioning |
| Bottom-right | Lower effective hold rate | Pronounced held steps, very little output reconstruction |

Weights are continuous, not quadrant switches. Control targets update around 250 Hz and interpolate over that interval. The existing 20 ms parameter smoothing remains; frequency descriptors interpolate in logarithmic frequency space. MUTATE uses a zero-reaching linear 20 ms ramp in model 3. Quantizer levels themselves are not smoothed into continuous audio. The hold accumulator is retained across blocks and rate updates; block-partition tests compare actual processor output at 64 versus 512 samples with changes at the same absolute positions.

The folder's small-signal slope is cancelled with a bounded correction, so its residual cannot act as a hidden broadband gain copy. The four-times nonlinear stage now adds only its filtered nonlinear residual to the original excitation. The full original signal is not passed through that low-pass path or mixed against its delayed filtered copy. The isolated small-signal test remains at unity at the above frequencies. Reconstruction remains stronger in the intentionally warmer region; it is reduced in the metallic and held-step regions. No compensating treble shelf or post-clipper was added.

There is no new fixed sample delay: the centre and power bypass retain their original undelayed dry path, and the host still reports zero samples. Intentional hold age and frequency-dependent filter phase are effects, not a constant transport delay that a dry-path FIFO could correct. The residual topology removes the diagnosed full-signal parallel cancellation rather than guessing an oversampling delay and delaying the neutral/bypass path. The existing amount of oversampling is unchanged.

## Temporal DNA

Model 3 reference analysis adds up to 128 fixed-capacity spectral frames, using overlapping approximately 60 ms analysis windows, plus the existing 2,048-point temporal envelope/character data. Each frame holds normalized spectral shape, log resonance frequencies, bandwidth-derived bounded resonance, and confidence that a peak recurs across the reference. Absolute reference loudness is excluded from spectral shape. Noise-like frames broaden resonances rather than treating every isolated FFT peak as a persistent tone.

These are compact descriptors, not PCM or a wavetable. Analysis runs on the existing worker. Full-region analysis is reused; portable-preset archives reuse these frames instead of repeating those FFTs. Runtime reads/interpolates immutable descriptors at the control rate, prepares bounded filter targets, and interpolates their application. No browser clock or browser audio analysis participates.

Low MUTATE retains subtle asymmetry/buzz and midrange coloration. At high MUTATE, reference-dependent spectral contrast, resonances, bandwidths and time-varying input-derived harmonic folding become stronger. The reference never supplies an audible source. To excite those resonances on a simple low-pass bass, high mutation admits more of the incoming fundamental to the existing shaping path while retaining a protected sub component; this is not a reference-gain boost. Absent DNA and DNA at MUTATE zero produce identical core audio from equivalent initial state. The established centre remains dry even with DNA and DRIVE active.

START/END reanalyse a selected source region off-thread when PCM is available. Presets contain the exact active profile and full-sample descriptors, so recall does not need that PCM. Analysis-only region edits resample the stored timeline. The cache is bounded: at the 30-second limit its spectral centres are roughly 234 ms apart; the 2,048-point envelope retains finer rhythmic information. Selecting a short region from original PCM provides finer local spectral sampling; editing an analysis-only preset cannot recover spectral details absent from its cache. The existing analysis-only UI label remains accurate. SPEED and LOOP/RETRIGGER use the real processor profile clock.

## Real-time ownership and cost

Cached APVTS atomic pointers are read in `processBlock()`. That function does not serialize state, access files, call the WebView, allocate profile memory, or take the metadata lock. Profile publication retains the existing bounded four-slot immutable handoff. Writers serialize off-thread; audio/UI readers pin slots with lock-free integer atomics. The slots are allocated once in the owner before processing, and only the off-thread owner destroys their storage. No shared pointer or final heap owner is released by the audio reader. Keeping the larger fixed cache on the heap also avoids Windows stack pressure from multiple test/plugin instances.

Expensive spectral mappings and filter transforms run around 250 Hz, not per sample. Unchanged targets are cached. No SIMD, lookup approximation or extra oversampling was introduced. The callback heap guard observes C++ allocation/deallocation, including aligned allocation; direct source inspection also checks that no C allocator is used by the processing path. Worker analysis, preset serialization and test-file operations are outside that guard.

The native editor sends waveform data only on source-revision changes. Compact control state and the existing playhead/audio telemetry remain separate. JavaScript displays native bit/rate readouts, forwards gestures, and renders the existing scene. The former standalone browser audio-analysis fallback was removed; it was not used by the Windows native loader.

## Validation and its limits

Automated checks cover level-matched quadrant differences at radii 0.30, 0.55 and 0.90, harmonic-band structure, several synthetic DNA profiles, reference-level invariance, SPEED/region effects, silence, input timing, rapid automation, replacement during playback, stable peaks, zero-reaching mutation, fixed latency, callback heap activity and exact model-1/2 comparisons. Browser checks preserve all three sizes and the current jelly/grid/power behavior. Native Windows checks exercise the actual JUCE/WebView2 bridge and VST3 binary separately.

Local final validation passed all 11 native CTests and all 11 browser suites. The guarded callback recorded zero C++ allocations/reclamations. Synthetic high-input corner stress peaked at 0.9782. At matched radius 0.30, the least-separated quadrant pair still differed by 0.0775 normalized waveform RMS and 0.0120 normalized spectral distance. The minimum same-input, loudness-matched DNA pair distance increased from 0.0591 in v0.2.37 to 0.2226; these are objective differences, not a subjective quality score. `benchmarks/README.md` includes all local callback percentiles and memory measurements, including scheduling outliers.

CI's `Performance-comparison.md` reports paced actual processor callback mean, p95, p99 and observed maximum, sample rate, buffer size, memory, CPU/OS/RAM, and editor-open versus closed measurements. Open-editor runs use the real visible JUCE/WebView2 editor after a five-second warmup; their audition output must match closed-editor output exactly. These are hosted native measurements, not FL Studio/ASIO-device captures, offline render-speed claims or hard real-time guarantees. Process-tree memory includes readable live descendants and may count shared pages more than once. Actual FL Studio behavior and subjective sound quality still require listening/host validation.

New states/presets use processing model 3 and temporal analysis version 2. Preset containers 1/2/3 are checked against their processing model. Existing projects and presets remain on models 1/2; they are not silently reinterpreted. Test new behavior in a fresh instance. All distributed test audio is synthetic; no user reference recordings or derivatives are included. No public release or installer is produced.

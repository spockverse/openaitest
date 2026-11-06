# WUBROT interaction study

Open WUBROT.html in Edge or Chrome on Windows. It is self-contained, works without a server, and needs no installation or internet access. The HTML is a standalone visual preview and is also embedded in the Windows VST3 with a native audio/control bridge. Standalone HTML does not process audio. Source is available on the wubrot/audio-engine development branch; environment publication is separate.

## Agreed direction implemented for review

- Dark metal housing, purple gelatinous circular field, pink draggable orb, and enlarged ROT DNA panel.
- Click or drag anywhere inside the circle; outside motion projects onto its boundary. The orb stays where released. Pointer velocity excites a damped wave surface and an elastic body response: fast flicks make larger wakes than slow gestures. After movement stops, the gel overshoots, ripples briefly, and settles, even while the pointer is held down. There is no self-running jelly oscillation. Liquid physics never autonomously changes the parameter position.
- X maps to 16–2 bits, decreasing rightward so the upper-right increases destruction. Y maps to 500–44100 Hz effective sample rate, decreasing upward. Circular geometry limits which combinations are reachable; separate numeric controls should be added in the actual plugin so all combinations remain available.
- Liquid Physics controls the wave/spring response. The pearl has a miniature solar corona: five asymmetrical plasma prominences with staggered timing, 36 fine wisps, a slowly turning hot rim, and a 3.2-second breathing halo. The pearl material remains unchanged. All solar artwork is constructed once as inline SVG; small layers animate with transforms and opacity. Animation switches off the physics, halo, and every flare. Keyboard arrows and Home operate the field. Reduced-motion users start with all motion disabled.
- ROT DNA uses local WAV analysis and a waveform display. File reading/decoding highlights the loader. The file is never played or uploaded. Invalid files preserve the preceding loaded profile. Sample analysis estimates waveform roughness, not instrument identity: metallic and vocal samples can overlap in these features.
- MUTATE now controls how much sample contour and roughness shape the gel. The redundant DNA Influence slider is hidden. DRIVE remains a visual control study. These are visual demonstrations, not functioning audio knobs yet.

## Rendering budget

The housing, layout, glossy artwork, guides, and pearl material are retained. A single 640 × 640 WebGL pass now refracts the cached gel texture in two dimensions; the old horizontal strips and 30 FPS throttle are removed. A fixed 64 × 64 damped wave surface advances at 120 simulation steps per second, with a maximum of six steps after a delayed frame. Velocity-dependent impulses generate a directional wake; elastic body inertia adds a short overshoot. Circular boundaries absorb energy. Two-byte packed slopes with bilinear filtering keep the final small ripples from visibly stepping. Input positions remain separate from simulation coordinates.

Rendering follows browser refresh while the surface is active. Once settled, the simulation sleeps and its JavaScript/rendering loop stops. The solar corona continues on small independent layers, using fixed SVG paths and static bloom artwork rather than simulated particles. It does not excite the liquid. The static guides and pearl are only redrawn on UI changes. No particles, continuous sine-wave jelly motion, full-resolution CPU pixel processing, or per-frame allocations are needed. If WebGL is unavailable or its context is lost, a simpler continuous Canvas 2D spring transform preserves the artwork and controls; it lacks the local refractive waves. Motion pauses when hidden and can be switched off entirely.

The texture remains embedded in the HTML for a single offline download. Waveform analysis stays capped at 65536 probes, first 30 seconds, and 20 MB per WAV. These are bounded-work choices; target Windows CPU/GPU usage and VST integration are not yet measured.

## Current audio implementation

The C++ engine analyses a normalised 2048-bin sample envelope off the audio thread, then publishes immutable modulation data to the processor. MUTATE scales sample-envelope modulation of bit depth and effective sample rate; at zero the sample has no influence. LOOP follows the original sample duration and host timeline; RETRIGGER starts a one-shot pattern on bass transients or sample-positioned MIDI note-ons. Project state embeds the derived envelope, so audible DNA recall does not require the source WAV.

DRIVE uses antiderivative antialiasing before the intentional quantisation/sample-rate aliasing. Parameter smoothing, DC removal and optional low-band protection are implemented. Audio callbacks do not do file access or editor rendering. Windows native controls, sample byte transfer, preset events and host automation/state updates are bridged to this processor.

Sample-derived waveshaping, spectral/formant transfer, tempo-stretching, release signing and listening validation in real DAWs remain future work. Current release work targets Windows x64 VST3 only.

## Validation

Run `node tests/WubrotMotionTests.cjs` from the project root. The tests exercise no idle drift, no excitation from a stationary pointer, stronger fast-gesture response, inertial overshoot, decay to sleep, equivalent propagation at 60/120/144 Hz, and stability during rapid repeated flicks. The reference fast gesture settles in approximately 1.5 seconds; slow movement has a shorter tail.

Chromium interaction checks passed for the WebGL rendering path, matching pointer/pearl coordinates on both axes, a wake continuing after input stops, a fixed orb position during settling, rendering stopping at rest, a visibly varying glow, animation off, Canvas fallback, and reduced-motion defaults. No script or WebGL errors were observed. One headless Chromium run recorded a median interval of 16.6 ms between active renders (roughly 60 FPS); this is not a Windows hardware performance guarantee. Existing checks also passed for circular drag clamping, keyboard reset, WAV loading and decode-error recovery, clearing DNA, and mobile layout without horizontal overflow. The desktop appearance was visually inspected. This is browser validation, not a Windows VST or FL Studio test.

The solar-corona revision was additionally checked in Chromium for five independently timed flare layers, the rim, alignment with a moved orb, no idle JavaScript loop, normal liquid settling while the corona animates, complete animation shutdown under both motion-off and reduced-motion settings, and mobile layout without overflow. The enlarged field was visually inspected.

## Ambient ripple revision

The gel now also has continuous low-amplitude radial refraction and slow cross-flow in the existing WebGL pass. Gesture-driven waves remain separate and decay normally; the orb position stays fixed. The renderer now follows browser refresh even at rest when liquid and animation are enabled, replacing the earlier sleep-at-rest behaviour. The small physics simulation still sleeps when not excited. Canvas fallback uses a subtle continuous stretch. Motion-off, liquid-off, hidden tabs, and reduced-motion defaults stop ambient movement. Chromium checks passed for ambient time advancing, a stationary orb and inactive gesture simulation at rest, motion-off and reduced-motion stopping the loop, and no script/GL errors. Windows performance remains unmeasured.


Idle visibility correction: increased the radial displacement and travelling highlights so ripples read clearly without pointer input. The non-WebGL path now warps a cached image over a fixed 12 × 12 triangular mesh instead of merely stretching it. A Chromium pixel comparison at two idle times confirmed substantial changes across the jelly in both rendering paths (excluding the orb), while the orb stayed fixed and the gesture simulation remained inactive. Motion-off stopped both paths. The ambient-rendering loop remains active at rest; Windows CPU/GPU use still needs measurement.


## Embedded-sphere membrane revision

The artwork previously retained its original central cavity and irregular silhouette. A circle-preserving nonlinear disk mapping now places that cavity at the actual orb coordinates, with a uniform filled circular crop and a pinned boundary. The source texture is deformed rather than translated. Orb-relative waves, contact attenuation, and an independently damped material-lag state add local refraction and elastic/viscous response; the indentation remains at the new position after gesture energy decays. Guide rings follow the sphere while the crosshair stays fixed as a UI reference. The Canvas fallback uses a polar deforming mesh with the equivalent disk mapping. This is an interactive 2D membrane approximation, not a volumetric soft-body solver. Chromium checks exercised all four cardinal extremes, retained deformation and lag settling, both renderers, and script/GL errors. Both rendered views were inspected.


The current product target is Windows x64 VST3 only. macOS/AU and Linux releases are deferred. The approved web interface is embedded in the Windows plugin with a native parameter/sample bridge and a compact LOOP / RETRIGGER toggle in ROT DNA.

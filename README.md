# WUBROT — WUBWARE AUDIO

WUBROT is a digital tonal enhancer for incoming audio. Its tunable sine modulator adds grit, texture and inharmonic sidebands while retaining the source. ROT DNA adds optional sample-derived tone shaping; it never plays or layers the reference recording. Insert WUBROT on the audio track you want to colour.

## Master power

The existing power button retains its click-free dry bypass. OFF also extinguishes the lighting, turns the reflective materials metallic grey and freezes decorative animation and the visual DNA playhead. Preset browsing, size selection, file loading and controls remain functional. Powering back ON restores the original lighting and resumes visualization from the current settings. This uses the existing automatable enabled state; no audio-processing or parameter-identity changes.

The existing Crush Field movement also responds gently to low-end attacks in WUBROT's own incoming audio. A read-only three-band observer (30–180, 180–700 and 700–2500 Hz) prioritizes low/body energy and adds midrange articulation. It briefly boosts the existing ripple displacement and purple reflections, adding no new waves, controls, audio, or latency. Sustained bass settles to a gentle response capped at one tenth of the reactive boost rather than keeping the material maximized. Silence retains the original animation, and OFF/hidden/static-animation states suppress reactive rendering. It cannot hear other DAW tracks unless they are routed through this instance; there is no external sidechain input.

The thin targeting circles and crosshairs form a fixed, centered overlay. They never follow the orb or deform with the jelly, including during audio-reactive pulses. Audio intensifies only existing ripple displacement, with a separate six-pixel limit; it does not amplify the orb's bulk tension/stretch. The XY handle remains at its exact parameter position.

## Fresh instances and centred XY

New instances open as **INIT**: MUTATE 100%, DRIVE 0 dB, no DNA, XY exactly centred, and Output 0 dB. No factory sample or last-used sound is loaded automatically. At centre the new mapping passes the input unchanged (unless you explicitly change Output). DRIVE and DNA add intensity away from centre, not an independent sound at neutral.

In new instances, X tunes the existing continuous carrier and Y morphs its waveform: sine at the vertical centre, a rounded triangular shape toward the top, and a stepped sine toward the bottom. DRIVE controls modulation depth. The source keeps its bandwidth and precision; digital texture comes from audio-rate modulation rather than extreme input decimation. The carrier adds sum/difference frequencies and can deliberately sound out of key. Tune X by ear against the source; there is no automatic key, BPM or pitch-grid quantization.

**Existing projects, factory presets and old user presets retain their original mapping and sound.** New projects/presets save `xyMapping=14`; missing mapping metadata means the original mapping. Insert a genuinely fresh INIT instance to audition the new engine. Parameter IDs, ranges, automation, VST3 identity, region/speed/mode and DNA recall stay intact. Axis captions follow the recalled processing version.

## Tonal modulation and temporal DNA

Fresh instances use model 14, retaining the approved model-13 frequency/shape mapping. Above 50% DRIVE, a smooth internal curve adds more modulation presence and stronger stepped-carrier texture. The lower DRIVE half is sample-identical to model 13; the unity through signal and existing overload protection stay intact. X spans 20 Hz to about 5.5 kHz at a 44.1 kHz sample rate (6 kHz at 48 kHz and above), with a 300 Hz reference at its horizontal centre when no DNA is loaded. DNA can shift that reference; X retains its full tuning range at full MUTATE. PITCH transposes the sample-derived characteristics without changing SPEED. The single phase stays continuous when tuning or changing waveform. There is no independently audible oscillator: without input, the modulator produces silence. Source fundamentals, a unity through component and restrained sideband depth preserve body and articulation. The dry centre, DRIVE-0 texture, defaults, bypass and visual design stay intact. Saved models 1–13 retain their original bit/rate ranges and DRIVE response.

DNA adds normalized time-varying spectral shapes, persistent resonances and input-derived harmonic transformation. Higher MUTATE makes the reference's character more pronounced without playing its audio or adding an unrelated oscillator. Original reference loudness does not become output gain. Source-region analysis stays on a worker; runtime caches and profile ownership are bounded. Projects and portable presets retain their processing version, selected region, rhythmic/legacy timing and PITCH settings.

## Controls

- **Crush Field X:** tune the modulation frequency, low on the left and high on the right. Older sounds retain their original bit-depth mapping.
- **Crush Field Y:** morph the carrier shape around the centre sine. Older sounds retain their original sample-rate mapping. The circular boundary limits the reachable X/Y combinations.
- **X/Y coordinate controls:** the vertical faders and editable percentage boxes use the existing automatable **Crush Field X** / **Crush Field Y** parameters. X increases rightward, Y upward, and the dry centre reads 50.0% / 50.0%. Enter a number and press Enter or leave the box to apply; Escape cancels. The existing circular boundary projects points beyond the rim. Internal parameter ranges, IDs and automation directions remain unchanged.
- **ROT DNA:** drop or browse for WAV/MP3 audio, up to 20 MB. The first 30 seconds are analysed for temporal, spectral and harmonic characteristics. The analysis shapes incoming audio; the source is not played as a layer.
- **MUTATE:** blends the loaded DNA's temporal and harmonic characteristics into input-dependent processing. At 0%, the loaded DNA has no influence.
- **DRIVE:** the existing 0–36 parameter increases audio-rate modulation depth in new instances, adding stronger sidebands without increasing generic saturation or acting as output gain. Its historical parameter ID/range/host label remain unchanged. At zero, restrained XY texture still works independently. Saved versions retain their original DRIVE response.
- **LOOP / RETRIGGER:** the single cyan switch selects LOOP on the left and RETRIGGER on the right. In rhythmic timing, LOOP follows host tempo/phase while the DAW plays, including silence; RETRIGGER restarts at START on a detected bass attack or a MIDI note-on, reaches END once and holds there until the next trigger. Stopping the DAW freezes progression. The playhead displays the native processor's analysis position; no source audio is played. Without host transport/tempo, the native clock uses 120 BPM.
- **SPEED:** drag the rotary knob, use its wheel/arrow keys, or choose from the custom selector. Rhythmic divisions are **1/1, 1/2, 1/4, 1/8D, 1/8, 1/16D, 1/8T, 1/16, 1/16T, 1/32 and 1/64** (D = dotted, T = triplet). A complete START–END region spans the selected division; double-click resets to 1/4. Shift-drag gives finer stepping. Both controls share the existing automation parameter. Older saved sounds show LEGACY and preserve their original 0.25×, 0.5×, 1×, 2× or 4× timing until a rhythmic division is explicitly selected; legacy progression pauses during silence.
- **PITCH:** transposes sample-derived spectral/resonant features by −24 to +24 semitones in 0.01-semitone steps. It does not transpose the incoming bass, play the reference, or change SPEED's duration. Drag/wheel/arrow keys adjust it; Shift gives fine adjustment and double-click returns to 0 st. It has no effect without DNA or at MUTATE 0. Controls remain adjustable with grey lighting while the engine is OFF.

Rhythmic audio retrigger uses adaptive energy envelopes and a short guard time to avoid retriggering on individual waveform cycles. Very quiet or continuously sustained material may not provide a new attack. MIDI provides sample-offset timing when the DAW routes notes to the effect; routing differs between hosts and incoming notes are not passed through. Saved legacy timing retains its original detector.

Mix (default 100%), Output (fresh-instance default 0 dB), and Keep Sub are available as host automation parameters without adding more panels to the interface. Keep Sub preserves a gently filtered band around 120 Hz. Output is not a limiter; watch your mixer levels.

The approved black/purple interface, solar orb and deforming membrane are embedded in the Windows plugin. Native audio runs separately from the web renderer. Closing the editor does not stop processing. The cloud development build uses a native fallback editor for automated testing. Windows is the only current release target.

## Three factory presets and their samples

The base slots are **Riddim Monster**, **Liquid Glass**, and **Metal Teeth**. Each has its own controls and a separate sample folder:

```text
Resources/
  Presets/
    Factory/
      01 Riddim Monster/
        preset.json
        Samples/dna.wav
      02 Liquid Glass/
        preset.json
        Samples/dna.wav
      03 Metal Teeth/
        preset.json
        Samples/dna.wav
```

**The three factory WAVs have not been supplied yet.** The directories contain instructions, not substitute audio. Supply each preset's intended WAV at the corresponding `Samples/dna.wav` path. Preset names and settings can be refined when auditioning those samples. Development builds can load your own WAV immediately; factory presets without DNA still load their saved parameters, clear previous DNA and report the exact unavailable sample as a nonblocking warning. The intended originals are absent from the reachable Git history, so they have not been replaced with generated or unverified audio.

Installer work is deferred. The current delivery is a portable VST3 ZIP, with bundled factory manifests validated by CI. Check content with:

```sh
python3 tools/validate_factory.py                  # required for installers
python3 tools/validate_factory.py --allow-missing  # development manifests only
```

Factory content is read from the portable VST3 bundle under `Contents/Resources/WubrotContent/Presets/Factory`. User presets are stored separately in the OS-resolved Documents folder, never inside the installed bundle.

For development, set the non-secret `WUBROT_CONTENT_DIR` variable to the **Resources directory**, not to Factory. Installed use requires no variable. Projects save the sound and analysis independently of external preset files. Directly loaded source audio retains the existing project embedding behavior for full source-region editing; portable `.wubpreset` files contain analysis only. Source audio is never played as an extra layer.

## Downloading and replacing development builds

Open this repository's **Actions → Build WUBROT Windows**. Select a successful run and download **WUBROT-0.2.RUNNUMBER-dev-Windows-x64** from **Artifacts** at the bottom of the run page. GitHub downloads it as a ZIP. Each run has a unique version number; previous ZIPs remain available for 90 days. Download local backups for longer-term rollback. No standalone installer or public release is created automatically.

1. Close FL Studio and any other DAW using WUBROT.
2. Extract the ZIP. Remove/replace the old outer `C:\Program Files\Common Files\VST3\WUBROT.vst3` folder with the new outer **WUBROT.vst3 folder**, including all its contents.
3. Reopen FL Studio. Do not rename the installed folder or leave versioned copies in scanned plugin directories. If WUBROT is not listed, use **Options → Manage plugins → Find installed plugins**.

To roll back, repeat these steps using an older downloaded ZIP. Back up your projects before testing development updates. Internal plugin identity, parameter IDs and the existing project-state format remain stable.

The UI and jelly artwork are embedded in the plugin; no separate HTML/UI installation is needed. Factory content is bundled under Contents/Resources/WubrotContent. The Windows WebView2 Runtime remains a system prerequisite for the editor (already present on most current Windows machines). The three factory WAVs have not been supplied yet; load your own WAV into ROT DNA for now. Installer work is deferred.

## Windows DAW support

The current release target is **Windows x64 VST3**: FL Studio, Ableton Live, Cubase and other Windows VST3 hosts. The Windows CI build and audio/state tests passed. A separate real-VST3 loading/processing smoke test is included. Real listening and interaction tests in FL Studio and the other DAWs are still required; this is an early development build.

macOS, Audio Units, Logic Pro and Linux releases are deferred. There is no current macOS installer or AU build workflow. Source code is portable for local testing, but only Windows is being packaged now.

Windows development builds are unsigned. Signing remains a release task.

## In-plugin updates

This build checks the repository's latest **public stable GitHub Release** once per DAW process session when an editor opens. Development Actions artifacts, drafts, prereleases, unavailable networks and rate limits do not produce prompts. LATER collapses the notice into a glowing update badge beside the preset selector. Click it to reopen the update menu; reopening an editor in the same session keeps the badge minimized. The gear remains grey when no update is known or a check fails; open it and choose CHECK FOR UPDATES to retry. Existing users must install an updater-capable build once.

UPDATE NOW downloads inside WUBROT, shows actual byte progress, and checks the asset's GitHub SHA-256 digest and archive paths/size. CANCEL leaves the installed version alone. A bundled helper waits for the DAW to exit before swapping the complete bundle, retaining a rollback copy. Windows may request administrator approval for protected installation folders. Wait for its completion confirmation before reopening the DAW. No running DAW is terminated. Projects, user presets and embedded DNA outside the bundle are untouched.

For a stable build, manually dispatch **Build WUBROT Windows** with `version` set to the intended three-part release version (for example `1.0.0`). Attach the successful Windows ZIP to the latest GitHub Release tagged `v1.0.0`. The tag must match the binary version. Keep `WUBROT`, `Windows`, `x64` and `.zip` in the asset name. GitHub supplies its `sha256:` asset digest; releases without a supported ZIP/digest are not offered. Publishing remains a separate developer action; this workflow does not publish releases.

## Developer build

Requires CMake 3.22+, C++17, Git and JUCE's platform prerequisites. The build fetches official JUCE 8.0.6 at its exact commit. Use an existing checkout; cloud tasks are already isolated, so do not create worktrees unless requested.

Windows: Visual Studio 2022 with Desktop development with C++, plus the pinned WebView2 SDK:

```powershell
nuget install Microsoft.Web.WebView2 -Version 1.0.2903.40 -OutputDirectory webview2 -NonInteractive
cmake -S . -B build -A x64 -DJUCE_WEBVIEW2_PACKAGE_LOCATION="$PWD/webview2"
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

VST3 output is `build/WUBROT_artefacts/Release/VST3/WUBROT.vst3`.

Linux prerequisites include ALSA, X11/Xext/Xinerama/Xrandr/Xcursor, FreeType/fontconfig and GL development libraries. Web UI is disabled by default on Linux. To validate only the portable audio core without JUCE:

```sh
cmake -S . -B build -DWUBROT_DSP_ONLY=ON
cmake --build build
ctest --test-dir build --output-on-failure
python3 tests/test_factory.py
```

`tests/DspTests.cpp` exercises sample analysis, loop wrapping, MUTATE zero, audible DNA influence, no sample layering, transient/MIDI reset, concurrent profile publication, automation, mono/stereo and multiple sample rates. `tests/ProcessorTests.cpp` tests actual WAV decoding, project recall, failed-load preservation and nested factory content. Windows CI and the cloud development tests additionally load the actual compiled VST3 bundle through Steinberg interfaces and process audio. Browser tests exercise the UI bridge separately; that is not equivalent to testing WebView2 inside every DAW.

## Framework licensing

JUCE has AGPL/commercial licensing terms. Check the appropriate JUCE licence before distributing a closed-source product. The VST3 SDK and platform dependencies retain their own licences. Factory samples must be ones you can include with the product. This project does not supply a JUCE commercial licence, proprietary samples or Windows signing certificates.

## Portable development updates and rollback

Windows development builds are ZIP artifacts, not installers or public releases. Open the repository's **Actions → Build WUBROT Windows**, open a successful run, and download **WUBROT-0.2.RUNNUMBER-dev-Windows-x64** under **Artifacts**. The run number is the development build version. GitHub retains each artifact for 90 days; download older ZIPs locally for longer-term rollback. No public releases are created automatically.

Close FL Studio before updating. Extract the ZIP and replace the entire existing `C:\Program Files\Common Files\VST3\WUBROT.vst3` folder with its new counterpart. Do not rename it or leave versioned copies in your scanned VST3 folders. Use the same process with an older ZIP to roll back. Back up projects before opening them in newer development versions.

The plugin bundle contains the embedded UI, jelly texture and native bridge. Factory manifests and any supplied sample content are copied into Contents/Resources/WubrotContent inside the bundle, which is checked before the legacy ProgramData location. No separate UI folder or installer is needed. Microsoft WebView2 Runtime is still a platform prerequisite for the editor. Factory WAVs are not supplied yet; load your own WAV. The filename, bundle ID, manufacturer code, plugin code, parameter IDs and state format remain stable across development builds.

UI sizes are Small 600×520, Medium 804×697 (default), and Large 996×864 logical pixels. The size selector requests the corresponding native editor size; arbitrary resizing is disabled. The selection persists when reopening the editor and is stored with project state. Windows/FL Studio DPI settings can affect physical pixel dimensions. FL Studio-specific visual validation still requires testing on your machine.

## Preset library

Use the header preset browser to search FACTORY or MY PRESETS. Save Preset, Save, Save As, Rename, Delete, Import, Export and Open Preset Folder are integrated into the floating browser. User files are stored under the actual Windows Documents folder in `Wubware Audio/WUBROT/Presets` and survive VST3 replacements. Portable `.wubpreset` files contain analysis, not the source WAV. See [PRESETS.md](PRESETS.md) for workflow, format, recall behavior and testing limits.

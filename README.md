# WUBROT — WUBWARE AUDIO

WUBROT is a sample-driven bitcrush distortion effect for existing bass audio. It does not generate or layer the loaded sample. Insert it on a mixer/audio track carrying your bass.

## Master power

The existing power button retains its click-free dry bypass. OFF also extinguishes the lighting, turns the reflective materials metallic grey and freezes decorative animation and the visual DNA playhead. Preset browsing, size selection, file loading and controls remain functional. Powering back ON restores the original lighting and resumes visualization from the current settings. This uses the existing automatable enabled state; no audio-processing or parameter-identity changes.

The existing Crush Field movement also responds gently to low-end attacks in WUBROT's own incoming audio. A read-only three-band observer (30–180, 180–700 and 700–2500 Hz) prioritizes low/body energy and adds midrange articulation. It briefly boosts the existing ripple displacement and purple reflections, adding no new waves, controls, audio, or latency. Sustained bass settles to a gentle response capped at one tenth of the reactive boost rather than keeping the material maximized. Silence retains the original animation, and OFF/hidden/static-animation states suppress reactive rendering. It cannot hear other DAW tracks unless they are routed through this instance; there is no external sidechain input.

## Controls

- **Crush Field X:** move right for fewer bits (16 down to 2).
- **Crush Field Y:** move up for more sample-rate reduction (the current host sample rate down to 500 Hz). The circular boundary limits the reachable X/Y combinations.
- **ROT DNA:** drop or browse for WAV/MP3 audio, up to 20 MB. The first 30 seconds are analysed for temporal, spectral and harmonic characteristics. The analysis shapes incoming audio; the source is not played as a layer.
- **MUTATE:** blends the loaded DNA's temporal and harmonic characteristics into input-dependent processing. At 0%, the loaded DNA has no influence.
- **DRIVE:** adds nonlinear distortion before quantisation. Its antiderivative antialiasing reduces unwanted drive-stage aliasing; the sample-rate crusher's aliasing is intentional. At zero, the drive stage blends out.
- **LOOP / RETRIGGER:** LOOP repeats the selected analysis region while the DAW plays and input is active; RETRIGGER restarts it on bass attacks or MIDI note-on. START/END select the region and SPEED changes the modulation rate. The playhead follows the actual DSP profile position and freezes when stopped or silent. With no host transport information, active live input can drive the profile. No source audio is played.
- **SPEED:** drag the small rotary knob up/down, use its mouse wheel or arrow keys, or choose from the custom selector. The five supported values are 0.25×, 0.5×, 1×, 2× and 4×. Shift-drag gives finer stepping; double-click resets to 1×. Both controls follow the existing automation parameter. The single cyan mode switch selects LOOP on the left and RETRIGGER on the right; both labels use the same cyan lighting. Controls remain adjustable, with grey lighting, while the engine is OFF.

Bass transient detection uses a fixed threshold and 50 ms guard time; very quiet or continuously sustained sounds may not retrigger as expected. MIDI offers explicit timing when the DAW routes notes to an audio effect. MIDI routing differs between hosts; incoming notes are not passed through the effect.

Mix (default 100%), Output (default −6 dB), and Keep Sub are available as host automation parameters without adding more panels to the interface. Keep Sub preserves a gently filtered band around 120 Hz. Output is not a limiter; watch your mixer levels.

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

**The three factory WAVs have not been supplied yet.** The directories contain instructions, not substitute audio. Supply each preset's intended WAV at the corresponding `Samples/dna.wav` path. Preset names and settings can be refined when auditioning those samples. Development builds can load your own WAV immediately; factory presets without content report the missing sample instead of pretending to have loaded one.

Installer work is deferred. The current delivery is a portable VST3 ZIP, with bundled factory manifests validated by CI. Check content with:

```sh
python3 tools/validate_factory.py                  # required for installers
python3 tools/validate_factory.py --allow-missing  # development manifests only
```

Factory content is read from the portable VST3 bundle under `Contents/Resources/WubrotContent/Presets/Factory`. User presets are stored separately in the OS-resolved Documents folder, never inside the installed bundle.

For development, set the non-secret `WUBROT_CONTENT_DIR` variable to the **Resources directory**, not to Factory. Installed use requires no variable. Projects save the sound and analysis independently of external preset files. Directly loaded source audio retains the existing project embedding behavior for full source-region editing; portable `.wubpreset` files contain analysis only. Source audio is never played as an extra layer.

## Downloading and replacing development builds

Open this repository's **Actions → Build WUBROT Windows**. Select a successful run and download **WUBROT-0.2.RUNNUMBER-dev-Windows-x64** from **Artifacts** at the bottom of the run page. GitHub downloads it as a ZIP. Each run has a unique version number; previous ZIPs remain available for 90 days. Download local backups for longer-term rollback. No installer or public release is created automatically.

1. Close FL Studio and any other DAW using WUBROT.
2. Extract the ZIP. Remove/replace the old outer `C:\Program Files\Common Files\VST3\WUBROT.vst3` folder with the new outer **WUBROT.vst3 folder**, including all its contents.
3. Reopen FL Studio. Do not rename the installed folder or leave versioned copies in scanned plugin directories. If WUBROT is not listed, use **Options → Manage plugins → Find installed plugins**.

To roll back, repeat these steps using an older downloaded ZIP. Back up your projects before testing development updates. Internal plugin identity, parameter IDs and the existing project-state format remain stable.

The UI and jelly artwork are embedded in the plugin; no separate HTML/UI installation is needed. Factory content is bundled under Contents/Resources/WubrotContent. The Windows WebView2 Runtime remains a system prerequisite for the editor (already present on most current Windows machines). The three factory WAVs have not been supplied yet; load your own WAV into ROT DNA for now. Installer work is deferred.

## Windows DAW support

The current release target is **Windows x64 VST3**: FL Studio, Ableton Live, Cubase and other Windows VST3 hosts. The Windows CI build and audio/state tests passed. A separate real-VST3 loading/processing smoke test is included. Real listening and interaction tests in FL Studio and the other DAWs are still required; this is an early development build.

macOS, Audio Units, Logic Pro and Linux releases are deferred. There is no current macOS installer or AU build workflow. Source code is portable for local testing, but only Windows is being packaged now.

Windows development builds are unsigned. Signing remains a release task.

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

UI sizes are Small 600×500, Medium 804×670 (default), and Large 996×830 logical pixels. The size selector requests the corresponding native editor size; arbitrary resizing is disabled. The selection persists when reopening the editor and is stored with project state. Windows/FL Studio DPI settings can affect physical pixel dimensions. FL Studio-specific visual validation still requires testing on your machine.

## Preset library

Use the header preset browser to search FACTORY or MY PRESETS. Save Preset, Save, Save As, Rename, Delete, Import, Export and Open Preset Folder are integrated into the floating browser. User files are stored under the actual Windows Documents folder in `Wubware Audio/WUBROT/Presets` and survive VST3 replacements. Portable `.wubpreset` files contain analysis, not the source WAV. See [PRESETS.md](PRESETS.md) for workflow, format, recall behavior and testing limits.

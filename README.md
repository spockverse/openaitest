# WUBROT — Spock Audio

WUBROT is a sample-driven bitcrush distortion effect for existing bass audio. It does not generate or layer the loaded sample. Insert it on a mixer/audio track carrying your bass.

## Controls

- **Crush Field X:** move right for fewer bits (16 down to 2).
- **Crush Field Y:** move up for more sample-rate reduction (the current host sample rate down to 500 Hz). The circular boundary limits the reachable X/Y combinations.
- **ROT DNA:** drop or browse for a WAV, up to 20 MB. The first 30 seconds are analysed into a normalised envelope. This is a volume/transient pattern, not automatic instrument identification or spectral/formant transfer.
- **MUTATE:** controls the envelope's influence. At 100%, envelope peaks can subtract up to six bits and reduce the effective rate by up to three octaves, within the crusher's limits. At 0%, DNA has no effect.
- **DRIVE:** adds nonlinear distortion before quantisation. Its antiderivative antialiasing reduces unwanted drive-stage aliasing; the sample-rate crusher's aliasing is intentional. At zero, the drive stage blends out.
- **LOOP / RETRIGGER:** the compact toggle in ROT DNA selects continuous envelope looping or a one-shot pattern restarted by a bass transient or incoming MIDI note-on. LOOP is the default. Playback uses the analysed sample's original duration; it is not automatically tempo-stretched. LOOP aligns to the DAW timeline while transport plays and runs freely for live input without a running transport.

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

Release installer packaging refuses missing, malformed, silent, or out-of-folder sample references. CI separately compile-checks the installer against temporary synthetic test fixtures outside the checkout; that test installer and its audio are never uploaded or shipped. Check content with:

```sh
python3 tools/validate_factory.py                  # required for installers
python3 tools/validate_factory.py --allow-missing  # development manifests only
```

On Windows the installer puts the VST3 bundle into `C:\Program Files\Common Files\VST3\WUBROT.vst3` and factory content under `C:\ProgramData\Spock Audio\WUBROT\Presets\Factory`. Each preset's sample remains inside its own folder.

For development, set the non-secret `WUBROT_CONTENT_DIR` variable to the **Resources directory**, not to Factory. Installed use requires no variable. Projects embed the analysed envelope, its duration and display name, so the audible DNA survives moving/deleting the source WAV or moving the project to another Windows computer. The original audio file is not embedded or played.

## Getting a Windows build without compiling yourself

The project files must be committed and pushed to GitHub first. Environment publication does not push them.

1. On the repository's **Actions** tab, run **Build WUBROT Windows**.
2. A successful run provides **WUBROT-Windows-VST3**. Extract it and copy the entire `WUBROT.vst3` bundle into the VST3 directory above. It is an effect, not an instrument.
3. In FL Studio, use **Options → Manage plugins → Find installed plugins**, then add WUBROT to a mixer effect slot carrying your bass.
4. To create `WUBROT-Setup-0.2.0.exe`, provide the three WAVs, run the workflow with **package_installer** enabled, and download **WUBROT-Windows-Installer**.

The editor requires the Microsoft Edge WebView2 Evergreen Runtime, normally present on current Windows systems. If absent, install it from Microsoft's WebView2 page and reopen the editor. The installer offers a link; it does not bundle or silently download this Microsoft runtime. No proprietary factory WAVs or signing credentials are included in this source checkout.

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

UI sizes are Small 600×500, Medium 800×666 (default), and Large 1000×833 logical pixels. The size selector requests the corresponding native editor size; arbitrary resizing is disabled. The selection persists when reopening the editor and is stored with project state. Windows/FL Studio DPI settings can affect physical pixel dimensions. FL Studio-specific visual validation still requires testing on your machine.

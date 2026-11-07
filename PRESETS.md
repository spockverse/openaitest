# WUBROT presets

Click the preset name in the header to open the floating browser. FACTORY contains the three bundled factory settings; MY PRESETS contains your Documents library. Search is case-insensitive. Use Up/Down and Enter to select, Escape or a click outside to close. The browser has a short fade/expansion and respects reduced motion.

SAVE PRESET and Save As create named user presets. SAVE updates the selected user preset. The ••• menu provides Save As, Rename, Delete (with confirmation), Import, Export and Open Preset Folder. Factory presets cannot be renamed, deleted or overwritten. You can save a modified factory sound as a separate user preset, including with the same display name in MY PRESETS.

An asterisk means that sound parameters or DNA differ from the last loaded/saved state. Opening or resizing the editor, moving the playhead, and library-only actions do not change the sound. Before switching sounds, choose Save, Discard or Cancel. A processor-side guard also catches edits that arrive between UI updates. Host-requested VST program changes remain host-controlled and do not open a modal prompt.

## Storage and safe writes

The native JUCE `File::getSpecialLocation(File::userDocumentsDirectory)` lookup resolves the current user's Documents folder. In the pinned JUCE Windows implementation this uses Windows' redirected `CSIDL_PERSONAL` folder lookup; it is not constructed from a drive letter or username. Presets live under:

`<actual Documents>/Wubware Audio/WUBROT/Presets/*.wubpreset`

The folder is created when the library is first used. Unavailable or read-only storage is reported; no alternate directory is silently used. Existing readable presets can still be listed in a read-only directory. User files are outside the VST3 bundle, so replacing WUBROT.vst3 does not replace the library. Open Preset Folder reveals this actual directory.

Names are limited to 80 characters. Windows reserved device names, control characters, path separators, leading/trailing spaces and trailing dots are rejected. Conflicts are case-insensitive. Imports are validated before writing. Factory files are not managed through the user library API.

Filesystem operations and archive creation run on a non-audio worker. Writes use a unique temporary file next to the destination, flush it, reread/validate its contents, then commit it. Existing Windows files use JUCE's `ReplaceFile` path; new Windows files use `MoveFileExW` without the replace flag to prevent a late collision from overwriting another file. An interprocess lock serializes writes to the same destination across instances. Errors preserve the prior preset and temporary files are cleaned up. Rename commits the destination before removing the source; if removing the source fails, the error explicitly reports that a preserved copy remains.

Export writes the stored user preset. Save current edits first if you want those edits in an export. The Windows picker handles destination selection and overwrite confirmation; adding a missing extension cannot silently overwrite a different existing file.

## Portable format, version 1

`.wubpreset` is UTF-8 XML, capped at 2 MiB:

```xml
<WubrotPreset formatVersion="4" plugin="WUBROT" name="Metallic Yoi">
  <WubrotState ...analysis and metadata properties...>
    <PARAM id="x" value="0.3"/>
    <!-- all 12 existing sound parameters -->
  </WubrotState>
</WubrotPreset>
```

The inner state uses the same APVTS `WubrotState` structure as DAW projects. Required parameters retain their existing IDs and order: `x`, `y`, `mutate`, `drive`, `mode`, `mix`, `output`, `protect`, `enabled`, `regionStart`, `regionEnd`, `dnaSpeed`. All are validated against their ranges; discrete values must be integral. The optional `xyMapping` property selects the sound model: missing/1 retains the original mapping and processing exactly; 2 retains the v0.2.37 centred sound; 3 retains the v0.2.41 signed regions and temporal spectral DNA; 4 corrects excitation, folding and resonance balance without changing coordinates or analysis data. New sounds use outer `formatVersion="4"`, so older plugins reject them rather than silently interpreting their XY differently. This build reads versions 1, 2, 3 and 4. Saving a legacy sound retains its original model/container; renaming/exporting preserves its model. Mapping version participates in the dirty-state signature without adding an automation parameter. Informational `crushBitDepth`, `crushSampleRate` and `referenceSampleRate` properties document the reduction at the saving host's sample rate; they do not add parameters or change the processing model.

The versioned outer container rejects unsupported future versions rather than guessing. XML document types/entities, missing/duplicate parameters, invalid values and corrupt analysis arrays are rejected before applying a state. There is no code, external file dependency, or original audio in the portable format. The `sourceWav` property is explicitly forbidden.

### ROT DNA analysis

`presetProfile` is a JSON property containing the **exact active** DnaProfile: validity, duration, 2,048 envelope points, 2,048 temporal brightness/character points and a spectral descriptor. This retains the saved selected-region sound exactly. A spectral descriptor is an array with validity followed by 12 band levels, five character features and three resonance frequencies; peak levels are derived by the existing algorithm.

`dnaArchive` is a version-1 JSON object containing the full-sample profile plus 64 time slices of spectral descriptors. It contains **analysis descriptors only**, never PCM, a wavetable, or a buffer played as audio. The original audio filename is informational and need not exist. Temporal-analysis archives use version 2 and include the full 128-frame spectral timeline; the active profile also retains its exact selected-region timeline. Legacy version-1 archives remain supported. Each frame contains 26 bounded numeric descriptors, never PCM. New-model presets missing required temporal descriptors are rejected. Typical files remain well below the 2 MiB cap.

Restored presets show **DNA ENVELOPE (ANALYSIS)** and disclose that no original audio is included. START/END, SPEED and LOOP/RETRIGGER remain usable. Editing a new region interpolates/crops the stored temporal analysis and combines overlapping spectral slices; it does not re-run an FFT on an unavailable WAV. Consequently, a newly selected region is an analysis-based approximation of reanalysing that exact region of the source audio. Returning to the originally saved START/END restores its exact active profile. Loading the original WAV enables full audio-based analysis again. Legacy projects with only an active snapshot can still play, but saving them as fully editable portable presets requires loading the source once to create the full archive.

DAW projects remain self-contained. They save the selected sound, analysis, name and dirty baseline independently of library files. Directly loaded audio retains the existing project-source embedding behavior for exact source editing; portable preset files never embed that audio. A project restored from an analysis-only preset retains the analysis archive without requiring a `.wubpreset` file. Renaming/deleting a library preset does not change existing project audio. No global last-used sound is applied to new/restored instances.

## Validation and remaining manual checks

`WubrotPresets` tests Documents resolution/injected relocated roots, directory creation/failure, safe names and traversal, save/load/Save As, case-insensitive conflicts, rename/delete, import/export, malformed/version/range rejection, compact analysis-only exact audio recall, editable regions/speed/retrigger, stable automation IDs, dirty baselines, independent instances and project recall after deleting the preset file. Existing DSP, processor, content and actual-VST3 tests also run.

`tests/PresetBrowser.cjs` tests browser geometry, animations, search, keyboard and outside closing, save/conflict/unsaved workflows, reduced motion and bounded DOM rendering of 10,000 entries at Small/Medium/Large using a mocked native bridge. The other browser suites verify existing XY, waveform, bypass, size and loader behavior. Browser/mock tests do not prove Windows Shell dialogs or FL Studio behavior.

Windows Actions builds and tests the native VST3 and runs the browser suites with pinned Playwright/Chromium tooling. The UI-Checks artifact contains browser-only screenshots, not FL Studio screenshots. Actual FL Studio interaction, native file pickers, Explorer reveal, redirected/OneDrive Documents with Windows ACLs, cross-process simultaneous writes and manual listening still need user-side Windows verification. The bundled factory manifests are preserved; their factory WAV files have not been supplied in this repository, and missing sample content is reported in ROT DNA. User-provided reference recordings are never included in presets, source, factory content or build artifacts.

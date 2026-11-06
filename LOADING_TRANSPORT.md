# File loading and profile-clock compatibility update

No branding, layout, transfer curves, filters, quantizer, resonance or preset/parameter
identity changes. The attached branding brief is separate from this request.

## Import routes

Previously only the web DataTransfer FileList was used, and .wav was required.
The JUCE editor and embedded Browser component now implement FileDragAndDropTarget,
forwarding native path drops to the same worker decoder. Browser handling accepts
FileList, file items, filesystem entries and local file URI/path text. Missing or
unreadable payloads show a diagnostic instead of silently doing nothing. Temporary
host filenames are decoded by header, not rejected just for lacking .wav.

The existing upload affordance is now a real button invoking a native asynchronous
Windows file picker. WAV and MP3 are advertised. JUCE's bundled MP3 decoder is enabled;
AudioFormatManager performs format detection. Source bytes and region edit/recall
continue through the existing pipeline, including for MP3. Errors distinguish
inaccessible paths, unsupported formats, decoding/read failures and oversized/invalid
transfers. Previous valid DNA remains available after failed imports.

Native callbacks do not prove Windows Shell OLE delivery through FL Studio/WebView2.
Host virtual drags without files are not a public FL-specific audio-buffer API.
Windows privilege boundaries can also prevent external drags reaching an application;
this update does not bypass OS restrictions. Use the native picker when a source
cannot supply a readable file. Actual Explorer/FL Studio drag delivery must still be
checked by a Windows tester. No universal drag-source support is claimed.

## Modulation clock

The shaping engine is unchanged. Only profile progression follows host play state
and live input. LOOP pauses in stop/silence and resumes without consuming idle time.
RETRIGGER still uses sample-positioned MIDI notes and input attacks. Finished
one-shots hold their cursor at END. Seeks/host loops relocate modulation, and repeated
stale host positions cannot reset it every block. Seconds are used if sample positions
are unavailable. Speed remains time-based, not a new tempo-synced feature. Tempo
changes follow the host timeline and incoming note articulation.

Without host transport information, live input drives progression. A 2ms activity
hold bridges individual zero crossings; entirely silent blocks freeze immediately.
The actual profile position is emitted in small messages at up to 60Hz, while the
existing full UI snapshot stays 20Hz. No JavaScript timer or CSS transition advances
the cursor. Position changes only on processor snapshots.

## Verification

- Synthetic WAV16/24/float32 at 44.1/48/88.2/96/192kHz through native callback and
  browser-byte decoder routes; same file gives identical analysis/audio.
- Synthetic MP3 fixture (generated test signal, not a user recording), decode and
  embedded-source reopening/region editing; missing-file diagnostics.
- Browser FileList/item/URI payloads, picker request, missing-payload errors and
  stationary UI without DSP updates. These are not native Explorer OS tests.
- Processor mock host stop with nonzero input, silent playback, resume, seeking,
  stale timestamps, plus core gating and existing LOOP/RETRIGGER checks.
- Existing editor interactions, geometry, focus and five audio/processor/VST3 suites.

The source recordings supplied for sonic analysis remain excluded from all artifacts.
Only the explicitly generated synthetic MP3 decoder fixture is added to tests.

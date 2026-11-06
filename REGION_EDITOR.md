# ROT DNA mini-editor and cleaner crusher

The UI follows the supplied panel reference: cyan START/END triangles, purple
selection, dimmed outer waveform, red position line/cyan pointer, compact purple
speed controls and cyan loop switch, plus the existing LOOP/RETRIGGER buttons.
The written behavior takes precedence over conflicting mockup status text:
selection and speed actually affect modulation and selected-region analysis.
Main XY/knobs, housing, power, sizes and footer remain unchanged.

The exact selected WAV samples are analysed off-thread. Envelope, spectrum,
harmonic balance, resonances and dynamics are recalculated. Changes to the active
spectral descriptors smooth over milliseconds. Silent selections disable DNA.
Marker drags coalesce into one worker analysis at a time; no FFT, allocation or
file access occurs in audio processing. The complete waveform stays visible.

Three new automatable parameters are appended: regionStart, regionEnd, dnaSpeed.
All previous IDs/order and plugin identity stay intact. Speed is 0.25/0.5/1/2/4x,
applied to modulation time only. Speed changes progress continuously rather than
multiplying elapsed host time. LOOP wraps the selected profile; RETRIGGER restarts
on bass transients/MIDI and stops the one-shot at selected duration / speed.
The incoming bass isn't resampled or transposed by SPEED. No donor audio is played.

New project state embeds the user-loaded WAV (20MB cap; first 30s editable) so the
selection remains editable offline. This increases project size, up to roughly
28MB serialized state. Active profile, actual analyzed bounds, requested bounds,
speed and full waveform also save. Existing analysis-only projects still load
and play; reload the original WAV to enable editing. No user reference recordings
are included in source/resources, factory content, builds or audition artifacts.
All distributed fixtures and renders are generated synthetic audio.

Audio quality: input anti-alias filtering, fractional sample capture and DAC-like
reconstruction replace raw hold edges. The controlled 4bit/1.6kHz test shows about
29dB lower clock-image/fundamental ratio, with four-bit DC levels still preserved.
DNA no longer sweeps the crusher clock/bit count. Extra 4x-filtered nonlinear DRIVE
follows reconstruction, generating input-derived harmonics. DRIVE0 isolates the
bare quantizer; extreme bit/rate settings remain deliberately lo-fi. A separate
envelope output gain has been removed. Silence, tails and the dry power bypass
remain tested. No free-running audible oscillators or unrelated audio layers.

The supplied dry/wet pair was analysed only: 5.28s versus 7.42s, with imperfect
alignment and an unavailable exact loaded donor WAV. The screenshot implies about
3.8bits/1.6kHz before the old donor offsets. Stage tests isolate a real clock-image
defect but don't prove a null-exact reproduction or subjective fix of that capture.

Validation includes selected-region spectra/output, MUTATE0, retained source and
editing after project reopening, five speeds, loop/retrigger/end timing, spectrum
contrasts, pitch/rhythm, silence/gaps, dry bypass and VST3 binary smoke. Browser
checks cover mouse/keyboard markers, no unintended file dialog, speed/mode events,
playhead, power, popup, footer and XY at three sizes and 100–200% simulated density.
Actual Windows/FL Studio listening and wrapper checks remain user validation.

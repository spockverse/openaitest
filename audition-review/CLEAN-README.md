# Crisp digital component check — not a plugin build

Four short WAVs, each using the same three synthetic inputs in order:
sine sub, low-pass bass, saw bass. Each input lasts two seconds, with 250 ms gaps.
Every segment is RMS matched to 0.12. Source recording is synthetic, not the user video.

0-dry: unchanged source.
1-8bit-only: uniform 8-bit quantization, full native sample rate.
2-hold-only: 3 kHz zero-order sample/hold, no bit-depth reduction.
3-8bit-and-hold: both operations together.

No saturation, wavefolding, companding, resonant filters, dual-clock mixing, DNA,
noise generator, or high-frequency reconstruction low-pass is used. A 140 Hz
low-pass of the processing error is subtracted to preserve the original low-end;
this is not a bass EQ boost. These isolate components for listening, rather than
presenting another supposedly finished four-quadrant engine. Silence and finite
bounds passed. No production plugin code, state or UI changed; no VST3 build.

Please compare against dry: distinguish quantization grain from stepped hold
texture. Reject these too if they still miss the reference. No sonic-quality
approval or real-time integration is claimed.

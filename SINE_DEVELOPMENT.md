# Frequency-selective sine development build

New instances use processing version 5. Existing projects and presets retain
versions 1–4, including sample-exact v0.2.42 recall. All parameter IDs, editor
assets, animation, DNA analysis/handoff and VST3 identity remain unchanged.
No user reference recordings or derivatives are included.

The audition's complementary 240 Hz bass foundation and sine carrier are combined
with the existing bounded-control-rate quantizer/hold stage. Signed XY continuously
weights modulation depth, carrier frequency, carrier stepping and reconstruction.
The established axis mapping and dry-center gating are retained. DRIVE excites the
selected band with a bounded saturation curve, compensated separately from output
gain. Keep Sub continues controlling the protected complementary contribution.
DNA retains its time-varying spectral-bank coloration; generic no-DNA moving
resonances/wavefolding are omitted from the new core. DNA controls/analysis are
unchanged. This is a development sound choice, not a claim of Corrosion equivalence.

Oscillator/hold state persists across blocks. Fixed-size working storage is
prepared in advance; no lookahead, additional reported latency, UI clock or
sample playback is introduced. Coefficients update at the existing control rate;
oscillator rotations reanchor every 64 samples. Existing real-time tests exercise
actual processor allocations/reclamations, replacement and rapid automation.

SineEngineTests covers center, silence at four sample rates, DRIVE response,
DNA zero/full connectivity, XY at DRIVE zero and exact model-4 legacy comparison.
Instance/state/preset tests run against the new default. Windows Actions also
hosts the actual VST3, tests the WebView bridge and records paced processBlock
mean/p95/p99/max and memory for models 4/5 with editor open/closed. These checks
are engineering validation, not FL Studio/ASIO or subjective sound approval.

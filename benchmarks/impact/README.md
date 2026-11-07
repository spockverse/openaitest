# Model 3 / model 4 native callback comparison

Release -O3, stereo synthetic bass with the same synthetic DNA. Both modes run through the actual JUCE processor in the same optimized executable; the model-3 DSP is checked sample-exact against frozen v0.2.41 code. Existing original v0.2.41/earlier data is retained separately. These paced callback measurements are not offline throughput, an ASIO capture, or a worst-case guarantee. Shared-runner scheduling outliers are included. Editor-closed Linux; native Windows open/closed reports are produced by Actions.

## v041-closed

GenuineIntel INTEL(R) XEON(R) PLATINUM 8573C; 5 logical CPUs; 34087 MiB RAM; Linux. Reported latency 0 samples.

| Rate | Buffer | Mean us | p95 us | p99 us | Maximum us | Parent peak RSS MiB |
|---|---|---|---|---|---|---|
| 44100 | 64 | 28.53 | 40.05 | 94.46 | 101.92 | 6.93 |
| 44100 | 128 | 53.99 | 77.18 | 115.63 | 129.53 | 6.93 |
| 44100 | 512 | 221.99 | 284.97 | 464.32 | 562.78 | 6.93 |
| 48000 | 64 | 27.31 | 28.71 | 49.72 | 199.98 | 6.93 |
| 48000 | 128 | 63.76 | 120.08 | 275.71 | 557.71 | 6.93 |
| 48000 | 512 | 221.18 | 293.62 | 334.23 | 516.51 | 6.93 |
| 96000 | 64 | 34.17 | 59.03 | 96.84 | 125.04 | 6.93 |
| 96000 | 128 | 54.45 | 57.50 | 178.22 | 472.04 | 6.93 |
| 96000 | 512 | 228.43 | 337.49 | 562.02 | 783.46 | 6.93 |

## revised final

GenuineIntel INTEL(R) XEON(R) PLATINUM 8573C; 5 logical CPUs; 34087 MiB RAM; Linux. Reported latency 0 samples.

| Rate | Buffer | Mean us | p95 us | p99 us | Maximum us | Parent peak RSS MiB |
|---|---|---|---|---|---|---|
| 44100 | 64 | 28.78 | 42.10 | 78.15 | 82.38 | 6.92 |
| 44100 | 128 | 63.52 | 102.34 | 169.37 | 890.16 | 6.92 |
| 44100 | 512 | 243.05 | 331.87 | 1129.86 | 1247.90 | 6.92 |
| 48000 | 64 | 27.75 | 28.36 | 80.05 | 82.59 | 6.92 |
| 48000 | 128 | 55.10 | 58.02 | 155.71 | 157.79 | 6.92 |
| 48000 | 512 | 233.79 | 341.14 | 566.69 | 706.62 | 6.92 |
| 96000 | 64 | 30.28 | 34.78 | 90.53 | 136.93 | 6.92 |
| 96000 | 128 | 58.00 | 82.02 | 151.74 | 374.88 | 6.92 |
| 96000 | 512 | 241.17 | 314.18 | 917.65 | 1666.26 | 6.92 |

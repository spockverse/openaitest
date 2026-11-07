# Local paced callback comparison

Optimized Release, actual native JUCE processor, stereo synthetic bass with DNA; editor closed. Same Xeon Platinum 8573C shared Linux environment, 34,087 MiB RAM. Each case measures 160 callbacks after 20 warmup callbacks. Input generation, deadline pacing, serialization and audition rendering are outside the callback timer. These are not FL Studio/ASIO measurements or a worst-case guarantee. Scheduling outliers remain visible. Windows CI measures editor-open/closed cases separately.

| Version | Rate | Buffer | Mean µs | p95 µs | p99 µs | Observed max µs | Deadline µs | Parent peak RSS MiB |
|---|---|---|---|---|---|---|---|---|
| v037-closed | 44100 | 64 | 32.64 | 52.01 | 118.40 | 138.00 | 1451.25 | 6.67 |
| v037-closed | 44100 | 128 | 73.79 | 123.01 | 239.47 | 1919.03 | 2902.49 | 6.67 |
| v037-closed | 44100 | 512 | 251.76 | 366.64 | 465.13 | 805.19 | 11610.00 | 6.67 |
| v037-closed | 48000 | 64 | 28.22 | 29.48 | 40.40 | 41.14 | 1333.33 | 6.67 |
| v037-closed | 48000 | 128 | 70.16 | 90.55 | 296.29 | 349.18 | 2666.67 | 6.67 |
| v037-closed | 48000 | 512 | 238.72 | 308.33 | 418.07 | 521.99 | 10666.70 | 6.67 |
| v037-closed | 96000 | 64 | 31.21 | 41.39 | 149.00 | 187.98 | 666.67 | 6.67 |
| v037-closed | 96000 | 128 | 58.44 | 70.32 | 149.29 | 217.60 | 1333.33 | 6.67 |
| v037-closed | 96000 | 512 | 290.57 | 470.95 | 911.67 | 1215.90 | 5333.33 | 6.67 |
| v3-closed | 44100 | 64 | 27.65 | 40.74 | 103.97 | 113.60 | 1451.25 | 6.80 |
| v3-closed | 44100 | 128 | 60.15 | 72.78 | 155.01 | 950.30 | 2902.49 | 6.80 |
| v3-closed | 44100 | 512 | 229.67 | 284.65 | 430.37 | 2294.14 | 11610.00 | 6.80 |
| v3-closed | 48000 | 64 | 26.77 | 30.51 | 58.61 | 77.64 | 1333.33 | 6.80 |
| v3-closed | 48000 | 128 | 81.23 | 175.31 | 324.51 | 595.52 | 2666.67 | 6.80 |
| v3-closed | 48000 | 512 | 224.69 | 315.56 | 571.01 | 679.98 | 10666.70 | 6.80 |
| v3-closed | 96000 | 64 | 26.55 | 29.93 | 40.49 | 153.65 | 666.67 | 6.80 |
| v3-closed | 96000 | 128 | 65.91 | 134.45 | 190.90 | 191.74 | 1333.33 | 6.80 |
| v3-closed | 96000 | 512 | 203.22 | 261.98 | 333.53 | 410.10 | 5333.33 | 6.80 |

The original before-edit measurement is also retained. Raw callback reports accompany this table; offline render speed is not used as a processing-cost proxy. No user recordings are stored here.

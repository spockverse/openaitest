# Local paced callback comparison

Optimized Release, actual native JUCE processor, stereo synthetic bass with DNA; editor closed. Same Xeon Platinum 8573C shared Linux environment, 34,087 MiB RAM. Each case measures 160 callbacks after 20 warmup callbacks. Input generation, deadline pacing, serialization and audition rendering are outside the callback timer. These are not FL Studio/ASIO measurements or a worst-case guarantee. Scheduling outliers remain visible. Windows CI measures editor-open/closed cases separately.

| Version | Rate | Buffer | Mean µs | p95 µs | p99 µs | Observed max µs | Deadline µs | Parent peak RSS MiB |
|---|---|---|---|---|---|---|---|---|
| v037-closed | 44100 | 64 | 34.84 | 66.74 | 160.29 | 166.00 | 1451.25 | 6.74 |
| v037-closed | 44100 | 128 | 60.92 | 99.94 | 160.82 | 168.72 | 2902.49 | 6.74 |
| v037-closed | 44100 | 512 | 252.39 | 374.12 | 568.27 | 584.66 | 11610.00 | 6.74 |
| v037-closed | 48000 | 64 | 38.69 | 101.80 | 239.62 | 266.27 | 1333.33 | 6.74 |
| v037-closed | 48000 | 128 | 58.36 | 75.12 | 137.07 | 167.76 | 2666.67 | 6.74 |
| v037-closed | 48000 | 512 | 246.16 | 342.01 | 393.21 | 466.30 | 10666.70 | 6.74 |
| v037-closed | 96000 | 64 | 38.60 | 108.55 | 188.27 | 203.65 | 666.67 | 6.74 |
| v037-closed | 96000 | 128 | 61.94 | 113.22 | 185.54 | 349.68 | 1333.33 | 6.74 |
| v037-closed | 96000 | 512 | 254.70 | 406.31 | 625.44 | 991.60 | 5333.33 | 6.74 |
| v3-closed | 44100 | 64 | 41.21 | 89.04 | 362.61 | 494.32 | 1451.25 | 6.87 |
| v3-closed | 44100 | 128 | 59.47 | 80.09 | 287.82 | 478.20 | 2902.49 | 6.87 |
| v3-closed | 44100 | 512 | 223.89 | 307.55 | 528.12 | 541.23 | 11610.00 | 6.87 |
| v3-closed | 48000 | 64 | 31.39 | 38.18 | 356.71 | 427.60 | 1333.33 | 6.87 |
| v3-closed | 48000 | 128 | 63.89 | 122.43 | 173.99 | 225.83 | 2666.67 | 6.87 |
| v3-closed | 48000 | 512 | 312.89 | 636.23 | 1773.05 | 3413.20 | 10666.70 | 6.87 |
| v3-closed | 96000 | 64 | 37.84 | 118.83 | 199.63 | 567.96 | 666.67 | 6.87 |
| v3-closed | 96000 | 128 | 54.90 | 68.45 | 222.20 | 236.67 | 1333.33 | 6.87 |
| v3-closed | 96000 | 512 | 238.97 | 405.28 | 1327.85 | 1770.64 | 5333.33 | 6.87 |

The original before-edit measurement is also retained. Raw callback reports accompany this table; offline render speed is not used as a processing-cost proxy. No user recordings are stored here.

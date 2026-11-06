"""Optional offline spectral report: pip install numpy scipy; pass render directory."""
import json
import sys
from pathlib import Path
import numpy as np
from scipy.io import wavfile
from scipy.signal import stft

root = Path(sys.argv[1])
report = {}
for path in sorted(root.glob('0*.wav')):
    sr, x = wavfile.read(path)
    x = x.astype(float) / 32768
    f, t, z = stft(x, sr, nperseg=4096, noverlap=3072)
    power = abs(z) ** 2
    spectrum = power.mean(axis=1)
    spectrum /= max(spectrum.sum(), 1e-20)
    centroid = (f[:, None] * power).sum(axis=0) / (power.sum(axis=0) + 1e-20)
    report[path.name] = {
        'rms_dBFS': float(20 * np.log10(np.sqrt(np.mean(x*x)) + 1e-20)),
        'peak_dBFS': float(20 * np.log10(abs(x).max() + 1e-20)),
        'centroid_Hz': float((f * spectrum).sum()),
        'centroid_motion_std_Hz': float(np.std(centroid[2:-2])),
        'energy_above_200Hz_fraction': float(spectrum[f > 200].sum()),
    }
print(json.dumps(report, indent=2))

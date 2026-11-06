#!/usr/bin/env python3
"""Make temporary test-only content to compile-check the Windows installer, never shipped."""
import json
import math
import shutil
import struct
import sys
import wave
from pathlib import Path

project = Path(__file__).resolve().parents[1]
destination = Path(sys.argv[1]).resolve()
if destination.is_relative_to(project) or destination.exists():
    raise SystemExit('Fixture destination must be new and outside the project checkout.')
destination.mkdir(parents=True)
shutil.copyfile(project / 'README.md', destination / 'README.md')
for source in (project / 'Resources/Presets/Factory').iterdir():
    if not source.is_dir():
        continue
    target = destination / 'Resources/Presets/Factory' / source.name
    (target / 'Samples').mkdir(parents=True)
    manifest = json.loads((source / 'preset.json').read_text())
    (target / 'preset.json').write_text(json.dumps(manifest))
    wav = target / manifest['sample']
    with wave.open(str(wav), 'wb') as stream:
        stream.setparams((1, 2, 48000, 0, 'NONE', 'not compressed'))
        stream.writeframes(b''.join(struct.pack('<h', int(10000 * math.sin(i * .1))) for i in range(4800)))
print('Test-only installer fixture created. Do not distribute this content or its installer.')

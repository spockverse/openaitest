#!/usr/bin/env python3
"""Validate the exact three factory preset/sample pairs before packaging."""
import argparse
import json
import math
import struct
from pathlib import Path

FOLDERS = ('01 Riddim Monster', '02 Liquid Glass', '03 Metal Teeth')
RANGES = {'x': (-1, 1), 'y': (-1, 1), 'mutate': (0, 100), 'drive': (0, 36),
          'mode': (0, 1), 'mix': (0, 100), 'output': (-24, 6), 'protect': (0, 1)}


def validate_wav(path):
    size = path.stat().st_size
    if size > 20 * 1024 * 1024:
        raise ValueError('WAV exceeds 20 MB')
    data = path.read_bytes()
    if len(data) < 44 or data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        raise ValueError('not a RIFF WAV')
    if struct.unpack_from('<I', data, 4)[0] + 8 > len(data):
        raise ValueError('truncated RIFF container')
    offset, fmt, audio_bytes, payloads = 12, None, 0, []
    while offset + 8 <= len(data):
        tag, length = struct.unpack_from('<4sI', data, offset)
        start = offset + 8
        if start + length > len(data):
            raise ValueError('truncated WAV chunk')
        if tag == b'fmt ':
            if length < 16:
                raise ValueError('invalid format chunk')
            code, channels, rate, _, align, bits = struct.unpack_from('<HHIIHH', data, start)
            if code == 0xfffe:
                if length < 40:
                    raise ValueError('invalid extensible format')
                code = struct.unpack_from('<H', data, start + 24)[0]
            fmt = code, channels, rate, align, bits
        elif tag == b'data':
            audio_bytes += length
            payloads.append(data[start:start + length])
        offset = start + length + length % 2
    if fmt is None:
        raise ValueError('missing format chunk')
    code, channels, rate, align, bits = fmt
    if (code not in (1, 3) or not 1 <= channels <= 8 or not 8000 <= rate <= 384000
            or bits not in (16, 24, 32) or (code == 3 and bits != 32)
            or align != channels * bits // 8 or audio_bytes < align or audio_bytes % align):
        raise ValueError('unsupported or invalid PCM/float WAV format')
    # Reject silent factory fixtures; the real processor performs its own envelope validation too.
    audible = False
    width = bits // 8
    remaining = rate * 30 * align
    for payload in payloads:
        payload = payload[:remaining]
        remaining -= len(payload)
        for i in range(0, len(payload) - width + 1, width):
            if code == 3:
                value = struct.unpack_from('<f', payload, i)[0]
                if math.isfinite(value) and abs(value) > 1e-7:
                    audible = True
                    break
            elif abs(int.from_bytes(payload[i:i + width], 'little', signed=True)) / (2 ** (bits - 1)) > 1e-7:
                audible = True
                break
        if audible or remaining <= 0:
            break
    if not audible:
        raise ValueError('sample has no audible signal in its first 30 seconds')
    return audio_bytes / align / rate


def validate(root, allow_missing=False):
    errors, missing = [], []
    for folder in FOLDERS:
        preset_dir = root / 'Presets' / 'Factory' / folder
        try:
            manifest = json.loads((preset_dir / 'preset.json').read_text())
            if manifest.get('version') != 1 or not manifest.get('name'):
                raise ValueError('invalid preset name/version')
            params = manifest['parameters']
            for key, (lo, hi) in RANGES.items():
                value = params[key]
                if not isinstance(value, (int, float)) or not math.isfinite(value) or not lo <= value <= hi:
                    raise ValueError(f'invalid {key}')
            if math.hypot(params['x'], params['y']) > 1.00001:
                raise ValueError('preset XY is outside circular field')
            if params['mode'] not in (0, 1):
                raise ValueError('mode must be LOOP (0) or RETRIGGER (1)')
            relative = Path(manifest['sample'])
            sample = (preset_dir / relative).resolve()
            if relative.is_absolute() or not sample.is_relative_to(preset_dir.resolve()):
                raise ValueError('sample must stay inside its own preset folder')
            if not sample.is_file():
                missing.append(str(sample))
            else:
                duration = validate_wav(sample)
                print(f'{folder}: WAV valid, {duration:.2f}s (first 30s analysed)')
        except (KeyError, ValueError, OSError, TypeError, struct.error) as error:
            errors.append(f'{folder}: {error}')
    if missing:
        print('Factory WAVs not supplied:\n' + '\n'.join(missing))
    if errors:
        print('\n'.join(errors))
    return not errors and (allow_missing or not missing)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1] / 'Resources')
    parser.add_argument('--allow-missing', action='store_true', help='check manifests for development; not valid for release packaging')
    args = parser.parse_args()
    raise SystemExit(0 if validate(args.root, args.allow_missing) else 1)

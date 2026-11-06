import importlib.util
import json
import math
import shutil
import struct
import tempfile
import unittest
import wave
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('factory_validator', PROJECT / 'tools/validate_factory.py')
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)


class FactoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name) / 'Resources'
        shutil.copytree(PROJECT / 'Resources', self.root)
        for folder in validator.FOLDERS:
            sample = self.root / 'Presets/Factory' / folder / 'Samples/dna.wav'
            with wave.open(str(sample), 'wb') as stream:
                stream.setparams((1, 2, 48000, 0, 'NONE', 'not compressed'))
                stream.writeframes(b''.join(struct.pack('<h', int(12000 * math.sin(i * .1))) for i in range(4800)))

    def tearDown(self):
        self.temp.cleanup()

    def test_complete_installer_content(self):
        self.assertTrue(validator.validate(self.root))

    def test_missing_factory_sample_blocks_release(self):
        next(self.root.rglob('dna.wav')).unlink()
        self.assertFalse(validator.validate(self.root))
        self.assertTrue(validator.validate(self.root, allow_missing=True))

    def test_sample_must_stay_in_its_preset_folder(self):
        manifest = next(self.root.rglob('preset.json'))
        content = json.loads(manifest.read_text())
        content['sample'] = '../../../other.wav'
        manifest.write_text(json.dumps(content))
        self.assertFalse(validator.validate(self.root, allow_missing=True))

    def test_silent_content_is_not_a_factory_sample(self):
        sample = next(self.root.rglob('dna.wav'))
        with wave.open(str(sample), 'wb') as stream:
            stream.setparams((1, 2, 48000, 0, 'NONE', 'not compressed'))
            stream.writeframes(bytes(9600))
        self.assertFalse(validator.validate(self.root))

    def test_float_wav_supported(self):
        sample = next(self.root.rglob('dna.wav'))
        payload = b''.join(struct.pack('<f', .5 * math.sin(i * .1)) for i in range(4800))
        fmt = struct.pack('<HHIIHH', 3, 1, 48000, 192000, 4, 32)
        sample.write_bytes(b'RIFF' + struct.pack('<I', 36 + len(payload)) + b'WAVEfmt ' + struct.pack('<I', 16) + fmt + b'data' + struct.pack('<I', len(payload)) + payload)
        self.assertTrue(validator.validate(self.root))


if __name__ == '__main__':
    unittest.main()

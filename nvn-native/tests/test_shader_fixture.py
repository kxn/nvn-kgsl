import copy
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('fixture', Path(__file__).resolve().parents[1] / 'tools/shader_fixture.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class FixtureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'input').write_bytes(bytes(256))  # synthetic data, never submitted
        blob = {'path': 'input', 'offset': 0, 'size': 8, 'sha256': fixture.digest(bytes(8))}
        self.doc = {'schema': 1, 'stage': 'cs', 'profile': {'abi': 'test'},
                    'compiler': {'revision': 'test'}, 'provenance': {'source': 'synthetic'},
                    'blobs': {name: copy.deepcopy(blob) for name in ('code', 'control', 'c1')},
                    'state': {'resources': {}, 'host': {}, 'precision': {}, 'graphics': None,
                              'compute': {'local_size': [1, 1, 1], 'shared_bytes': 0, 'local_bytes': 0}}}

    def key(self):
        return fixture.validate(self.doc, self.root)['request_sha256']

    def test_input_mutation_is_rejected(self):
        (self.root / 'input').write_bytes(bytes([1]) * 256)
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            self.key()

    def test_range_cannot_read_past_end(self):
        self.doc['blobs']['code']['offset'] = 250
        with self.assertRaisesRegex(ValueError, 'exceeds file'):
            self.key()

    def test_constant_and_state_change_invalidate_key(self):
        original = self.key()
        self.doc['blobs']['c1']['size'] = 0
        self.doc['blobs']['c1']['sha256'] = fixture.digest(b'')
        self.assertNotEqual(original, self.key())
        original = self.key()
        self.doc['state']['compute']['local_size'] = [2, 1, 1]
        self.assertNotEqual(original, self.key())

    def test_no_missing_compute_defaults(self):
        del self.doc['state']['compute']['shared_bytes']
        with self.assertRaises(ValueError):
            self.key()

    def test_paths_do_not_specialize_shader(self):
        original = self.key()
        (self.root / 'alias').write_bytes(bytes(256))
        self.doc['blobs']['code']['path'] = 'alias'
        self.assertEqual(original, self.key())

    def test_compiler_and_profile_change_invalidate_key(self):
        original = self.key()
        self.doc['compiler']['revision'] = 'other'
        self.assertNotEqual(original, self.key())
        original = self.key()
        self.doc['profile']['abi'] = 'other'
        self.assertNotEqual(original, self.key())


if __name__ == '__main__':
    unittest.main()

"""Mock probe provenance checks: no real Android build, ADB, GPU or game."""
import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile


NATIVE = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_tool(name):
    spec = importlib.util.spec_from_file_location('_provenance_' + name, NATIVE / 'tools' / (name + '.py'))
    module = importlib.util.module_from_spec(spec)
    with patch.object(sys, 'path', [str(NATIVE / 'tools'), *sys.path]):
        spec.loader.exec_module(module)
    return module


class ProbeProvenance(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='nvn-mock-provenance-')
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.native = self.base / 'native'
        self.sdk, self.ndk, self.out = [self.base / name for name in ('sdk', 'ndk', 'output')]
        sources = ('tools/build_android_probe.py', 'tools/run_android_probe.py',
                   'tools/native_contract.py', 'include/nvn_native/profile.h',
                   'third_party/nvn/nvn.h', 'src/nvn.c', 'src/nvn_entries.inc',
                   'android/AndroidManifest.xml')
        for relative in sources:
            target = self.native / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(NATIVE / relative, target)
        self.source = self.native / 'src/nvn.c'
        bt = self.sdk / 'build-tools/36.0.0'
        self.tools = [bt / name for name in ('aapt2', 'zipalign', 'apksigner', 'lib/apksigner.jar')]
        self.tools += [self.sdk / 'platforms/android-36/android.jar',
                       self.ndk / 'source.properties', self.ndk / 'build/cmake/android.toolchain.cmake',
                       self.ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin/clang']
        self.which = {name: str(self.base / 'bin' / name) for name in ('cmake', 'ninja', 'keytool')}
        self.tools += [Path(name) for name in self.which.values()]
        for tool in self.tools:
            tool.parent.mkdir(parents=True, exist_ok=True)
            tool.write_text('MOCK TOOL INPUT ' + tool.name + '\n')
        self.keystore = self.base / 'debug.keystore'
        self.keystore.write_text('MOCK EXISTING SIGNING KEY\n')
        self.builder = load_tool('build_android_probe')
        self.runner = load_tool('run_android_probe')
        self.builder.__file__ = str(self.native / 'tools/build_android_probe.py')
        self.runner.__file__ = str(self.native / 'tools/run_android_probe.py')
        self.commands = []
        self.mutate = None

    def fake_build(self, argv, **kwargs):
        argv = list(map(str, argv))
        self.commands.append(argv)
        self.assertTrue(kwargs['check'])
        executable = Path(argv[0]).name
        if executable == 'cmake':
            if argv[1] == '--build':
                library = Path(argv[2]) / 'libnvn_kgsl_probe.so'
                library.parent.mkdir(parents=True, exist_ok=True)
                library.write_bytes(b'MOCK ARM64 LIBRARY, NOT EXECUTABLE\n')
                os.utime(library, (1700000000, 1700000000))
            else:
                self.assertEqual(argv[1], '-S')
                self.assertIn('-DNVN_DEVICE_INSPECT=ON', argv)
                self.assertIn('-DNVN_AHB_PRESENT=OFF', argv)
                self.assertIn('-DNVN_AHB_GRAPHICS_HEADER=', argv)
        elif executable == 'aapt2':
            with zipfile.ZipFile(argv[argv.index('-o') + 1], 'w') as archive:
                archive.writestr(zipfile.ZipInfo('AndroidManifest.xml'), b'MOCK MANIFEST')
        elif executable == 'zipalign':
            shutil.copyfile(argv[-2], argv[-1])
        elif executable == 'apksigner':
            if argv[1] == 'sign':
                shutil.copyfile(argv[-1], argv[argv.index('--out') + 1])
            else:
                self.assertEqual(argv[1], 'verify')
                if self.mutate:
                    self.mutate.write_bytes(self.mutate.read_bytes() + b'\nCHANGED DURING MOCK BUILD\n')
        else:
            self.fail('Unexpected external command (never execute): ' + repr(argv))
        return subprocess.CompletedProcess(argv, 0)

    def build(self):
        argv = ['build_android_probe.py', '--sdk', str(self.sdk), '--ndk', str(self.ndk),
                '--out', str(self.out), '--keystore', str(self.keystore), '--device-inspect']
        with patch.object(sys, 'argv', argv), patch.object(self.builder.shutil, 'which', self.which.__getitem__), \
                patch.object(self.builder.subprocess, 'run', self.fake_build), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.builder.main()
        return json.loads((self.out / 'build-record.json').read_text())

    def test_stable_build_records_all_inputs(self):
        record = self.build()
        self.assertEqual(record, self.build())
        self.assertEqual(record['source'], self.builder.source_map(self.native, self.out))
        self.assertEqual(record['tools'], {str(tool): digest(tool) for tool in self.tools})
        self.assertEqual(record['nvn_contract'], self.builder.native_contract(self.native))
        self.assertEqual(record['artifacts'], {str(p): digest(p) for p in
                         (self.out / 'cmake/libnvn_kgsl_probe.so', self.out / 'probe.apk')})
        self.assertTrue(record['device_inspect'])
        self.assertFalse(record['compiled_inputs'])
        self.assertTrue(all(Path(command[0]).name != 'adb' for command in self.commands))

    def test_source_mutation_does_not_publish_record(self):
        self.mutate = self.source
        with self.assertRaises(SystemExit) as failure:
            self.build()
        self.assertEqual(failure.exception.code, 2)
        self.assertFalse((self.out / 'build-record.json').exists())

    def test_rebuild_rejects_modified_cached_products_before_commands(self):
        record = self.build()
        compiled = self.base / 'cached-generated.h'
        compiled.write_text('MOCK CACHED GENERATED INPUT\n')
        record['compiled_inputs'] = {str(compiled): digest(compiled)}
        (self.out / 'build-record.json').write_text(json.dumps(record))
        for path in (self.out / 'cmake/libnvn_kgsl_probe.so', self.out / 'probe.apk', compiled):
            with self.subTest(cached=path.name):
                original = path.read_bytes()
                path.write_bytes(original + b'CORRUPT CACHED PRODUCT')
                before = len(self.commands)
                try:
                    with self.assertRaises(SystemExit) as failure:
                        self.build()
                    self.assertEqual(failure.exception.code, 2)
                    self.assertEqual(len(self.commands), before)
                finally:
                    path.write_bytes(original)

    def test_tool_mutation_does_not_publish_record(self):
        # Include the transitive signing implementation and platform API jar.
        for tool in (self.sdk / 'build-tools/36.0.0/lib/apksigner.jar',
                     self.sdk / 'platforms/android-36/android.jar',
                     self.ndk / 'toolchains/llvm/prebuilt/linux-x86_64/bin/clang'):
            with self.subTest(tool=tool.name):
                self.mutate = tool
                with self.assertRaises(SystemExit) as failure:
                    self.build()
                self.assertEqual(failure.exception.code, 2)
                self.assertFalse((self.out / 'build-record.json').exists())

    def runner_argv(self):
        return ['run_android_probe.py', '--serial', 'MOCK-NOT-A-DEVICE', '--build', str(self.out),
                '--evidence', str(self.base / 'evidence.json'), '--install']

    def test_runner_rejects_corruption_before_any_adb(self):
        record = self.build()
        compiled = self.base / 'compiled-input.h'
        compiled.write_text('MOCK VERIFIED GENERATED INPUT\n')
        record['compiled_inputs'] = {str(compiled): digest(compiled)}
        (self.out / 'build-record.json').write_text(json.dumps(record))
        paths = [('source', self.source), ('tool', self.tools[0]),
                 ('signing-jar', self.sdk / 'build-tools/36.0.0/lib/apksigner.jar'),
                 ('platform-jar', self.sdk / 'platforms/android-36/android.jar'),
                 ('compiled-input', compiled), ('library', self.out / 'cmake/libnvn_kgsl_probe.so'),
                 ('apk', self.out / 'probe.apk')]
        for label, path in paths:
            with self.subTest(damaged=label):
                original = path.read_bytes()
                path.write_bytes(original + b'CORRUPT')
                try:
                    with patch.object(sys, 'argv', self.runner_argv()), \
                            patch.object(self.runner.subprocess, 'run') as command, \
                            contextlib.redirect_stderr(io.StringIO()):
                        with self.assertRaises(SystemExit) as failure:
                            self.runner.main()
                        self.assertEqual(failure.exception.code, 2)
                        command.assert_not_called()
                    self.assertFalse((self.base / 'evidence.json').exists())
                finally:
                    path.write_bytes(original)

    def test_valid_record_reaches_only_mocked_adb(self):
        self.build()
        class MockAdbReached(Exception):
            pass
        def stop_at_adb(argv, **_kwargs):
            self.assertEqual(argv[:4], ['adb', '-s', 'MOCK-NOT-A-DEVICE', 'install'])
            raise MockAdbReached
        with patch.object(sys, 'argv', self.runner_argv()), \
                patch.object(self.runner.subprocess, 'run', side_effect=stop_at_adb) as command:
            with self.assertRaises(MockAdbReached):
                self.runner.main()
            self.assertEqual(command.call_count, 1)

    def test_runner_rejects_source_inventory_change_before_adb(self):
        self.build()
        extra = self.native / 'src/new-build-input.c'
        extra.write_text('MOCK NEW SOURCE\n')
        with patch.object(sys, 'argv', self.runner_argv()), \
                patch.object(self.runner.subprocess, 'run') as command, \
                contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as failure:
                self.runner.main()
            self.assertEqual(failure.exception.code, 2)
            command.assert_not_called()


if __name__ == '__main__':
    unittest.main()

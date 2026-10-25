#!/usr/bin/env python3
"""HOST ONLY: exact original VI identity plus RAM frontend/control boundaries.

No Android/GPU/original module execution. Synthetic ANW/registry/main/resolver
fixtures never enter the production shared-library build.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))
from workspace_paths import d2r_root


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    activity = Path(__file__).resolve().parents[1]
    root = activity.parents[3]
    fixtures = activity / 'tests'
    out = args.out.resolve()
    if not out.is_relative_to(root / '.local'):
        parser.error('host test artifacts must remain under this worktree .local')
    out.mkdir(parents=True, exist_ok=True)
    inputs = [Path(__file__), activity / 'activity_host.c', activity / 'activity_host.h',
              fixtures / 'attach_test.c', fixtures / 'include/android/native_window.h',
              d2r_root() / 'dm/gfx/hle_vi.c', d2r_root() / 'dm/host/android/display.c',
              d2r_root() / 'dm/host/android/main.c', d2r_root() / 'dm/host/dm.h',
              d2r_root() / 'dm/host/hle.h', d2r_root() / 'dm/host/hle_oe.c', d2r_root() / 'dm/host/platform.h',
              root / 'nvn-native/adapters/d2r/host_bridge.h']
    inputs += sorted((root / 'nvn-native/include').rglob('*.h'))
    inputs += sorted((root / 'nvn-native/third_party/nvn').glob('*.h'))
    before = {str(p): sha(p) for p in inputs}
    results = []
    for compiler, flags, label in (
            ('gcc', ['-fsanitize=address,undefined', '-fno-sanitize-recover=all'], 'asan-ubsan'),
            ('clang', ['-fsanitize=function', '-fsanitize-trap=function'], 'function-abi')):
        command = [compiler, '-std=c11', '-D_GNU_SOURCE', '-include', 'sys/types.h',
                   '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                   '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', '-DDM_HEADLESS=1', *flags,
                   '-I' + str(fixtures / 'include'), '-I' + str(root / 'nvn-native/include'),
                   '-I' + str(root / 'nvn-native/third_party/nvn'),
                   '-I' + str(root / 'nvn-native/adapters/d2r'), '-I' + str(d2r_root() / 'dm/host'),
                   str(fixtures / 'attach_test.c'), str(d2r_root() / 'dm/gfx/hle_vi.c'),
                   str(d2r_root() / 'dm/host/android/display.c'), '-lpthread', '-o', str(out / label)]
        compiled = subprocess.run(command, text=True, capture_output=True)
        if compiled.returncode:
            raise RuntimeError(compiled.stderr)
        run = subprocess.run([str(out / label)], text=True, capture_output=True,
                             env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1'})
        if run.returncode:
            raise RuntimeError(run.stderr)
        summary = json.loads(run.stdout)
        if (summary['checks'] != 65 or summary['gpu_executed'] or
                summary['original_modules_executed'] or not summary['original_vi_identity']):
            raise RuntimeError('unexpected RAM fixture contract')
        results.append({'compile': command, 'compile_stderr': compiled.stderr,
                        'returncode': run.returncode, 'stdout': run.stdout, 'stderr': run.stderr,
                        'artifact_sha256': sha(out / label)})
    # Compile/preprocess the ACTUAL main under both macros. No host/game entry
    # execution: verify the old CLI exit and new shared return remain separate.
    for shared in (False, True):
        command = ['gcc', '-std=c11', '-D_GNU_SOURCE', '-I' + str(d2r_root() / 'dm/host'),
                   '-Dmain=dm_activity_main_contract',
                   *(['-DDM_NATIVE_SHARED_ENTRY=1'] if shared else []),
                   '-E', str(d2r_root() / 'dm/host/android/main.c')]
        processed = subprocess.run(command, text=True, capture_output=True, check=True)
        tail = processed.stdout.rsplit('fflush', 1)[1]
        if shared:
            assert 'return 0;' in tail and '_exit(0);' not in tail
        else:
            assert '_exit(0);' in tail and 'return 0;' not in tail
        syntax = command.copy()
        syntax[syntax.index('-E')] = '-fsyntax-only'
        subprocess.run(syntax, text=True, capture_output=True, check=True)
        results.append({'main_preprocess': command, 'shared_return': shared, 'syntax_checked': True})
    after = {str(p): sha(p) for p in inputs}
    if before != after:
        raise RuntimeError('test inputs changed during checks')
    proof = {'schema': 1, 'host_only': True, 'game_modules_executed': False,
             'gpu_executed': False, 'frontend_checks': json.loads(results[0]['stdout'])['checks'], 'main_macro_contracts': 2,
             'inputs_before': before, 'inputs_after': after, 'results': results}
    (out / 'proof.json').write_text(json.dumps(proof, indent=2) + '\n')
    print(out / 'proof.json')


if __name__ == '__main__':
    main()

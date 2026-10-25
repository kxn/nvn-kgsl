#!/usr/bin/env python3
"""Exercise the pinned IR3 host compiler; this does not run GPU code."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    record = json.loads((build / 'build-record.json').read_text())
    binary = build / 'build/src/freedreno/fdl/nvn-ir3-compile'
    if record['artifacts'][str(binary)] != sha(binary):
        parser.error('compiler artifact differs from build record')
    env = os.environ.copy()
    env['IR3_SHADER_DEBUG'] = 'asmroundtrip'
    cases = []
    for name, value in [('write-a', 0x4e564e32), ('write-b', 0x12345678)]:
        prefix = out / name
        command = [str(binary), str(prefix), hex(value)]
        result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=30)
        if result.returncode:
            raise RuntimeError(result.stderr or f'compiler exited {result.returncode}')
        metadata = json.loads(prefix.with_suffix('.json').read_text())
        code = prefix.with_suffix('.bin')
        asm = prefix.with_suffix('.asm').read_text()
        if metadata['expected'] != value or metadata['chip_id'] != 0x43030b00:
            raise RuntimeError('unexpected compilation metadata')
        if metadata['binary_bytes'] != code.stat().st_size or not code.stat().st_size:
            raise RuntimeError('invalid code extent')
        if 'stib.' not in asm or hex(value) not in asm:
            raise RuntimeError('expected storage write missing from IR3 disassembly')
        cases.append({'command': command, 'metadata': metadata,
                      'stdout': result.stdout, 'stderr': result.stderr,
                      'artifacts': {p.name: sha(p) for p in (code, prefix.with_suffix('.asm'),
                                                            prefix.with_suffix('.nir'), prefix.with_suffix('.json'),
                                                            prefix.with_suffix('.h'))}})
    if cases[0]['artifacts']['write-a.bin'] == cases[1]['artifacts']['write-b.bin']:
        raise RuntimeError('changing the stored value did not change machine code')
    rejected = subprocess.run([str(binary), str(out / 'invalid'), '-1'],
                              capture_output=True, text=True, timeout=30)
    if rejected.returncode != 2:
        raise RuntimeError('invalid unsigned input not rejected')
    evidence = {'scope': 'host NIR -> IR3 compilation and Mesa assembler roundtrip only',
                'gpu_execution': False, 'maxwell_translation': False, 'build': record,
                'environment': {'IR3_SHADER_DEBUG': 'asmroundtrip'}, 'cases': cases,
                'invalid_input_exit': rejected.returncode, 'passed': True}
    (out / 'check.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(out / 'check.json')


if __name__ == '__main__':
    main()

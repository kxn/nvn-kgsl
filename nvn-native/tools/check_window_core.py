#!/usr/bin/env python3
"""HOST ONLY: standard NVN Window resource/PFN tests with a synthetic session."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--ndk-clang',type=Path,required=True)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[2];out=args.out.resolve()
    if not out.is_relative_to(root/'.local') or out.exists():parser.error('choose a new ignored output below worktree .local')
    out.mkdir(parents=True)
    native=root/'nvn-native'
    sources=[native/'tests/d2r_bridge/window_core.c',native/'src/external_image.c',native/'src/compute.c',native/'src/graphics.c',native/'src/order.c',native/'src/render_state.c',native/'src/sampler_state.c',native/'src/texture_source.c', native/'src/texture_bridge.c', native/'src/counter.c']
    files=sources+[native/'src/nvn.c',native/'src/nvn_entries.inc',native/'tests/d2r_bridge/memory_backend.c',native/'tests/d2r_bridge/window_session_backend.inc',Path(__file__).resolve()]
    files+=list((native/'include/nvn_native').glob('*.h'))+[native/'third_party/nvn/nvn.h']
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    proof=dict(scope='HOST ONLY core/resource and synthetic session, not real platform/GPU/game',passed=False,runs=[],sources={str(p):sha(p) for p in files})
    def run(label,cmd):
        p=subprocess.run(list(map(str,cmd)),cwd=root,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
        proof['runs'].append(dict(label=label,argv=list(map(str,cmd)),returncode=p.returncode,stdout=p.stdout,stderr=p.stderr))
        if p.returncode:raise RuntimeError(label+' failed')
        return p.stdout
    flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(native/'include'),'-I'+str(native/'third_party/nvn')]
    try:
        for cc,sanitizer in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all'])]:
            binary=out/cc;run(cc+' compile',[cc,*flags,*sanitizer,*sources,'-o',binary]);data=json.loads(run(cc+' run',[binary]))
            if data.get('checks')!=292 or data.get('passed') is not True:raise ValueError('unexpected fixture counters')
        run('NDK actual core strict compile',[args.ndk_clang,*flags,'-fPIC','-c',native/'src/nvn.c','-o',out/'nvn.o'])
        if any(sha(Path(p))!=h for p,h in proof['sources'].items()):raise ValueError('source changed during checks')
        proof['passed']=True
    except Exception as e:proof['error']=str(e)
    (out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],proof=str(out/'proof.json'))))
    return 0 if proof['passed'] else 1
if __name__=='__main__':raise SystemExit(main())

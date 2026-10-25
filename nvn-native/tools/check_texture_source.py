#!/usr/bin/env python3
"""Check source-payload geometry and real converter encoding; no GPU execution."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path,required=True);p.add_argument('--check',type=Path,required=True)
    p.add_argument('--ndk-clang',type=Path,required=True);a=p.parse_args()
    root=Path(__file__).resolve().parents[2];n=root/'nvn-native';out=a.out.resolve()
    if out.exists() or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
    out.mkdir(parents=True);sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
    files=[Path(__file__).resolve(),a.check.resolve(),n/'src/texture_source.c',n/'include/nvn_native/texture_source.h',
           n/'tests/source_layout_test.c',n/'tests/native_encode_test.c',n/'tests/d2r_bridge/memory_backend.c',
           n/'tests/d2r_bridge/window_session_backend.inc',n/'src/compute.c',n/'src/external_image.c',a.ndk_clang.resolve()]
    files+=[f for f in (n/'include').rglob('*.h')]
    proof={'passed':False,'scope':'HOST source geometry/address math and real CS encoding; static Android link. No GPU/public NVN texture/game qualification.',
           'inputs':{str(f):sha(f) for f in files},'runs':[]}
    def run(label,argv):
        value=subprocess.run(list(map(str,argv)),capture_output=True,text=True,
            env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
        proof['runs'].append({'label':label,'argv':list(map(str,argv)),'returncode':value.returncode,'stdout':value.stdout,'stderr':value.stderr})
        if value.returncode:raise RuntimeError(label+' failed')
        return value.stdout
    try:
        check=json.loads(a.check.read_text())
        if check.get('kind')!='native-texture-layout-copy-v1' or check.get('runtime_compiler') is not False or check.get('gpu_execution') is not False:raise ValueError('wrong authored conversion artifact')
        closure={**check['inputs'],**check['artifacts']}
        if any(sha(f)!=h for f,h in closure.items()):raise ValueError('converter artifact/input mismatch')
        cases={x['name']:x for x in check['cases']}
        if set(cases)!={'gob_copy','linear_copy'}:raise ValueError('unexpected converter inventory')
        define=lambda key,value:'-D'+key+'="'+str(value)+'"'
        flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn')]
        native=[n/'tests/native_encode_test.c',n/'tests/d2r_bridge/memory_backend.c',n/'src/compute.c',n/'src/external_image.c']
        definitions=[define('NN_GOB_COPY_HEADER',cases['gob_copy']['header']),define('NN_LINEAR_COPY_HEADER',cases['linear_copy']['header'])]
        fixtures=[('layout',[n/'tests/source_layout_test.c',n/'src/texture_source.c'],[],'source-layout/address checks=780353'),('native',native,definitions,'native encode checks=8198')]
        for cc,san in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie'])]:
            for name,sources,defs,expected in fixtures:
                binary=out/(name+'-'+cc);run(name+' '+cc+' compile',[cc,*flags,*san,*defs,*sources,'-o',binary])
                if expected not in run(name+' '+cc+' execute',[binary]):raise ValueError('fixture output changed')
        # Actual calculator+CS encoder target link, never the RAM ownership backend.
        binary=out/'libtexture-source-api29.so'
        run('NDK29 real source/encoder/backend strict shared link',[a.ndk_clang,*flags,'-O2','-I'+str(n/'third_party/kgsl'),'-fPIC','-shared','-Wl,--no-undefined',n/'src/texture_source.c',n/'src/compute.c',n/'src/kgsl.c','-o',binary])
        if any(sha(f)!=h for f,h in {**proof['inputs'],**closure}.items()):raise ValueError('inputs mutated during checks')
        proof['passed']=True;proof['converter_check_sha256']=sha(a.check);proof['binary_sha256']=sha(binary)
    except Exception as error:proof['error']=str(error)
    (out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n')
    print(json.dumps({'passed':proof['passed'],'proof':str(out/'proof.json')}));return 0 if proof['passed'] else 1


if __name__=='__main__':raise SystemExit(main())

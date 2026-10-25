#!/usr/bin/env python3
"""Verify recovered NVN ABI forwarding against original name extraction; no GPU."""
import argparse
import hashlib
import json
import os
from pathlib import Path
from workspace_paths import d2r_root
import shutil
import subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--nso',type=Path,required=True)
    parser.add_argument('--python',type=Path,required=True)
    parser.add_argument('--ndk-clang',type=Path,required=True)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[2];native=root/'nvn-native';out=args.out.resolve()
    if out.exists() or not out.is_relative_to(root/'.local'):parser.error('fresh ignored output required')
    out.mkdir(parents=True);names=out/'names';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    relative=['nvn-native/adapters/d2r/'+n for n in ('extract_nvn_names.py','host_bridge.c','host_bridge.h')]
    relative+=['nvn-native/tools/workspace_paths.py','nvn-native/third_party/nvn/nvn.h','dm/tools/nso.py','dm/gfx/tools/nvn_slots.py']
    files=[(d2r_root() if p.startswith('dm/') else root)/p for p in relative]+[native/'tests/d2r_bridge/abi_forward.c',native/'tests/d2r_bridge/abi_public_three.c',Path(__file__).resolve(),
        native/'include/nvn_native/nvn.h',native/'include/nvn_native/profile.h',d2r_root()/'dm/host/hle.h',d2r_root()/'dm/host/platform.h',
        args.nso.resolve(),args.ndk_clang.resolve(),args.python.resolve()]
    proof={'passed':False,'scope':'HOST typed ABI mock, original static metadata and Android strict linkage only; no native getter implementation/GPU/game',
           'inputs':{str(p):sha(p) for p in files},'runs':[]}
    def run(label,argv,expected=0):
        value=subprocess.run(list(map(str,argv)),capture_output=True,text=True,
            env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1'})
        proof['runs'].append({'label':label,'argv':list(map(str,argv)),'returncode':value.returncode,'stdout':value.stdout,'stderr':value.stderr})
        if value.returncode!=expected:raise RuntimeError(label+' failed')
        return value.stdout
    argv=[args.python.absolute(),native/'adapters/d2r/extract_nvn_names.py','--nso',args.nso.resolve(),'--out',names]
    try:
        run('original metadata generation',argv);run('original metadata verify',argv+['--verify'])
        metadata=json.loads((names/'names-record.json').read_text())
        if (metadata['names'],metadata['public_prototype_names'],metadata['typed_forward_names'],len(metadata['unprototyped_fatal_boundaries']))!=(523,508,507,15):raise ValueError('ABI counts differ')
        common=['-D_GNU_SOURCE','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
            '-I'+str(native/'third_party/nvn'),'-I'+str(native/'include'),'-I'+str(native/'adapters/d2r'),'-I'+str(d2r_root()),
            '-DNN_D2R_HOST_HLE=1','-DNN_D2R_NVN_NAMES_HEADER="'+str(names/'names.inc.h')+'"']
        sources=[native/'tests/d2r_bridge/abi_forward.c',native/'adapters/d2r/host_bridge.c']
        for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
            binary=out/cc;run(cc+' compile',[cc,*common,*san,*sources,'-o',binary]);result=json.loads(run(cc+' execute',[binary]).splitlines()[-1])
            if result.get('passed') is not True or result.get('forward_calls')!=30 or result.get('checks')!=182:raise ValueError('unexpected ABI fixture result')
            extra=out/('public-three-'+cc)
            run(cc+' public three compile',[cc,*common,*san,native/'tests/d2r_bridge/abi_public_three.c',sources[1],'-o',extra])
            result=json.loads(run(cc+' public three execute',[extra]).splitlines()[-1])
            if result.get('passed') is not True or result.get('forward_calls')!=272 or result.get('gpu_submits')!=0:raise ValueError('unexpected public three fixture result')
        binary=out/'libabi.so';run('NDK29 strict HLE link',[args.ndk_clang,*common,'-O2','-fPIC','-shared','-Wl,--no-undefined',*sources,'-o',binary])
        elf=run('ARM64 ELF',[args.ndk_clang.parent/'llvm-readelf','-h',binary]);sections=run('HLE_FORCE section',[args.ndk_clang.parent/'llvm-readelf','-SW',binary])
        if 'AArch64' not in elf or 'ELF64' not in elf or 'swforce' not in sections:raise ValueError('wrong target or missing HLE')
        run('NDK29 public three strict HLE link',[args.ndk_clang,*common,'-O2','-fPIC','-shared','-Wl,--no-undefined',native/'tests/d2r_bridge/abi_public_three.c',sources[1],'-o',out/'libabi-public-three.so'])
        bad=out/'bad-root'
        for name in relative:
            q=bad/name;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile((d2r_root() if name.startswith('dm/') else root)/name,q)
        header=bad/'nvn-native/third_party/nvn/nvn.h';text=header.read_text();before='NVNmemoryPool* (*PFNNVNTEXTUREBUILDERGETMEMORYPOOLPROC)'
        if text.count(before)!=1:raise ValueError('pointer negative fixture contract changed')
        header.write_text(text.replace(before,before.replace('Pool*','Pool'),1))
        run('opaque by-value return rejected',[args.python.absolute(),bad/'nvn-native/adapters/d2r/extract_nvn_names.py','--nso',args.nso.resolve(),'--out',bad/'.local/names'],2)
        if 'opaque NVN object returned by value: nvnTextureBuilderGetMemoryPool' not in proof['runs'][-1]['stderr'] or (bad/'.local/names/names-record.json').exists():raise ValueError('opaque guard failed')
        run('original metadata final verify',argv+['--verify'])
        if any(sha(p)!=h for p,h in proof['inputs'].items()):raise ValueError('inputs mutated')
        proof['passed']=True;proof['metadata']={k:metadata[k] for k in ('names','static_call_names','public_prototype_names','typed_forward_names','unprototyped_fatal_boundaries')}
        proof['artifacts']={str(p):sha(p) for p in (binary,names/'names-record.json',names/'names.inc.h')}
    except Exception as error:proof['error']=str(error)
    (out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n')
    print(json.dumps({'passed':proof['passed'],'proof':str(out/'proof.json')}));return 0 if proof['passed'] else 1


if __name__=='__main__':raise SystemExit(main())

#!/usr/bin/env python3
"""Isotropic mip sampler words vs actual pinned XML packs; HOST + NDK, no GPU."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def check(root, native, ir3, ndk, out):
    if out.exists() or not out.is_relative_to(root/'.local') or out==root/'.local':
        raise ValueError('fresh dedicated ignored output required')
    spec=importlib.util.spec_from_file_location('mip_normal_compiler',root/'nvn-native/tools/compute_driverparams.py')
    own=importlib.util.module_from_spec(spec);spec.loader.exec_module(own)
    compiler,inputs,_,_=own.compiler_inputs(root,ir3)
    source=ir3/'source/mesa-26.2.5/src/freedreno/registers'
    built=ir3/'build/src/freedreno/registers'
    tracked=root/'nvn-native'
    fixture=native/'tests/d2r_bridge/sampler_mips.c';module=native/'src/sampler_state.c'
    files=[module,native/'include/nvn_native/sampler_state.h',fixture,tracked/'tests/d2r_bridge/sampler_words.c',tracked/'third_party/nvn/nvn.h',Path(__file__).resolve(),Path(sys.executable).resolve()]
    files += [p for p in source.rglob('*')if p.is_file()and'__pycache__'not in p.parts]
    for p in files:inputs[str(p)]=sha(p)
    out.mkdir(parents=True);commands=[];results=[]
    def run(args):
        result=subprocess.run(list(map(str,args)),capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
        commands.append(dict(argv=list(map(str,args)),returncode=result.returncode,stdout=result.stdout,stderr=result.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
        if result.returncode:raise ValueError((result.stdout+result.stderr)[-8000:])
        return result.stdout
    headers=out/'headers/adreno';headers.mkdir(parents=True)
    for name in('adreno_common','a6xx_enums','a6xx_descriptors'):
        path=source/'adreno'/(name+'.xml');target=headers/(name+'.xml.h')
        target.write_text(run([sys.executable,source/'gen_header.py','--rnn',source,'--xml',path,'c-defines']))
        golden=built/'adreno'/target.name;inputs[str(golden)]=sha(golden)
        if target.read_bytes()!=golden.read_bytes():raise ValueError('normal XML/generated header mismatch')
    common=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(native/'include'),'-I'+str(tracked/'include'),'-I'+str(tracked/'third_party/nvn'),'-I'+str(out/'headers')]
    for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
        compiler_path=Path(shutil.which(cc)).resolve();inputs[str(compiler_path)]=sha(compiler_path)
        for name,test in [('mips',fixture),('legacy',tracked/'tests/d2r_bridge/sampler_words.c')]:
            binary=out/(name+'-'+cc);run([cc,*common,*san,'-fno-pie','-no-pie',test,module,'-o',binary]);record=json.loads(run([binary]))
            if record.get('passed')is not True or record.get('gpu_submits')!=0:raise ValueError('fixture scope mismatch')
            results.append(dict(compiler=cc,fixture=name,result=record))
    cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
    inputs[str(cc.resolve())]=sha(cc.resolve())
    run([cc,*common,'-fPIC','-shared',module,'-Wl,--no-undefined','-o',out/'sampler-mips-api29.so'])
    run([cc,*common,fixture,module,'-Wl,--no-undefined','-o',out/'sampler-mips-fixture-api29'])
    for p,h in inputs.items():
        if sha(p)!=h:raise ValueError('input changed during check '+p)
    proof=dict(schema=1,passed=True,kind='sampler-isotropic-mips-host-v1',gpu_execution=False,android_execution=False,default_enabled=False,results=results,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()})
    (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');return out/'check.json'


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in('root','ir3','ndk','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--native-root',type=Path)
    a=p.parse_args();root=a.root.resolve();native=(a.native_root or root/'nvn-native').resolve()
    print(check(root,native,a.ir3.resolve(),a.ndk.resolve(),a.out.resolve()))

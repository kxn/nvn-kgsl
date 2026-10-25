#!/usr/bin/env python3
"""HOST successful-allocation cleanup quarantine + existing import regression; no GPU."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','ndk','out'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--backend-source',type=Path);a=p.parse_args();root=a.root.resolve();out=a.out.resolve();n=root/'nvn-native';backend=(a.backend_source or n/'src/kgsl.c').resolve();fixture=Path(__file__).resolve().parents[1]/'tests/d2r_bridge/kgsl_alloc_owner.c';regression=n/'tests/d2r_bridge/kgsl_import_coherent.c'
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';readelf=cc.parent/'llvm-readelf'
 sources=[backend,fixture,regression,n/'include/nvn_native/kgsl.h',n/'third_party/kgsl/msm_kgsl.h',cc.resolve(),readelf,Path(__file__).resolve()]
 inputs={str(f):sha(f)for f in sources};out.mkdir(parents=True);commands=[];results=[]
 # The unmodified old import fixture includes nvn-native/src/kgsl.c by name.
 # An exact isolated source copy makes it exercise this candidate too.
 overlay=out/'include-root/nvn-native/src/kgsl.c';overlay.parent.mkdir(parents=True);overlay.write_bytes(backend.read_bytes())
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));commands.append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stderr[-4000:])
  return r.stdout
 try:
  flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(out/'include-root'),'-I'+str(root),'-I'+str(n/'include'),'-I'+str(n/'third_party/kgsl')]
  own=['-DNN_TEST_KGSL_SOURCE='+json.dumps(str(backend))]
  wraps=['-Wl,--wrap='+name for name in ('open','close','ioctl','mmap','munmap')];oldwraps=['-Wl,--wrap='+name for name in ('open','close','ioctl','fstat')]
  for compiler,san in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
   path=Path(shutil.which(compiler)).resolve();inputs[str(path)]=sha(path)
   for name,src,defs,links in [('alloc',fixture,own,wraps),('import-regression',regression,[],oldwraps)]:
    binary=out/(name+'-'+compiler);run([compiler,*flags,*defs,*san,'-fno-pie','-no-pie',src,*links,'-o',binary]);value=json.loads(run([binary]));assert value['passed']is True
    expected=(dict(checks=256,cases=16,alloc_ioctl_calls=19,sharedmem_free_calls=27,close_calls=16,poisoned_fixture_owners_manually_released=1,gpu_execution=False)if name=='alloc'else dict(checks=4297,cases=37,import_ioctl_calls=38,get_info_ioctl_calls=32,free_id_ioctl_calls=29,close_calls=37,cache_ioctl_calls=2,poisoned_fixture_owners_manually_released=2))
    if any(value.get(k)!=v or type(value.get(k))is not type(v)for k,v in expected.items()):raise ValueError('incomplete fault/regression fixture '+name)
    results.append(dict(test=name,compiler=compiler,**value))
  run([cc,*flags,'-fPIC','-shared',backend,'-Wl,--no-undefined','-o',out/'libkgsl-alloc-owner-api29.so'])
  run([cc,*flags,*own,fixture,*wraps,'-Wl,--no-undefined','-o',out/'alloc-owner-fixture-api29'])
  elf=run([readelf,'-h','-d',out/'libkgsl-alloc-owner-api29.so']);assert 'AArch64'in elf and not any(x in elf.lower()for x in ('libvulkan','libegl','libgles'))
  for f,h in inputs.items():
   if sha(f)!=h:raise ValueError('source changed during check '+f)
  assert overlay.read_bytes()==backend.read_bytes()
  proof=dict(schema=1,passed=True,scope=__doc__,gpu_execution=False,android_execution=False,backend_source=str(backend),inputs=inputs,results=results,artifacts={str(f):sha(f)for f in out.rglob('*')if f.is_file()})
 except Exception as e:proof=dict(schema=1,passed=False,error=str(e),scope=__doc__,gpu_execution=False,inputs=inputs,results=results)
 (out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],proof=str(out/'proof.json'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())

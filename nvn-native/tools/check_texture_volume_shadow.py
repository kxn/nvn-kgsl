#!/usr/bin/env python3
"""HOST actual FDL true3D shadow owner/failure checks, strict ARM64 link; no GPU."""
import argparse,hashlib,importlib.util,json,os,shlex,shutil,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p):
 s=importlib.util.spec_from_file_location('volume_shadow_dependency',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def check(root,fdl_check,kernel_check,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output below ROOT/.local required')
 native=Path(__file__).resolve().parents[1];dep=root/'nvn-native'
 fdl=json.loads(fdl_check.read_text());normal=load(dep/'tools/build_runtime_fdl.py')
 fdl_roots=[Path(p).parents[2]for p in fdl['inputs']if p.endswith('/nvn-native/src/runtime_fdl.cc')]
 if len(fdl_roots)!=1:raise ValueError('ambiguous FDL origin')
 library,fdl=normal.verify(fdl_roots[0],fdl_check)
 if 'volume-v1'not in fdl.get('features',[]):raise ValueError('actual FDL volume exports required')
 block=json.loads(kernel_check.read_text());roots=[Path(p).parents[2]for p in block['inputs']if p.endswith('/nvn-native/tools/compile_volume_copy.py')]
 if len(roots)!=1:raise ValueError('ambiguous normal volume kernel')
 load(dep/'tools/compile_volume_copy.py').verify(roots[0],kernel_check)
 inputs={str(fdl_check):sha(fdl_check),str(kernel_check):sha(kernel_check),**fdl['inputs'],**fdl['artifacts'],**block['inputs'],**block['artifacts']}
 for rel in ['src/runtime_fdl.cc','include/nvn_native/runtime_fdl.h','tools/build_runtime_fdl.py','tests/runtime_fdl.cc']:
  if not any(p.endswith('/nvn-native/'+rel)and h==sha(dep/rel)for p,h in fdl['inputs'].items()):raise ValueError('current FDL source differs '+rel)
 for rel in ['shaders/volume_copy.comp','tools/compile_volume_copy.py']:
  if not any(p.endswith('/nvn-native/'+rel)and h==sha(dep/rel)for p,h in block['inputs'].items()):raise ValueError('current kernel source differs '+rel)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('source/product changed '+p)
 sources=[native/'src/texture_volume_shadow.c',*[dep/'src'/s for s in ('block_copy.c','compute.c','texture_source.c')]]
 fixture=native/'tests/d2r_bridge/texture_volume_shadow.c';backend=dep/'src/kgsl.c';facade=dep/'src/runtime_fdl.cc'
 host=Path(fdl['normal_ir3'])/'build';ninja=Path(shutil.which('ninja')).resolve();cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
 for p in [Path(__file__).resolve(),fixture,*sources,backend,facade,host/'compile_commands.json',cc.resolve(),ninja,*sorted((dep/'include/nvn_native').glob('*.h')),*sorted((native/'include/nvn_native').glob('*.h')),dep/'third_party/kgsl/msm_kgsl.h']:
  inputs[str(p)]=sha(p)
 out.mkdir(parents=True);commands=[];results=[]
 def run(argv,cwd=None):
  argv=list(map(str,argv));p=subprocess.run(argv,cwd=cwd,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  commands.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if p.returncode:raise ValueError(p.stdout+p.stderr)
  return p.stdout
 entry=next(e for e in json.loads((host/'compile_commands.json').read_text())if e['file'].endswith('/fd6_view.cc'));_,hf=normal.flags(entry)
 link=shlex.split(run([ninja,'-C',host,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
 for v in link[1:]:
  if skip:skip=False;continue
  if v=='-o':skip=True;continue
  if not v.endswith('.o'):libs.append(v)
 for v in libs:
  p=(host/v).resolve()
  if p.is_file():inputs[str(p)]=sha(p)
 flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(dep/'third_party/nvn'),'-I'+str(native/'include'),'-I'+str(dep/'include'),'-I'+str(dep/'include/nvn_native')]
 defines=['-DNN_VOLUME_SHADOW_IMAGE='+json.dumps(block['header'])]
 for compiler,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  folder=out/compiler;folder.mkdir();objects=[]
  for tool in (compiler,cpp):p=Path(shutil.which(tool)).resolve();inputs[str(p)]=sha(p)
  for i,s in enumerate([fixture,*sources]):
   o=folder/(str(i)+'.o');run([compiler,*flags,*defines,*san,'-fno-pie','-c',s,'-o',o]);objects.append(o)
  ff=hf if cpp=='g++'else[x for x in hf if x not in ('-flifetime-dse=1','-Wno-error=maybe-uninitialized')]
  o=folder/'fdl.o';run([cpp,*ff,'-I'+str(dep/'include'),'-Werror',*san,'-fno-pie','-c',facade,'-o',o],host);objects.append(o)
  binary=folder/'volume-shadow';run([cpp,'-no-pie',*san,*objects,*libs,'-o',binary],host);result=json.loads(run([binary]))
  if not result['passed']or result['gpu_execution']or result['live']or result['owners']!=20 or result['retained_fault_owners']!=3 or result['CPU_pixel_maps']!=0 or result['actual_GPU_submits']!=0:raise ValueError('incomplete HOST fixture')
  results.append(result)
 if results[0]!=results[1]:raise ValueError('sanitizer results differ')
 run([cc,*flags,*defines,fixture,*sources,library,'-lm','-Wl,--no-undefined','-o',out/'volume-shadow-fixture-api29'])
 run([cc,*flags,'-I'+str(dep/'third_party/kgsl'),'-fPIC','-shared',*sources,backend,library,'-lm','-Wl,--no-undefined','-o',out/'volume-shadow-real-kgsl-api29.so'])
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('source/product changed '+p)
 result=dict(schema=1,kind='volume-native-shadow-host-v1',passed=True,gpu_execution=False,android_execution=False,public_nvn_enabled=False,
  scope=__doc__,results=results,inputs=inputs,commands=commands,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()})
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','runtime-fdl-check','kernel-check','ndk','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(check(a.root.resolve(),a.runtime_fdl_check.resolve(),a.kernel_check.resolve(),a.ndk.resolve(),a.out.resolve()))

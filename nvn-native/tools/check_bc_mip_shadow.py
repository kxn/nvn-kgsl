#!/usr/bin/env python3
"""HOST actual FDL/BC-shadow owner+failure checks, strict ARM64 link; no GPU."""
import argparse,hashlib,importlib.util,json,os,shlex,shutil,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p):
 s=importlib.util.spec_from_file_location('bc_shadow_dependency',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def check(root,fdl_check,kernel_check,fetch_check,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output below ROOT/.local required')
 native=Path(__file__).resolve().parents[1];dep=root/'nvn-native'
 normal=load(dep/'tools/build_runtime_fdl.py');library,fdl=normal.verify(root,fdl_check)
 block=load(dep/'tools/compile_block_copy.py').verify(root,kernel_check)
 inputs={str(fdl_check):sha(fdl_check),str(kernel_check):sha(kernel_check),**fdl['inputs'],**fdl['artifacts'],**block['inputs'],**block['artifacts']}
 fetch=json.loads(fetch_check.read_text())
 if fetch.get('kind')!='authored-bc-fetch-pipeline-v1'or not fetch.get('passed')or fetch.get('gpu_execution')is not False:raise ValueError('unknown authored fetch package')
 inputs.update({str(fetch_check):sha(fetch_check),**fetch['inputs'],**fetch['artifacts']})
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('source/product changed '+p)
 sources=[native/'src/texture_bc_shadow.c',native/'src/pipeline.c',*[native/'src'/s for s in ('block_copy.c','compute.c','texture_source.c','pipeline_program.c','pipeline_shader.c','pipeline_vertex.c','offline_graphics.c','sampler_state.c','render_state.c')]]
 fixture=native/'tests/d2r_bridge/texture_bc_shadow.c';backend=dep/'src/kgsl.c';facade=dep/'src/runtime_fdl.cc'
 for rel in ('src/runtime_fdl.cc','include/nvn_native/runtime_fdl.h'):
  if sha(native/rel)!=sha(dep/rel):raise ValueError('isolated facade differs from normal verified source '+rel)
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
 flags=['-DNN_PIPELINE_BC_SAMPLED=1','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(dep/'third_party/nvn'),'-I'+str(native/'include'),'-I'+str(dep/'include'),'-I'+str(dep/'include/nvn_native')]
 defines=['-DNN_BC_BLOCK_IMAGE='+json.dumps(block['header']),'-DNN_BC_FETCH_RECIPE='+json.dumps(str(fetch_check.parent/'bc_fetch.pipeline.h')),'-DNN_BC_FETCH_PLATFORM='+json.dumps(str(fetch_check.parent/'device.platform.h'))]
 for compiler,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  folder=out/compiler;folder.mkdir();objects=[]
  for tool in (compiler,cpp):p=Path(shutil.which(tool)).resolve();inputs[str(p)]=sha(p)
  for i,s in enumerate([fixture,*sources]):
   o=folder/(str(i)+'.o');run([compiler,*flags,*defines,*san,'-fno-pie','-c',s,'-o',o]);objects.append(o)
  ff=hf if cpp=='g++'else[x for x in hf if x not in ('-flifetime-dse=1','-Wno-error=maybe-uninitialized')]
  o=folder/'fdl.o';run([cpp,*ff,'-I'+str(dep/'include'),'-Werror',*san,'-fno-pie','-c',facade,'-o',o],host);objects.append(o)
  binary=folder/'bc-shadow';run([cpp,'-no-pie',*san,*objects,*libs,'-o',binary],host);result=json.loads(run([binary]))
  if not result['passed']or result['gpu_execution']or result['live']or result['owners']!=8 or result['retained_fault_owners']!=3:raise ValueError('incomplete HOST fixture')
  results.append(result)
  if compiler=='clang':
   alternate=[]
   for i,s in enumerate([fixture,*sources]):
    if i in (0,2):
     o=folder/(str(i)+'-off.o');run([compiler,*flags,*defines,*san,'-UNN_PIPELINE_BC_SAMPLED','-DNN_PIPELINE_BC_SAMPLED=0','-fno-pie','-c',s,'-o',o]);alternate.append(o)
    else:alternate.append(objects[i])
   alternate.append(objects[-1]);binary=folder/'bc-shadow-off';run([cpp,'-no-pie',*san,*alternate,*libs,'-o',binary],host);default_off=json.loads(run([binary]))
   if not default_off['passed']or default_off['pipeline_bc_enabled']or default_off['live']:raise ValueError('default-OFF does not reject BC')
 if results[0]!=results[1]:raise ValueError('sanitizer results differ')
 run([cc,*flags,*defines,fixture,*sources,library,'-lm','-Wl,--no-undefined','-o',out/'bc-shadow-fixture-api29'])
 run([cc,*flags,'-I'+str(dep/'third_party/kgsl'),'-fPIC','-shared',*sources,backend,library,'-lm','-Wl,--no-undefined','-o',out/'bc-shadow-real-kgsl-api29.so'])
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('source/product changed '+p)
 result=dict(schema=1,kind='bc-native-mip-shadow-host-v1',passed=True,gpu_execution=False,android_execution=False,public_nvn_enabled=False,
  scope=__doc__,results=results,default_off=default_off,inputs=inputs,commands=commands,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()})
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','runtime-fdl-check','kernel-check','fetch-check','ndk','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(check(a.root.resolve(),a.runtime_fdl_check.resolve(),a.kernel_check.resolve(),a.fetch_check.resolve(),a.ndk.resolve(),a.out.resolve()))

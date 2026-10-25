#!/usr/bin/env python3
"""Actual FDL / nativeBUFFER pipeline HOST identity/packet/owner checks, NDK29 link. No GPU."""
import argparse,hashlib,importlib.util,json,os,shlex,shutil,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p):
 s=importlib.util.spec_from_file_location('buffer_sampled_dependency',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def check(root,fdl_check,shader_check,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored ROOT/.local output required')
 n=root/'nvn-native';fdl=json.loads(fdl_check.read_text());normal=load(n/'tools/build_runtime_fdl.py')
 roots=[Path(p).parents[2]for p in fdl['inputs']if p.endswith('/nvn-native/src/runtime_fdl.cc')]
 if len(roots)!=1:raise ValueError('ambiguous actual FDL source')
 library,fdl=normal.verify(roots[0],fdl_check)
 for rel in ['src/runtime_fdl.cc','include/nvn_native/runtime_fdl.h','tools/build_runtime_fdl.py','tests/runtime_fdl.cc']:
  if not any(p.endswith('/nvn-native/'+rel)and h==sha(n/rel)for p,h in fdl['inputs'].items()):raise ValueError('different actual FDL source '+rel)
 shaders=json.loads(shader_check.read_text())
 if not shaders.get('passed')or shaders.get('kind')!='authored-native-buffer-texture-fetch-v1' or shaders.get('gpu_execution')is not False:raise ValueError('unqualified shader producer')
 inputs={str(shader_check):sha(shader_check),str(fdl_check):sha(fdl_check),**fdl['inputs'],**fdl['artifacts'],**shaders['inputs'],**shaders['artifacts']}
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('source/product changed '+p)
 names=['pipeline_program','pipeline_shader','pipeline_vertex','pipeline','offline_graphics','render_state','sampler_state','texture_buffer_sample']
 sources=[n/'src'/(name+'.c')for name in names];fixture=n/'tests/d2r_bridge/buffer_sampled.c';facade=n/'src/runtime_fdl.cc';backend=n/'src/kgsl.c';memory=n/'tests/d2r_bridge/memory_backend.c'
 host=Path(fdl['normal_ir3'])/'build';ninja=Path(shutil.which('ninja')).resolve();cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
 for p in [Path(__file__).resolve(),fixture,*sources,facade,backend,memory,ninja,cc.resolve(),host/'compile_commands.json',*sorted((n/'include/nvn_native').glob('*.h')),*sorted((n/'tests/d2r_bridge').glob('*.inc')),n/'third_party/kgsl/msm_kgsl.h',n/'third_party/nvn/nvn.h']:inputs[str(p)]=sha(p)
 out.mkdir(parents=True);commands=[];results=[]
 def run(argv,cwd=None):
  argv=list(map(str,argv));p=subprocess.run(argv,cwd=cwd,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));commands.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
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
 flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn')]
 defines=['-DNN_PIPELINE_BUFFER_SAMPLED=1','-DNN_MEMORY_BACKEND='+json.dumps(str(memory)),'-DNN_BUFFER_FETCH_ARTIFACT='+json.dumps(shaders['header'])]
 for compiler,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  folder=out/compiler;folder.mkdir();objects=[]
  for tool in (compiler,cpp):p=Path(shutil.which(tool)).resolve();inputs[str(p)]=sha(p)
  for i,s in enumerate([fixture,*sources]):
   o=folder/(str(i)+'.o');run([compiler,*flags,*defines,*san,'-fno-pie','-c',s,'-o',o]);objects.append(o)
  ff=hf if cpp=='g++'else[x for x in hf if x not in ('-flifetime-dse=1','-Wno-error=maybe-uninitialized')]
  o=folder/'fdl.o';run([cpp,*ff,'-I'+str(n/'include'),'-Werror',*san,'-fno-pie','-c',facade,'-o',o],host);objects.append(o)
  binary=folder/'buffer-sampled';run([cpp,'-no-pie',*san,*objects,*libs,'-o',binary],host);r=json.loads(run([binary]))
  if not r['passed']or r['gpu_execution']or r['encoded_jobs']!=180:raise ValueError('incomplete HOST actual nativeBUFFER fixture')
  results.append(r)
  # Default-OFF must reject realBUFFER views before allocating a job, using the
  # same immutable authored stage metadata and actual FDL fixtures.
  off_objects=[o for i,o in enumerate(objects)if i not in (0,4)]
  for i,src in ((0,fixture),(4,n/'src/pipeline.c')):
   obj=folder/('off'+str(i)+'.o');run([compiler,*flags,*[x for x in defines if x!='-DNN_PIPELINE_BUFFER_SAMPLED=1'],'-DNN_TEST_BUFFER_OFF=1',*san,'-fno-pie','-c',src,'-o',obj]);off_objects.append(obj)
  off_binary=folder/'buffer-sampled-off';run([cpp,'-no-pie',*san,*off_objects,*libs,'-o',off_binary],host);off_result=json.loads(run([off_binary]))
  if not off_result['passed']or off_result['encoded_jobs']!=0 or off_result['gpu_execution']:raise ValueError('default-OFF gate weakened')
 if results[0]!=results[1]:raise ValueError('two sanitizer outcomes differ')
 run([cc,*flags,*defines,fixture,*sources,library,'-lm','-Wl,--no-undefined','-o',out/'buffer-sampled-fixture-api29'])
 run([cc,*flags,'-DNN_PIPELINE_BUFFER_SAMPLED=1','-I'+str(n/'third_party/kgsl'),'-fPIC','-shared',*sources,backend,library,'-lm','-Wl,--no-undefined','-o',out/'buffer-sampled-real-kgsl-api29.so'])
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('input changed '+p)
 r=dict(schema=1,kind='native-buffer-sampled-host-v1',passed=True,gpu_execution=False,android_execution=False,scope=__doc__,results=results,inputs=inputs,commands=commands,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()});(out/'check.json').write_text(json.dumps(r,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','runtime-fdl-check','shader-check','ndk','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(check(a.root.resolve(),a.runtime_fdl_check.resolve(),a.shader_check.resolve(),a.ndk.resolve(),a.out.resolve()))

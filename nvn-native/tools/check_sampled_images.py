#!/usr/bin/env python3
"""HOST+Android-link sampled FLOAT2D plan/owner, actual FDL and program facts; no GPU."""
import argparse,hashlib,importlib.util,json,os,shlex,shutil,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,n):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def check(root,fdl_check,kernel_check,pipeline_check,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 native=root/'nvn-native';fdltool=module(native/'tools/build_runtime_fdl.py','sampled_fdl');library,fdl=fdltool.verify(root,fdl_check)
 sources=('compute','block_copy','texture_source','texture_bc_shadow','sampler_state','sampled_image','pipeline_program','offline_graphics')
 inputs={str(fdl_check):sha(fdl_check),**fdl['inputs'],**fdl['artifacts']}
 for checkpath,fields in [(kernel_check,('inputs','artifacts')),(pipeline_check,('inputs','outputs'))]:
  record=json.loads(checkpath.read_text());inputs[str(checkpath)]=sha(checkpath)
  for field in fields:inputs.update(record[field])
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('normal input/product changed '+p)
 kernel=json.loads(kernel_check.read_text());pipeline=json.loads(pipeline_check.read_text())
 if kernel.get('kind')!='native-general-block-copy-v1' or pipeline.get('kind')!='native-graphics-pipeline-recipes-v1':raise ValueError('unknown normal producer profile')
 module(native/'tools/compile_block_copy.py','sampled_kernel').verify(root,kernel_check)
 module(native/'tools/package_graphics_pipelines.py','sampled_pipeline').build(root,Path(pipeline['stage_check']),Path(pipeline['compiler_record']).parent,pipeline['gmem_bytes'],pipeline_check.parent,verify=True)
 out.mkdir(parents=True);commands=[];results=[]
 def run(argv,cwd=None):
  p=subprocess.run(list(map(str,argv)),cwd=cwd,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  commands.append(dict(argv=list(map(str,argv)),cwd=str(cwd)if cwd else None,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if p.returncode:raise ValueError((p.stdout+p.stderr)[-8000:])
  return p.stdout
 host=Path(fdl['normal_ir3'])/'build';ninja=Path(shutil.which('ninja')).resolve();cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
 archive=pipeline_check.parent/'offline_graphics_pipelines.h'
 for folder in ('include','src','tests/d2r_bridge','third_party/nvn'):
  for p in (native/folder).rglob('*'):
   if p.is_file()and p.suffix in('.h','.c','.inc','.cc'):inputs[str(p)]=sha(p)
 for p in (Path(__file__).resolve(),cc.resolve(),ninja,host/'compile_commands.json'):inputs[str(p)]=sha(p)
 entry=next(e for e in json.loads((host/'compile_commands.json').read_text())if e['file'].endswith('/fd6_view.cc'));_,hf=fdltool.flags(entry)
 command=shlex.split(run([ninja,'-C',host,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
 for v in command[1:]:
  if skip:skip=False;continue
  if v=='-o':skip=True;continue
  if not v.endswith('.o'):libs.append(v)
 for v in libs:
  p=(host/v).resolve()
  if p.is_file():inputs[str(p)]=sha(p)
 common=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(native/'third_party/nvn'),'-I'+str(native/'include'),'-I'+str(native/'include/nvn_native')]
 defines=['-DNN_SAMPLED_KERNEL='+json.dumps(kernel['header']),'-DNN_SAMPLED_ARCHIVE='+json.dumps(str(archive))]
 units=[native/'tests/d2r_bridge/sampled_image.c',*[native/'src'/(s+'.c')for s in sources]]
 for compiler,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  folder=out/compiler;folder.mkdir();objs=[]
  for tool in(compiler,cpp):p=Path(shutil.which(tool)).resolve();inputs[str(p)]=sha(p)
  for i,s in enumerate(units):
   o=folder/(str(i)+'.o');run([compiler,*common,*defines,*san,'-fno-pie','-c',s,'-o',o]);objs.append(o)
  ff=hf if cpp=='g++'else[x for x in hf if x not in('-flifetime-dse=1','-Wno-error=maybe-uninitialized')]
  o=folder/'fdl.o';run([cpp,*ff,'-I'+str(native/'include'),'-Werror',*san,'-fno-pie','-c',native/'src/runtime_fdl.cc','-o',o],host);objs.append(o)
  binary=folder/'sampled';run([cpp,'-no-pie',*san,*objs,*libs,'-o',binary],host);r=json.loads(run([binary]));results.append(r)
  if not r['passed']or r['formats']!=22 or r['owners']!=88 or r['normal_programs']<2 or r['CPU_pixel_maps']or r['live']or r['GPU_execution']:raise ValueError('incomplete sampled owner fixture')
 if results[0]!=results[1]:raise ValueError('HOST results differ')
 actual=[native/'src/kgsl.c',*[native/'src'/(s+'.c')for s in sources]]
 run([cc,*common,'-I'+str(native/'third_party/kgsl'),'-fPIC','-shared',*actual,library,'-Wl,--no-undefined','-landroid','-ldl','-lm','-o',out/'sampled-images-real-api29.so'])
 run([cc,*common,*defines,*units,library,'-Wl,--no-undefined','-landroid','-lm','-o',out/'sampled-images-fixture-api29'])
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('input changed during check '+p)
 proof=dict(schema=1,kind='sampled-image-float2d-foundation-host-v1',passed=True,gpu_execution=False,android_execution=False,default_enabled=False,results=results,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()})
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in('root','runtime-fdl-check','kernel-check','pipeline-check','ndk','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(check(a.root.resolve(),a.runtime_fdl_check.resolve(),a.kernel_check.resolve(),a.pipeline_check.resolve(),a.ndk.resolve(),a.out.resolve()))

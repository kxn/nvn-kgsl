#!/usr/bin/env python3
"""HOST public Call two-BridgeClear offset/order/owners/replay; original normal bridge and 4CS producers, no GPU execution."""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ['root','bridge-check','compute-check','ndk','out']:p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();root=a.root.resolve();n=root/'nvn-native';out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);proof=dict(passed=False,GPU_execution=False,CPU_shader_execution=False,inputs={},runs=[])
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));proof['runs'].append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'progress.json').write_text(json.dumps(proof,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stderr[-6000:]+r.stdout[-1000:])
  return r.stdout
 try:
  bridge=module(n/'tools/texture_bridge_bundle.py','normal_call_clear_bridge');bh,br=bridge.build(root,a.bridge_check.resolve());proof['inputs'].update(br['inputs']);proof['inputs'].update(br['artifacts']);proof['inputs'][str(a.bridge_check.resolve())]=sha(a.bridge_check)
  cache=module(n/'tools/offline_compute_cache.py','normal_call_clear_compute_cache');h,m,locked=cache.build(a.compute_check.resolve(),out/'compute-cache',root=root);proof['inputs'].update(m['inputs']);proof['inputs'].update(locked)
  for folder in ['src','include','android','third_party/nvn','third_party/kgsl','tests/d2r_bridge']:
   for f in(n/folder).rglob('*'):
    if f.is_file()and f.suffix in ['.c','.h','.inc','.py']:proof['inputs'][str(f)]=sha(f)
  proof['inputs'][str(Path(__file__).resolve())]=sha(__file__)
  common=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-DNN_NVN_BLOCK_COPY=1','-DNN_NVN_GRAPHICS_SOURCE_CACHE=1',*['-I'+str(n/f)for f in ['include','third_party/nvn','third_party/kgsl','src','tests']]]
  defs=['-DNN_COMPUTE_CORE='+json.dumps(str(n/'src/nvn.c')),'-DNN_COMPUTE_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c')),'-DNN_COMPUTE_CACHE_HEADER='+json.dumps(str(h)),'-DNN_CALL_COPY_BRIDGE_BUNDLE='+json.dumps(str(bh))]
  mods=[n/'src'/f'{v}.c'for v in ['compute','block_copy','graphics','external_image','order','render_state','sampler_state','counter','texture_source','texture_bridge','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries']]
  for mode in [0,1]:
   flags=[x for x in common if not x.startswith('-DNN_NVN_GRAPHICS_SOURCE_CACHE=')]+['-DNN_NVN_GRAPHICS_SOURCE_CACHE='+str(mode)]
   for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
    binary=out/(cc+'-graphics'+str(mode));run([cc,*flags,*defs,*san,n/'tests/d2r_bridge/command_copy_bridge.c',*mods,'-lm','-o',binary]);x=json.loads(run([binary]));assert x['passed']and x['BridgeClear_Call_operations']==2 and x['replays']==2 and not x['GPU_execution'];proof.setdefault('results',[]).append(dict(compiler=cc,graphics_source_cache=mode,**x))
   cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';proof['inputs'][str(cc.resolve())]=sha(cc.resolve());binary=out/('libbridge-call-api29-graphics'+str(mode)+'.so');run([cc,*flags,'-fPIC','-shared','-Wl,--no-undefined',n/'src/nvn.c',n/'src/kgsl.c',*mods,*[n/'android'/f'{m}.c'for m in ['window_session','window_android','present','present_pacer','window_display','ahb']],'-landroid','-ldl','-lm','-o',binary]);elf=run([cc.parent/'llvm-readelf','-h','-d',binary]);assert'AArch64'in elf and not any(x in elf.lower()for x in ['libvulkan','libegl','libgles']);proof.setdefault('NDK_real_KGSL',[]).append(dict(graphics_source_cache=mode,path=str(binary),sha256=sha(binary)))
  for f,h in proof['inputs'].items():
   if sha(f)!=h:raise ValueError('changed source '+f)
  proof['passed']=True
 except Exception as e:proof['error']=str(e)
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],error=proof.get('error'),proof=str(out/'check.json'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())

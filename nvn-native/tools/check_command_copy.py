#!/usr/bin/env python3
"""HOST actual public Call copy/native ownership; original normal array/volume catalogues, no GPU execution."""
import argparse,hashlib,importlib.util,json,os,subprocess,sys
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ['root','array-check','volume-check','compute-check','ndk','out']:p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();root=a.root.resolve();n=Path(__file__).resolve().parents[1];out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);proof=dict(schema=1,passed=False,scope=__doc__,GPU_execution=False,CPU_shader_execution=False,inputs={},runs=[])
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));proof['runs'].append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'progress.json').write_text(json.dumps(proof,indent=2)+'\n')
  if r.returncode:raise RuntimeError(r.stderr[-6000:]+r.stdout[-1000:])
  return r.stdout
 try:
  sys.path.insert(0,str(root/'nvn-native/tools'));bundles=[]
  for profile,name,check in [('array','public_copy_bundle',a.array_check),('volume','public_volume_copy_bundle',a.volume_check)]:
   h,record,locked=module(root/'nvn-native/tools'/f'{name}.py',f'nn_call_{name}').build(check.resolve(),out/profile,root=root);proof['inputs'].update(locked);bundles.append((profile,h,record));proof.setdefault('profiles',{})[profile]=dict(cases=len(record['cases']),normal_proof=str(out/profile/'check.json'))
  for folder in ['src','include','android','third_party/nvn','third_party/kgsl','tests/d2r_bridge']:
   for f in(n/folder).rglob('*'):
    if f.is_file()and f.suffix in ['.c','.h','.inc','.py']:proof['inputs'][str(f)]=sha(f)
  proof['inputs'][str(Path(__file__).resolve())]=sha(__file__)
  common=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-DNN_NVN_BLOCK_COPY=1','-DNN_NVN_GRAPHICS_SOURCE_CACHE=1',*['-I'+str(n/f)for f in ['include','third_party/nvn','third_party/kgsl','src','tests']]]
  mods=[n/'src'/f'{m}.c'for m in ['compute','block_copy','graphics','external_image','order','render_state','sampler_state','counter','texture_source','texture_bridge','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries']]
  defs=['-DNN_PUBLIC_PROBE_SOURCE='+json.dumps(str(n/'tests/public_copy_probe.c')),'-DNN_PUBLIC_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c')),'-DNN_PUBLIC_CORE_SOURCE='+json.dumps(str(n/'src/nvn.c'))]
  cache=module(root/'nvn-native/tools/offline_compute_cache.py','nn_call_copy_compute_normal_cache');h,manifest,locked=cache.build(a.compute_check.resolve(),out/'compute-cache',root=root);assert len(manifest['entries'])==4;proof['inputs'].update(manifest['inputs']);proof['inputs'].update(locked)
  compute_defs=['-DNN_COMPUTE_CORE='+json.dumps(str(n/'src/nvn.c')),'-DNN_COMPUTE_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c')),'-DNN_COMPUTE_CACHE_HEADER='+json.dumps(str(h))]
  for fixture in ['command_copy_cp','command_copy_compute','command_compute']:
   for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
    binary=out/(fixture+'-'+cc);run([cc,*common,*compute_defs,*san,n/'tests/d2r_bridge'/f'{fixture}.c',*mods,'-lm','-o',binary]);result=json.loads(run([binary]));assert result['passed']and not result['GPU_execution'];proof.setdefault('supplement',[]).append(dict(fixture=fixture,compiler=cc,**result))
  for profile,h,record in bundles:
   for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
    binary=out/(profile+'-'+cc);run([cc,*common,*defs,'-DNVN_PUBLIC_COPY_HEADER='+json.dumps(str(h)),*san,n/'tests/d2r_bridge/command_copy_catalogue.c',*mods,'-lm','-o',binary]);result=json.loads(run([binary]));assert result['passed']and result['Call_copy_cases']==len(record['cases'])and result['Call_replays']==len(record['cases'])*2 and not result['GPU_execution'];proof.setdefault('results',[]).append(dict(profile=profile,compiler=cc,**result))
  cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';proof['inputs'][str(cc.resolve())]=sha(cc.resolve())
  for mode in [0,1]:
   shared=out/f'libcall-copy-{mode}-api29.so';flags=[x for x in common if not x.startswith('-DNN_NVN_BLOCK_COPY')]+['-DNN_NVN_BLOCK_COPY='+str(mode)]
   run([cc,*flags,'-fPIC','-shared','-Wl,--no-undefined',n/'src/nvn.c',n/'src/kgsl.c',*mods,*[n/'android'/f'{m}.c'for m in ['window_session','window_android','present','present_pacer','window_display','ahb']],'-landroid','-ldl','-lm','-o',shared]);elf=run([cc.parent/'llvm-readelf','-h','-d',shared]);assert'AArch64'in elf and not any(x in elf.lower()for x in ['libvulkan','libegl','libgles']);proof.setdefault('artifacts',{})[str(shared)]=sha(shared)
  for f,h in proof['inputs'].items():
   if sha(f)!=h:raise RuntimeError('source changed '+f)
  proof['passed']=True
 except Exception as e:proof['error']=str(e)
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],error=proof.get('error'),proof=str(out/'check.json'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())

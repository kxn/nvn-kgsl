#!/usr/bin/env python3
"""HOST normal four-original typed compute Call projection; actual NDK strict link, no GPU/shader execution."""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','check','ndk','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();root=a.root.resolve();n=Path(__file__).resolve().parents[1];out=a.out.resolve()
 if out.exists() or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);proof=dict(schema=1,passed=False,scope=__doc__,GPU_execution=False,CPU_shader_execution=False,runs=[],inputs={})
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  proof['runs'].append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'progress.json').write_text(json.dumps(proof,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stderr[-6000:])
  return r.stdout
 try:
  builder=root/'nvn-native/tools/offline_compute_cache.py';spec=importlib.util.spec_from_file_location('nn_call_compute_normal_cache',builder);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  h,manifest,outputs=m.build(a.check.resolve(),out/'cache',root=root);proof['normal_cache_entries']=len(manifest['entries']);assert len(manifest['entries'])==4;proof['inputs'].update(manifest['inputs']);proof['inputs'].update(outputs)
  for folder in ['src','include','third_party/nvn','third_party/kgsl','android','tests/d2r_bridge']:
   for f in (n/folder).rglob('*'):
    if f.is_file():proof['inputs'][str(f)]=sha(f)
  proof['inputs'][str(Path(__file__).resolve())]=sha(__file__)
  common=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-DNN_NVN_GRAPHICS_SOURCE_CACHE=1',*['-I'+str(n/f)for f in ['include','third_party/nvn','third_party/kgsl','src','tests']]]
  defs=['-DNN_COMPUTE_CORE='+json.dumps(str(n/'src/nvn.c')),'-DNN_COMPUTE_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c')),'-DNN_COMPUTE_CACHE_HEADER='+json.dumps(str(h))]
  modules=[n/'src'/f'{m}.c'for m in ['compute','graphics','external_image','order','render_state','sampler_state','counter','texture_source','texture_bridge','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries']]
  for cc,flags in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
   binary=out/('compute-'+cc);run([cc,*common,*defs,*flags,n/'tests/d2r_bridge/command_compute.c',*modules,'-lm','-o',binary]);r=json.loads(run([binary]));assert r['passed'] and r['four_original_compute_cases']==8 and r['indirect_cases']==4 and r['inspected_rebuilt_jobs']==16 and not r['GPU_execution'];proof.setdefault('results',[]).append(r)
  cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';proof['inputs'][str(cc.resolve())]=sha(cc.resolve())
  run([cc,*common,*defs,'-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1',n/'tests/d2r_bridge/command_compute.c',*modules,'-lm','-o',out/'compute-api29-not-run'])
  shared=out/'libnvn-call-compute-api29.so';run([cc,*common,'-fPIC','-shared','-Wl,--no-undefined',n/'src/nvn.c',n/'src/kgsl.c',*modules,*[n/'android'/f'{m}.c'for m in ['window_session','window_android','present','present_pacer','window_display','ahb']],'-landroid','-ldl','-lm','-o',shared])
  elf=run([cc.parent/'llvm-readelf','-h','-d',shared]);assert'AArch64'in elf and not any(x in elf.lower()for x in ['libvulkan','libegl','libgles'])
  for f,h in proof['inputs'].items():
   if sha(f)!=h:raise ValueError('source changed '+f)
  proof['passed']=True
 except Exception as e:proof['error']=str(e)
 proof['artifacts']={str(p):sha(p)for p in out.iterdir()if p.is_file()and p.name not in ['check.json','progress.json']};(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],proof=str(out/'check.json'),error=proof.get('error'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())

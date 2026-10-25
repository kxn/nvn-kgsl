#!/usr/bin/env python3
"""HOST refusal, strict qualification metadata and normal Android shared link; GPU submits zero."""
import argparse,hashlib,importlib.util,json,os,subprocess,sys
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(name,p):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','check','ndk','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();root=a.root.resolve();n=Path(__file__).resolve().parents[1];out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);proof=dict(schema=1,passed=False,scope=__doc__,GPU_execution=False,GPU_submits=0,CPU_shader_execution=False,runs=[],inputs={})
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  proof['runs'].append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'progress.json').write_text(json.dumps(proof,indent=2)+'\n')
  if r.returncode:raise RuntimeError(r.stderr[-6000:]+r.stdout[-1000:])
  return r.stdout
 try:
  cache=load('nn_call_qualify_cache',root/'nvn-native/tools/offline_compute_cache.py');h,manifest,locked=cache.build(a.check.resolve(),out/'cache',root=root)
  assert len(manifest['entries'])==4 and manifest['chip_id']==0x06050002
  proof['inputs'].update(manifest['inputs']);proof['inputs'].update(locked);proof['normal_cache_entries']=len(manifest['entries'])
  bundle=load('nn_call_qualify_bundle',root/'nvn-native/adapters/d2r/compute_bundle.py');header,qualification,locked=bundle.build(a.check.resolve(),out/'compiled');proof['inputs'].update(locked);proof['qualification']=qualification
  for folder in ['src','include','android','third_party/nvn','third_party/kgsl','tests','tools','adapters']:
   for f in (n/folder).rglob('*'):
    if f.is_file()and f.suffix in ('.py','.c','.h','.inc','.txt'):proof['inputs'][str(f.resolve())]=sha(f)
  proof['inputs'][str(n/'CMakeLists.txt')]=sha(n/'CMakeLists.txt')
  common=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1','-DNVN_COMPUTE_CALL=1','-DNVN_MAXWELL_HEADER='+json.dumps(str(header)),*['-I'+str(n/f)for f in ['include','third_party/nvn','third_party/kgsl','src','tests']]]
  modules=[n/'src'/f'{m}.c'for m in ['nvn','compute','graphics','external_image','order','render_state','sampler_state','counter','texture_source','texture_bridge','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries']]
  test=[n/'tests/compute_call_probe_failure.c',n/'tests/maxwell_compute_indirect_probe.c',n/'tests/d2r_bridge/memory_backend.c',*modules]
  for cc,flags in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
   binary=out/('caller-'+cc);run([cc,*common,*flags,*test,'-lm','-o',binary]);text=run([binary]);assert'GPU ENOTSUP preserved'in text
  proof['metadata_test']=json.loads(run([sys.executable,n/'tests/test_compute_call_validation.py']));assert proof['metadata_test']['passed']and not proof['metadata_test']['GPU_execution']
  # Admission options are explicit; invalid Call-only combinations reject before shader packaging.
  for args in [['--compute-call'],['--compute-call','--maxwell-check',a.check]]:
   cmd=[sys.executable,n/'tools/build_android_probe.py','--sdk',a.ndk.resolve().parents[1],'--ndk',a.ndk,'--out',out/'invalid-options',*args]
   r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True);proof.setdefault('option_negatives',[]).append(dict(argv=list(map(str,cmd)),returncode=r.returncode,stderr=r.stderr));assert r.returncode!=0 and '--compute-call requires'in r.stderr
  cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';proof['inputs'][str(cc.resolve())]=sha(cc.resolve())
  run([cc,*common,*test,'-lm','-o',out/'caller-api29-not-run'])
  toolchain=a.ndk.resolve()/'build/cmake/android.toolchain.cmake';proof['inputs'][str(toolchain)]=sha(toolchain)
  run(['cmake','-S',n,'-B',out/'cmake','-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(toolchain),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DNVN_MAXWELL_HEADER='+str(header),'-DNVN_COMPUTE_INDIRECT=ON','-DNVN_COMPUTE_CALL=ON'])
  run(['cmake','--build',out/'cmake','--parallel','4']);shared=out/'cmake/libnvn_kgsl_probe.so';elf=run([cc.parent/'llvm-readelf','-h','-d',shared]);assert'AArch64'in elf and not any(x in elf.lower()for x in ['libvulkan','libegl','libgles'])
  for f,h in proof['inputs'].items():
   if sha(f)!=h:raise RuntimeError('source changed '+f)
  proof['artifacts']={str(shared):sha(shared)};proof['passed']=True
 except Exception as e:proof['error']=str(e)
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],proof=str(out/'check.json'),error=proof.get('error'),GPU_submits=0)));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())

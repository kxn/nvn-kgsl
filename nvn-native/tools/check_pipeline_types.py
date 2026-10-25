#!/usr/bin/env python3
"""HOST ONLY exact Maxwell/NIR type propagation and ARM64 full backend link."""
import argparse,copy,hashlib,json,os,subprocess
from pathlib import Path
from package_graphics_pipelines import build,checked,sha,texture_types

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','bundle','ndk','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();root=a.root.resolve();n=Path(__file__).resolve().parents[1];out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored --out required')
 out.mkdir(parents=True);inputs={};runs=[];proof=dict(schema=1,passed=False,gpu_execution=False,android_execution=False,runs=runs)
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  runs.append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'progress.json').write_text(json.dumps(runs,indent=2)+'\n')
  if r.returncode:raise ValueError('HOST/NDK failed '+r.stderr[-5000:])
  return r.stdout
 try:
  bpath=a.bundle.resolve();b=json.loads(bpath.read_text());inputs[str(bpath)]=sha(bpath)
  h=build(root,Path(b['stage_check']),Path(b['compiler_record']).parent,b['gmem_bytes'],bpath.parent,True);checked(b['inputs'],inputs);checked(b['outputs'],inputs)
  from graphics_pipeline_cache import build as provider
  _,manifest=provider(root,bpath)
  if not manifest['native_texture_types']:raise ValueError('provider lost native type table')
  proof['provider']=manifest
  negative=0
  for i,pair in enumerate(b['pairs']):
   prefix=bpath.parent/('pair-'+str(i));actual=json.loads(prefix.with_suffix('.link.json').read_text());r=json.loads(prefix.with_suffix('.pipeline.json').read_text());t=json.loads(prefix.with_suffix('.pipeline.types.json').read_text())
   if texture_types(t,r,actual)!=pair['native_texture_types']:raise ValueError('type proof metadata differs')
   corruptions=[dict(schema=True),dict(basis='source SPIR-V guess'),dict(dimensions=[0]*15),dict(texture_mask=t['texture_mask']^1),dict(code_bytes=[0,0]),dict(operations=[8]*16)]
   for fields in corruptions:
    bad=copy.deepcopy(t);bad.update(fields)
    try:texture_types(bad,r,actual)
    except (ValueError,KeyError,TypeError):negative+=1
    else:raise ValueError('invalid native facts accepted')
  modules=[n/'src'/f'{name}.c'for name in ('compute','graphics','external_image','order','render_state','sampler_state','texture_source','texture_bridge','counter','offline_graphics','pipeline_program','pipeline_shader','pipeline','pipeline_archive','pipeline_source','pipeline_resource_queries','pipeline_frontend','pipeline_vertex')]
  for folder in (n/'src',n/'include',n/'third_party/nvn',n/'third_party/kgsl'):
   for f in folder.rglob('*'):
    if f.is_file()and f.suffix in ('.c','.h','.inc'):inputs[str(f.resolve())]=sha(f)
  for f in [Path(__file__).resolve(),n/'tools/graphics_pipeline_cache.py',n/'tests/d2r_bridge/program_owner.c',n/'tests/d2r_bridge/pipeline_nvn_source.c',n/'tests/d2r_bridge/pipeline_typed_archive.c',n/'tests/d2r_bridge/pipeline_typed_registration.c',n/'tests/d2r_bridge/memory_backend.c',n/'tests/d2r_bridge/window_session_backend.inc']:inputs[str(f.resolve())]=sha(f)
  common=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),'-I'+str(n/'src')]
  defines=['-DNN_PROGRAM_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c')),'-DNN_PROGRAM_ARCHIVE='+json.dumps(str(h)),'-DNN_PIPELINE_ARCHIVE='+json.dumps(str(h)),'-DNN_TEST_NVN_SOURCE='+json.dumps(str(n/'src/nvn.c')),'-DNN_MEMORY_BACKEND_SOURCE='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c'))]
  for cc,flags in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all','-no-pie'])]:
   exe=out/('types-'+cc);run([cc,*common,*defines,*flags,n/'tests/d2r_bridge/pipeline_typed_archive.c',*[n/'src'/f'{m}.c'for m in ('pipeline_program','pipeline_archive','offline_graphics','pipeline_resource_queries','pipeline_frontend')],'-o',exe]);run([exe])
   for mode in (0,1):
    exe=out/f'registration-{cc}-{mode}';run([cc,*common,*defines,*flags,'-DNN_NVN_GRAPHICS_SOURCE_CACHE='+str(mode),n/'tests/d2r_bridge/pipeline_typed_registration.c',*modules,'-o',exe]);run([exe])
  cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';readelf=cc.parent/'llvm-readelf'
  for f in [cc.resolve(),readelf]:inputs[str(f)]=sha(f)
  # Full real backend link: no RAM backend or guest CPU execution in Android.
  lib=out/'libpipeline-types-api29.so'
  android=[n/'android'/f'{m}.c'for m in ('window_session','window_android','present','present_pacer','window_display','ahb')]
  for f in android+[n/'src/kgsl_probe.c']:inputs[str(f.resolve())]=sha(f)
  run([cc,*common,'-I'+str(n/'third_party/kgsl'),'-DNN_NVN_GRAPHICS_SOURCE_CACHE=1','-fPIC','-shared','-Wl,--no-undefined',n/'src/nvn.c',n/'src/kgsl.c',n/'src/kgsl_probe.c',*modules,*android,'-landroid','-ldl','-lm','-o',lib])
  proof['elf']=run([readelf,'-d','-Ws',lib])
  checked(inputs,{})
  proof.update(passed=True,inputs=inputs,producer_negative_cases=negative,native_texture_types=True,guest_3d_resources_enabled=False,scope='Actual Maxwell source and native NIR facts only; HOST value/ownership tests; ARM64 library link, no Android/GPU run',artifacts={str(f):sha(f)for f in out.iterdir()if f.is_file()})
 except Exception as e:proof.update(error=str(e),inputs=inputs)
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],proof=str(out/'check.json'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())

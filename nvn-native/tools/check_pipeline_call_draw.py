#!/usr/bin/env python3
"""HOST ONLY typed NVN dynamic Draw ownership; real API29 link, no GPU execution."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','bundle','native-views','names','ndk','out'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();root=a.root.resolve();native=Path(__file__).resolve().parents[1];dependency=root/'nvn-native';out=a.out.resolve()
 if out.exists() or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);proof=dict(schema=1,scope=__doc__,passed=False,gpu_submits=0,runs=[]);inputs={}
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  proof['runs'].append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
  if r.returncode:raise ValueError('command failed: '+r.stderr[-3000:])
  return r.stdout
 try:
  sys.path.insert(0,str(dependency/'tools'))
  from graphics_pipeline_cache import build as pipeline_build
  from texture_bridge_bundle import build as view_build
  header,manifest=pipeline_build(root,a.bundle);proof['pipeline_provider']=manifest;inputs.update(manifest['inputs'])
  texture_header,view_record=view_build(root,a.native_views.resolve());inputs.update(view_record['inputs']);inputs.update(view_record['artifacts']);inputs[str(a.native_views.resolve())]=sha(a.native_views)
  view_header=texture_header.parent/'texture_native_views.h'
  if inputs.get(str(view_header))!=sha(view_header):raise ValueError('FDL view absent from recorded outputs')
  names=a.names.resolve();record=names.parent/'names-record.json';nr=json.loads(record.read_text())
  if nr.get('generated_header_schema')!=2:raise ValueError('unknown names schema')
  for name,expected in nr['artifacts'].items():
   f=Path(name) if Path(name).is_absolute() else names.parent/name
   if sha(f)!=expected['sha256'] or f.stat().st_size!=expected['size']:raise ValueError('changed name artifact')
   inputs[str(f)]=sha(f)
  inputs[str(record)]=sha(record)
  for base in (native,dependency):
   for folder in ('src','include','android','third_party/nvn','third_party/kgsl'):
    for f in (base/folder).rglob('*'):
     if f.is_file() and f.suffix in ('.c','.h','.inc'):inputs[str(f.resolve())]=sha(f)
  fixtures=[native/'tests/d2r_bridge'/f for f in ('pipeline_frontend.c','pipeline_call_draw.c','pipeline_draw_memory.c')]
  for f in [*fixtures,native/'tests/d2r_bridge/pipeline_fixture_pair.h',Path(__file__),native/'CMakeLists.txt',native/'adapters/d2r/host_bridge.c',dependency/'tests/d2r_bridge/pipeline_nvn_source.c',dependency/'tests/d2r_bridge/program_owner.c',dependency/'tests/d2r_bridge/memory_backend.c']:
   inputs[str(f.resolve())]=sha(f)
  common=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(native/'include'),'-I'+str(dependency/'include'),'-I'+str(dependency/'third_party/nvn'),'-I'+str(dependency/'src'),'-iquote',str(dependency/'adapters/d2r')]
  modules=[dependency/'src'/f'{name}.c' for name in ('compute','graphics','external_image','order','render_state','sampler_state','texture_source','texture_bridge','counter','offline_graphics','pipeline_program','pipeline_shader','pipeline','pipeline_archive','pipeline_source','pipeline_resource_queries')]+[native/'src/pipeline_frontend.c',native/'src/pipeline_vertex.c']
  defs=['-DNN_TEST_NVN_SOURCE='+json.dumps(str(native/'src/nvn.c')),'-DNN_MEMORY_BACKEND_SOURCE='+json.dumps(str(native/'tests/d2r_bridge/pipeline_draw_memory.c')),'-DNN_REAL_MEMORY_BACKEND='+json.dumps(str(dependency/'tests/d2r_bridge/memory_backend.c')),'-DNN_SOURCE_FIXTURE='+json.dumps(str(dependency/'tests/d2r_bridge/pipeline_nvn_source.c')),'-DNN_PIPELINE_ARCHIVE='+json.dumps(str(header)),'-DNN_PIPELINE_NATIVE_VIEWS='+json.dumps(str(view_header)),'-DNN_PIPELINE_TEXTURE_BRIDGE='+json.dumps(str(texture_header)),'-DNN_NVN_GRAPHICS_SOURCE_CACHE=1']
  for cc,flags in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all'])]:
   binary=out/f'draw-{cc}';run([cc,*common,*defs,*flags,fixtures[1],*modules,'-o',binary]);text=run([binary])
   if 'source=0'not in text or 'source=1'not in text or text.count('mock_submits=3')!=4 or text.count('GPU execution=false')!=7 or text.count('inherited_UBO=8192 selected_handles=2 mock_submits=3')!=2 or text.count('First Call after ordinary source=')!=2 or text.count('control budget simple_draws=17 fit=16384 reject=256')!=1:raise ValueError('mock ownership fixture scope changed')
   front=out/f'frontend-{cc}';frontdefs=['-DNN_PROGRAM_OWNER_FIXTURE='+json.dumps(str(dependency/'tests/d2r_bridge/program_owner.c')),'-DNN_PROGRAM_MEMORY_BACKEND='+json.dumps(str(dependency/'tests/d2r_bridge/memory_backend.c')),'-DNN_PROGRAM_ARCHIVE='+json.dumps(str(header)),'-DNN_PIPELINE_ARCHIVE='+json.dumps(str(header))]
   run([cc,*common,*frontdefs,*flags,fixtures[0],*modules,'-o',front]);text=run([front])
   if 'shader/GPU execution=false'not in text:raise ValueError('frontend scope changed')
  cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';inputs[str(cc.resolve())]=sha(cc)
  run([cc,*common,*defs,'-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1',fixtures[1],*modules,'-o',out/'draw-api29-not-run'])
  # Real KGSL/Android full shared closure, excluding the memory/mock backend.
  android=[dependency/'android'/f'{name}.c' for name in ('window_session','window_android','present','present_pacer','window_display','ahb')]
  real=[native/'src/nvn.c',dependency/'src/kgsl.c',dependency/'src/kgsl_probe.c',*modules,*android]
  run([cc,*common,'-DNN_NVN_GRAPHICS_SOURCE_CACHE=1','-I'+str(dependency/'third_party/kgsl'),'-fPIC','-shared','-Wl,--no-undefined',*real,'-landroid','-ldl','-lm','-o',out/'libnvn-pipeline-draw-api29.so'])
  # Host bridge really includes the normal view artifact; no new host reader.
  obj=out/'bridge-api29.o';run([cc,*common,'-D_GNU_SOURCE','-DNN_D2R_HOST_HLE=1','-DNN_D2R_NVN_NAMES_HEADER='+json.dumps(str(names)),'-DNN_NVN_OFFLINE_GRAPHICS_PIPELINE_HEADER='+json.dumps(str(header)),'-DNN_NVN_PIPELINE_NATIVE_VIEWS_HEADER='+json.dumps(str(view_header)),'-c',native/'adapters/d2r/host_bridge.c','-o',obj])
  nm=cc.parent/'llvm-nm';inputs[str(nm.resolve())]=sha(nm);symbols=run([nm,'-u',obj])
  if 'nn_nvn_register_pipeline_views'not in symbols or 'dm_self_read'not in symbols:raise ValueError('normal bridge source/view registration absent')
  if any(sha(f)!=h for f,h in inputs.items()):raise ValueError('source/artifact changed during check')
  proof['passed']=True;proof['artifacts']={str(f):sha(f)for f in out.iterdir()if f.is_file()}
 except Exception as e:proof['error']=str(e)
 proof['inputs']=inputs;(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],proof=str(out/'check.json'),gpu_submits=0)));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())

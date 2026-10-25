#!/usr/bin/env python3
"""HOST ONLY typed NVN source-program cache; NDK objects/link are not GPU runs."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
from workspace_paths import d2r_root

def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True)
 p.add_argument('--bundle',type=Path,required=True);p.add_argument('--names',type=Path,required=True)
 p.add_argument('--ndk',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--expected-deferred-depth',type=int,default=0);a=p.parse_args()
 root=a.root.resolve();native=Path(__file__).resolve().parents[1];dependency=root/'nvn-native';out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 sys.path.append(str(dependency/'tools'));from graphics_pipeline_cache import build
 out.mkdir(parents=True);proof={'schema':1,'scope':__doc__,'passed':False,'gpu_submits':0,'runs':[]};inputs={};runs=proof['runs']
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  runs.append({'argv':argv,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
  if r.returncode:raise ValueError('command failed: '+r.stderr[-3000:])
  return r.stdout
 try:
  header,manifest=build(root,a.bundle);inputs.update(manifest['inputs']);proof['provider']=manifest
  catalogue=json.loads(a.bundle.read_text())
  if a.expected_deferred_depth<0 or len(catalogue.get('deferred_pairs',[]))!=a.expected_deferred_depth:raise ValueError('unexpected deferred depth count')
  proof['expected_deferred_depth']=a.expected_deferred_depth
  names=a.names.resolve();record=names.parent/'names-record.json';n=json.loads(record.read_text())
  if n['generated_header_schema']!=2:raise ValueError('unknown typed names schema')
  for name,expected in n['artifacts'].items():
   f=Path(name)if Path(name).is_absolute()else names.parent/name
   if not f.is_file()or sha(f)!=expected['sha256']or f.stat().st_size!=expected['size']:raise ValueError('changed name artifact')
   inputs[str(f)]=sha(f)
  inputs[str(record)]=sha(record)
  observer=native/'tests/d2r_bridge/present_interval_observer.c'
  fixture=native/'tests/d2r_bridge/pipeline_nvn_source.c';core=native/'src/nvn.c';bridge=native/'adapters/d2r/host_bridge.c';backend=dependency/'tests/d2r_bridge/memory_backend.c'
  modules=[dependency/'src'/f'{name}.c'for name in ('compute','graphics','external_image','order','render_state','sampler_state','texture_source','texture_bridge','counter','offline_graphics','pipeline_program','pipeline_shader','pipeline','pipeline_archive','pipeline_source','pipeline_resource_queries')]+[native/'src/pipeline_frontend.c',native/'src/pipeline_vertex.c']
  for folder in (native/'src',native/'include',native/'tools',dependency/'src',dependency/'include',dependency/'third_party/nvn',dependency/'third_party/kgsl'):
   for f in folder.rglob('*'):
    if f.is_file()and f.suffix in ('.c','.h','.inc','.py'):inputs[str(f.resolve())]=sha(f)
  for f in (Path(__file__).resolve(),fixture,observer,bridge,backend,native/'CMakeLists.txt',native/'tests/d2r_bridge/pipeline_source_options.py',dependency/'tests/d2r_bridge/window_session_backend.inc',d2r_root()/'dm/host/platform.h',d2r_root()/'dm/host/android/platform_elf.c'):
   inputs[str(f.resolve())]=sha(f)
  common=['-I'+str(d2r_root()),'-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(native/'include'),'-I'+str(dependency/'include'),'-I'+str(dependency/'third_party/nvn'),'-I'+str(dependency/'src'),'-iquote',str(dependency/'adapters/d2r')]
  defines=['-DNN_TEST_NVN_SOURCE='+json.dumps(str(core)),'-DNN_MEMORY_BACKEND_SOURCE='+json.dumps(str(backend)),'-DNN_PIPELINE_ARCHIVE='+json.dumps(str(header)),'-DNN_EXPECT_DEFERRED_DEPTH='+str(a.expected_deferred_depth)]
  for cc,flags in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all'])]:
   observer_binary=out/f'present-observer-{cc}'
   run([cc,*common,*flags,'-DNN_D2R_NVN_NAMES_HEADER='+json.dumps(str(names)),observer,bridge,'-o',observer_binary])
   observation=json.loads(run([observer_binary]).strip().splitlines()[-1])
   if observation.get('gpu_executes')!=0 or observation.get('checks')!=44:
    raise ValueError('typed present observer result/scope changed')
   for mode in (0,1):
    binary=out/f'core-{cc}-{mode}';run([cc,*common,*defines,'-DNN_NVN_GRAPHICS_SOURCE_CACHE='+str(mode),*flags,fixture,*modules,'-o',binary]);text=run([binary])
    if 'GPU submits=0'not in text or ('nonempty Draw unsupported=true'not in text if mode else 'disabled'not in text):raise ValueError('fixture scope/result changed')
  ndk=a.ndk.resolve();cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';nm=cc.parent/'llvm-nm'
  inputs[str(cc.resolve())]=sha(cc);inputs[str(nm.resolve())]=sha(nm)
  for mode in (0,1):
   run([cc,*common,*defines,'-DNN_NVN_GRAPHICS_SOURCE_CACHE='+str(mode),'-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1',fixture,*modules,'-o',out/f'core-api29-{mode}'])
  for mode in (0,1):
   obj=out/f'bridge-api29-{mode}.o';options=[]if not mode else['-D_GNU_SOURCE','-DNN_D2R_HOST_HLE=1','-DNN_NVN_OFFLINE_GRAPHICS_PIPELINE_HEADER='+json.dumps(str(header))]
   run([cc,*common,'-DNN_D2R_NVN_NAMES_HEADER='+json.dumps(str(names)),*options,'-c',bridge,'-o',obj]);symbols=run([nm,'-u',obj])
   if mode and ('dm_self_read'not in symbols or ('nn_nvn_register_typed_pipeline_archive' if manifest.get('native_texture_types') else 'nn_nvn_register_pipeline_archive')not in symbols):raise ValueError('production provider absent')
   if not mode and ('dm_self_read'in symbols or 'nn_nvn_register_pipeline_archive'in symbols or 'nn_nvn_register_typed_pipeline_archive'in symbols):raise ValueError('default off gained source provider dependency')
  run([sys.executable,native/'tests/d2r_bridge/pipeline_source_options.py','--root',root,'--native',native,'--check',a.bundle,'--out',out/'option-tests'])
  for name in ('graphics_pipeline_cache.py','check_pipeline_source.py','build_d2r_activity.py','build_d2r_host.py','build_d2r_activity_apk.py','run_d2r_host.py'):run([sys.executable,'-m','py_compile',native/'tools'/name])
  if any(sha(path)!=value for path,value in inputs.items()):raise ValueError('input changed during qualification')
  proof['inputs']=inputs;proof['passed']=True;proof['artifacts']={str(f):sha(f)for f in out.iterdir()if f.is_file()}
 except Exception as e:proof['error']=str(e);proof['inputs']=inputs
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps({'passed':proof['passed'],'proof':str(out/'check.json'),'gpu_submits':0}));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())

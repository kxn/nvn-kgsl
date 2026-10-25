#!/usr/bin/env python3
"""HOST ONLY bridge/core input observation tests; NDK compilation is not device execution."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from workspace_paths import d2r_root

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--names',type=Path,required=True);p.add_argument('--ndk',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
 p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2]);p.add_argument('--fixtures',type=Path)
 p.add_argument('--bridge-source',type=Path);p.add_argument('--core-source',type=Path);p.add_argument('--include-dir',type=Path)
 a=p.parse_args();root=a.root.resolve();native=root/'nvn-native';out=a.out.resolve();fixtures=(a.fixtures or native/'tests/d2r_bridge').resolve()
 bridge=(a.bridge_source or native/'adapters/d2r/host_bridge.c').resolve();core=(a.core_source or native/'src/nvn.c').resolve();include=(a.include_dir or native/'include').resolve();names=a.names.resolve()
 if not out.is_relative_to(root/'.local') or out.exists():p.error('--out must be new and below ROOT/.local')
 out.mkdir(parents=True);proof={'scope':__doc__,'passed':False,'runs':[],'gpu_submits':0}
 sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 def run(label,argv):
  argv=list(map(str,argv));r=subprocess.run(argv,cwd=root,env=env,text=True,capture_output=True)
  proof['runs'].append({'label':label,'argv':argv,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
  if r.returncode:raise RuntimeError(label+' failed')
  return r.stdout
 try:
  record=json.loads((names.parent/'names-record.json').read_text());assert record['generated_header_schema']==2
  assert 'PFNNVNPROGRAMSETSHADERSPROC' in names.read_text()
  for filename,expected in record['artifacts'].items():
   f=names.parent/filename if not Path(filename).is_absolute() else Path(filename)
   if not f.is_file() or sha(f)!=expected['sha256'] or f.stat().st_size!=expected['size']:raise ValueError('generated names artifact mismatch: '+str(f))
  sources=[bridge,core,include/'nvn_native/nvn.h',native/'adapters/d2r/host_bridge.h',native/'third_party/nvn/nvn.h',names,names.parent/'names-record.json',Path(__file__).resolve(),d2r_root()/'dm/host/platform.h',d2r_root()/'dm/host/android/platform_elf.c',native/'src/nvn_entries.inc']
  for name in ['program_observations.c','program_safe_observations.c','program_source_info.c','program_reader.h','texture_binding_observations.c','copy_observations.c']:sources.append(fixtures/name)
  modules=[native/'src'/f for f in ['compute.c','graphics.c','external_image.c','order.c','render_state.c','sampler_state.c','texture_source.c','texture_bridge.c','counter.c']]
  backend=native/'tests/d2r_bridge/memory_backend.c';sources+=modules+[backend,native/'tests/d2r_bridge/window_session_backend.inc']
  sources+=list((native/'include/nvn_native').glob('*.h'))
  sources += [native/'adapters/d2r/host/CMakeLists.txt',native/'adapters/d2r/activity/CMakeLists.txt']
  proof['sources']={str(f):sha(f) for f in sources}
  base=['-I'+str(d2r_root()),'-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(include),'-I'+str(native/'include'),'-iquote',str(native/'include/nvn_native'),'-iquote',str(native/'src'),'-I'+str(native/'third_party/nvn'),'-I'+str(native/'adapters/d2r'),'-DNN_D2R_NVN_NAMES_HEADER="'+str(names)+'"']
  safe=['-DNN_D2R_PROGRAM_INPUT_READER=nn_program_mock_read','-include',str(fixtures/'program_reader.h')]
  for cc,flags in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
   for kind,inputs,options in [('default-off',[bridge,fixtures/'program_observations.c'],[]),('reader-on',[bridge,fixtures/'program_safe_observations.c'],safe),('core-source',[core,fixtures/'program_source_info.c',*modules,backend],[]),('texture-binding-default',[bridge,fixtures/'texture_binding_observations.c'],[]),('texture-binding-reader',[bridge,fixtures/'texture_binding_observations.c'],[*safe,'-DNN_TEST_BINDING_READER=1']),('copy-default',[bridge,fixtures/'copy_observations.c'],[]),('copy-reader',[bridge,fixtures/'copy_observations.c'],[*safe,'-DNN_TEST_BINDING_READER=1'])]:
    exe=out/(kind+'-'+cc);run(kind+' '+cc+' compile',[cc,*base,*flags,*options,*inputs,'-o',exe]);run(kind+' '+cc+' execute',[exe])
  ndk=a.ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
  for kind,options in [('default-off',[]),('reader-on',safe),('production-host',['-D_GNU_SOURCE','-DNN_D2R_HOST_HLE=1','-DNN_D2R_PROGRAM_INPUT_OBSERVE=1'])]:
   obj=out/(kind+'.arm64.o');run('NDK '+kind+' compile',[ndk,*base,*options,'-c',bridge,'-o',obj])
   symbols=run('NDK '+kind+' symbols',[a.ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm','-u',obj])
   if kind=='default-off':assert 'dm_self_read' not in symbols and 'nn_nvn_get_program_source_info' not in symbols
   if kind=='production-host':assert 'dm_self_read' in symbols and 'nn_nvn_get_program_source_info' in symbols
  run('NDK core-source strict link only',[ndk,*base,'-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1',core,fixtures/'program_source_info.c',*modules,backend,'-o',out/'core-source.arm64'])
  proof['sources_unchanged']=all(sha(Path(f))==v for f,v in proof['sources'].items());assert proof['sources_unchanged']
  proof['passed']=True
 except Exception as e:proof['error']=str(e)
 finally:(out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n')
 print(json.dumps({'passed':proof['passed'],'scope':'HOST ONLY; NDK compile/link only','proof':str(out/'proof.json')}));return 0 if proof['passed'] else 1
if __name__=='__main__':raise SystemExit(main())

#!/usr/bin/env python3
"""HOST RAM ONLY: tracked descriptor CPU-role regression. No GPU or guest execution."""
import argparse, hashlib, importlib.util, json, os, subprocess
from pathlib import Path

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--root',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
 ap.add_argument('--ndk-clang',type=Path,required=True);ap.add_argument('--source',type=Path,help='default: tests/d2r_bridge/descriptor_role.c');ap.add_argument('--texture-check',type=Path,required=True);ap.add_argument('--color-check',type=Path,required=True);a=ap.parse_args()
 root=a.root.resolve();out=a.out.resolve();source=(a.source or root/'nvn-native/tests/d2r_bridge/descriptor_role.c').resolve();ndk=a.ndk_clang.resolve()
 if not out.is_relative_to(root/'.local') or out.exists():ap.error('new ignored --out below ROOT/.local required')
 out.mkdir(parents=True);proof={'scope':__doc__,'passed':False,'runs':[],'gpu_submits':0}
 files=[source,Path(__file__).resolve(),root/'nvn-native/src/nvn.c',root/'nvn-native/src/nvn_entries.inc',root/'nvn-native/third_party/nvn/nvn.h']
 files+=list((root/'nvn-native/include/nvn_native').glob('*.h'))
 files+=list((root/'nvn-native/src').glob('*.c'))+list((root/'nvn-native/src').glob('*.inc'))
 files+=[root/('nvn-native/'+p) for p in ['src/graphics.c','src/compute.c','src/external_image.c','tests/d2r_bridge/memory_backend.c','tests/d2r_bridge/window_session_backend.inc','tests/d2r_bridge/device_modes.c','adapters/d2r/graphics_bundle.py','adapters/d2r/texture_bundle.py']]
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 def run(label,cmd):
  r=subprocess.run(cmd,cwd=root,capture_output=True,text=True,env=env);row={'label':label,'argv':cmd,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr};proof['runs'].append(row)
  if r.returncode:raise RuntimeError(label+' failed')
  return r.stdout
 try:
  proof['source_sha256']={str(p):sha(p) for p in files}
  headers={};proof['checked_inputs']={}
  for name,check in [('texture',a.texture_check),('color',a.color_check)]:
   m=load(root/('nvn-native/adapters/d2r/'+('texture_bundle.py' if name=='texture' else 'graphics_bundle.py')),'descriptor_role_'+name)
   header,data,hashes=m.build(check.resolve(),out/name)
   if data.get('modes') is not True or data['program']['chip_id']!=0x06050002:raise ValueError('explicit schema3 A650 required')
   headers[name]=header;proof['checked_inputs'][name]={'check':str(check.resolve()),'sha256':sha(check),'hashes':hashes}
  common=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(root/'nvn-native/include'),'-I'+str(root/'nvn-native/third_party/nvn'),'-iquote',str(root/'nvn-native/tests/d2r_bridge'),'-DNN_TEST_NVN_SOURCE="'+str(root/'nvn-native/src/nvn.c')+'"','-DNN_MEMORY_BACKEND_SOURCE="'+str(root/'nvn-native/tests/d2r_bridge/memory_backend.c')+'"']
  modules=[str(root/('nvn-native/src/'+f)) for f in ['compute.c','graphics.c','external_image.c','order.c','render_state.c','sampler_state.c','texture_source.c','texture_bridge.c','counter.c']]
  for cc,flags in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all'])]:
   for group in ['role12','role14','modes_texture','modes_color']:
    binary=out/(group+'-'+cc);args=[cc,*common,*flags];is_role=group.startswith('role')
    if is_role:
     num=12 if group.endswith('12') else 14;args+=['-DNN_DESCRIPTOR_FLAGS'+'=0x'+str(num)]
    args+=['-DNN_MODE_IMAGE_HEADER="'+str(headers['color' if group.endswith('color') else 'texture'])+'"']
    fixture=source if is_role else root/'nvn-native/tests/d2r_bridge/device_modes.c'
    args+=[str(fixture),*modules]
    if not is_role:args+=[str(root/'nvn-native/tests/d2r_bridge/memory_backend.c')]
    run(group+' '+cc+' compile',args+['-o',str(binary)])
    result=json.loads(run(group+' '+cc+' run',[str(binary)]));proof['runs'][-1]['result']=result;proof['runs'][-1]['binary_sha256']=sha(binary)
    if result.get('gpu_submits')!=0:raise RuntimeError('unexpected GPU submit claim')
    if is_role and (result['checks']!=(387 if num==14 else 384) or result['host_submits']!=2 or result['private_descriptor_bases']!=12 or result['policy_rejections']!=10):raise RuntimeError('role result contract changed')
  if not ndk.is_file():raise ValueError('--ndk-clang is not a compiler file')
  for num in [12,14]:
   binary=out/('role'+str(num)+'-android-arm64')
   run('NDK '+str(num)+' compile/link only',[str(ndk),*common,'-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1','-DNN_DESCRIPTOR_FLAGS=0x'+str(num),'-DNN_MODE_IMAGE_HEADER="'+str(headers['texture'])+'"',str(source),*modules,'-o',str(binary)])
   proof['runs'][-1]['binary_sha256']=sha(binary)
  proof['source_unchanged']=all(sha(Path(p))==v for p,v in proof['source_sha256'].items())
  if not proof['source_unchanged']:raise RuntimeError('source changed during run')
  proof['passed']=True
 except Exception as e:proof['error']=str(e)
 finally:(out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n')
 print(json.dumps({'passed':proof['passed'],'scope':'HOST RAM ONLY','proof':str(out/'proof.json')}));return 0 if proof['passed'] else 1
if __name__=='__main__':raise SystemExit(main())

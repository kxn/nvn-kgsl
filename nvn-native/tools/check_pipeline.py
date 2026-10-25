#!/usr/bin/env python3
"""HOST ONLY pipeline packet/query/ownership tests plus actual ARM64 KGSL link.

Generated stage/recipe and FDL headers must stay ignored. This never runs GPU,
registers NVN programs, or treats its HOST query mock as actual guest state.
"""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
from package_graphics_pipelines import build as verify_package,checked,sha

MODULES=('pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source')
FIXTURES=('program_owner','shader_recipe','dynamic_pipeline','archive_select')

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--root',type=Path,required=True);p.add_argument('--bundle',type=Path)
 p.add_argument('--native-views',type=Path);p.add_argument('--ndk',type=Path)
 p.add_argument('--out',type=Path);p.add_argument('--names',action='store_true');a=p.parse_args()
 root=a.root.resolve();native=Path(__file__).resolve().parents[1];dependency=root/'nvn-native'
 own=[native/'src'/f'{m}.c' for m in MODULES]+sorted((native/'include/nvn_native').glob('pipeline*.h'))+[native/'include/nvn_native/texture_native_view.h']
 tests=[native/'tests/d2r_bridge'/f'{m}.c'for m in FIXTURES]
 helpers=[dependency/'src'/f'{m}.c'for m in ('offline_graphics','render_state','sampler_state')]
 memory=dependency/'tests/d2r_bridge/memory_backend.c'
 if a.names:
  print(json.dumps([str(x) for x in own+tests+helpers+[memory]],indent=2));return
 if not all((a.bundle,a.native_views,a.ndk,a.out)):p.error('--bundle --native-views --ndk --out required')
 out=a.out.resolve()
 if not out.is_relative_to(root/'.local') or out.exists():p.error('fresh ignored --out required')
 out.mkdir(parents=True);inputs={str(Path(__file__).resolve()):sha(__file__)};commands=[]
 try:
  bpath=a.bundle.resolve();b=json.loads(bpath.read_text());inputs[str(bpath)]=sha(bpath)
  bundle=verify_package(root,Path(b['stage_check']),Path(b['compiler_record']).parent,b['gmem_bytes'],bpath.parent,True)
  checked(b['inputs'],inputs);checked(b['outputs'],inputs)
  vpath=a.native_views.resolve();views=json.loads(vpath.read_text());inputs[str(vpath)]=sha(vpath)
  if views.get('kind')!='native-texture-consumer-bridge-v1':raise ValueError('unknown FDL view profile')
  checked(views['inputs'],inputs);checked(views['artifacts'],inputs)
  candidates=[Path(x)for x in views['artifacts'] if Path(x).name=='texture_native_views.h']
  if len(candidates)!=1:raise ValueError('missing unique actual FDL native view header')
  view_header=candidates[0]
  if [(x['width'],x['height'])for x in views['views'][:3]]!=[(64,64),(2,2),(2880,1620)]:raise ValueError('HOST fixture expects 64x64/2x2/2880x1620 FDL views, in that order')
  for f in own+tests+helpers+[memory,dependency/'src/kgsl.c']:
   inputs[str(f.resolve())]=sha(f)
  for directory in (dependency/'include',dependency/'third_party/kgsl',dependency/'third_party/nvn'):
   for f in directory.rglob('*'):
    if f.is_file():inputs[str(f.resolve())]=sha(f)
  common=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(native/'include'),'-I'+str(dependency/'include'),'-I'+str(dependency/'third_party/nvn')]
  defines=['-DNN_PROGRAM_MEMORY_BACKEND='+json.dumps(str(memory)),'-DNN_PROGRAM_ARCHIVE='+json.dumps(str(bundle)),'-DNN_PIPELINE_ARCHIVE='+json.dumps(str(bundle)),'-DNN_PIPELINE_NATIVE_VIEWS='+json.dumps(str(view_header))]
  sources=[str(native/'src'/f'{m}.c')for m in MODULES]+list(map(str,helpers))
  def run(argv):
   r=subprocess.run(list(map(str,argv)),capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1'))
   commands.append({'argv':list(map(str,argv)),'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
   if r.returncode:raise ValueError('HOST/NDK command failed: '+r.stderr[-3000:])
   return r.stdout
  results=[]
  for cc,flags in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
   # Dynamic fixture includes owner and shader fixtures, avoiding redundant runs.
   for name in ('dynamic_pipeline','archive_select'):
    binary=out/(name+'-'+cc)
    run([cc,*common,*defines,str(native/'tests/d2r_bridge'/f'{name}.c'),*sources,*flags,'-o',binary])
    text=run([binary]);results.append({'compiler':cc,'fixture':name,'stdout':text,'gpu_execution':False})
  cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
  inputs[str(cc.resolve())]=sha(cc)
  run([cc,*common,'-I'+str(dependency/'third_party/kgsl'),'-fPIC','-shared','-Wl,--no-undefined',*sources,str(dependency/'src/kgsl.c'),'-o',out/'libpipeline-api29.so'])
  checked(inputs,{})
  result={'schema':1,'scope':'HOST ONLY dynamic native pipeline; real ARM64 KGSL link; no NVN integration, no GPU execution','passed':True,'gpu_execution':False,'nvn_runtime_registered':False,'inputs':inputs,'commands':commands,'results':results,'artifacts':{str(f):sha(f)for f in out.iterdir()if f.is_file()}}
 except Exception as e:
  result={'schema':1,'scope':'HOST ONLY pipeline validation','passed':False,'error':str(e),'gpu_execution':False,'inputs':inputs,'commands':commands}
  (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');raise
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');print(out/'check.json')
if __name__=='__main__':main()

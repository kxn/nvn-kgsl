#!/usr/bin/env python3
"""HOST/API29 actual native FACE Program, relocation and readonly/busy ownership. No GPU."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def check(root,artifact,native_views,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 n=Path(__file__).resolve().parents[1];c=json.loads(artifact.read_text());inputs={str(artifact):sha(artifact)}
 if c.get('kind')!='native-authored-front-facing-v1'or not c.get('passed')or c.get('gpu_execution')is not False:raise ValueError('normal authored artifact required')
 for group in('inputs','artifacts'):
  for p,h in c[group].items():
   if sha(p)!=h:raise ValueError('artifact/source changed '+p)
   inputs[p]=h
 v=json.loads(native_views.read_text());inputs[str(native_views)]=sha(native_views)
 if v.get('kind')!='native-texture-consumer-bridge-v1':raise ValueError('actual normal FDL views required')
 for group in('inputs','artifacts'):
  for p,h in v[group].items():
   if sha(p)!=h:raise ValueError('FDL source/artifact changed '+p)
   inputs[p]=h
 headers=[p for p in v['artifacts']if Path(p).name=='texture_native_views.h']
 if len(headers)!=1 or(v['views'][0]['width'],v['views'][0]['height'],v['views'][0]['format'])!=(64,64,37):raise ValueError('actual RGBA8 64x64 FDL fixture required')
 sources=[n/'src'/(s+'.c')for s in('pipeline_program','pipeline_shader','offline_graphics','pipeline_vertex','pipeline','sampler_state','render_state')];fixture=n/'tests/d2r_bridge/front_facing_program.c'
 for p in[Path(__file__).resolve(),fixture,*sources,n/'tests/d2r_bridge/memory_backend.c',n/'tests/d2r_bridge/window_session_backend.inc',n/'src/kgsl.c',*[p for p in(n/'include/nvn_native').glob('*.h')]]:inputs[str(p)]=sha(p)
 out.mkdir(parents=True);commands=[]
 def run(argv):
  argv=list(map(str,argv));r=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));commands.append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stdout+r.stderr)
  return r.stdout
 base=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn')];defs=['-DNN_PIPELINE_FRONT_FACING=1','-DNN_FRONT_NATIVE_VIEWS='+json.dumps(headers[0]),'-DNN_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c')),'-DNN_FRONT_NATIVE_ARTIFACT='+json.dumps(c['header'])];results=[]
 for cc,san in[('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
  binary=out/cc;run([cc,*base,*san,'-fno-pie','-no-pie',*defs,fixture,*sources,'-Wl,--gc-sections','-lm','-o',binary]);results.append(json.loads(run([binary])))
 if results[0]!=results[1]:raise ValueError('sanitizer owner results differ')
 arm=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';run([arm,*base,'-fPIC','-shared',*sources,n/'src/kgsl.c','-I'+str(n/'third_party/kgsl'),'-Wl,--no-undefined','-landroid','-lm','-o',out/'libfront-program-real-kgsl-api29.so'])
 run([arm,*base,'-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1',*defs,fixture,*sources,'-Wl,--gc-sections','-Wl,--no-undefined','-landroid','-lm','-o',out/'front-program-host-fixture-api29-not-executed'])
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('source changed '+p)
 result=dict(schema=1,kind='native-front-facing-program-host-v1',passed=True,gpu_execution=False,android_execution=False,host=results,actual_KGSL_API29_link=True,inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});(out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in('root','artifact','native-views','ndk','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(check(a.root.resolve(),a.artifact.resolve(),a.native_views.resolve(),a.ndk.resolve(),a.out.resolve()))

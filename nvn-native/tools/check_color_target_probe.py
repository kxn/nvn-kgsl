#!/usr/bin/env python3
"""HOST ONLY COLOR packet/default equality + oracle negatives; actual API29 link."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
from color_target_qualification import build,sha

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for name in('root','check','ndk','out'):p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();root=a.root.resolve();native=root/'nvn-native';out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);inputs={};runs=[];proof={'schema':1,'scope':__doc__,'passed':False,'gpu_execution':False}
 def run(argv,cwd=None):
  r=subprocess.run(list(map(str,argv)),cwd=cwd,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  runs.append({'argv':list(map(str,argv)),'cwd':str(cwd)if cwd else None,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
  if r.returncode:raise ValueError('check failed\n'+r.stderr[-5000:])
  return r
 try:
  header,c,inputs=build(a.check.resolve(),out,root=root)
  test=native/'tests/test_color_target_validation.py';inputs[str(test)]=sha(test)
  oracle=json.loads(run([sys.executable,test,a.check.resolve()]).stdout)
  if oracle.get('negative_cases')!=23 or oracle.get('gpu_execution')is not False:raise ValueError('validator fixture scope')
  fdl=json.loads(Path(c['runtime_fdl_check']).read_text());commands=json.loads((Path(c['runtime_fdl_check']).parent/'commands.json').read_text())
  sources=[native/'src'/(n+'.c')for n in('offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','render_state','sampler_state','color_target')]
  fixture=native/'tests/d2r_bridge/color_target_packets.c';pipeline=native/'src/pipeline.c'
  for q in [Path(__file__).resolve(),fixture,pipeline,native/'tests/d2r_bridge/program_owner.c',native/'tests/d2r_bridge/memory_backend.c',*sources]:inputs[str(q)]=sha(q)
  for directory in(native/'include',native/'third_party/nvn'):
   for q in directory.rglob('*.h'):inputs[str(q)]=sha(q)
  common=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(native/'include'),'-I'+str(native/'third_party/nvn')]
  defs=['-DNN_PROGRAM_MEMORY_BACKEND='+json.dumps(str(native/'tests/d2r_bridge/memory_backend.c')),'-DNN_PROGRAM_ARCHIVE='+json.dumps(str(header)),'-DNN_COLOR_QUALIFICATION_HEADER='+json.dumps(str(header))]
  results=[]
  for cc,cpp,san in [('clang','clang++',['-fsanitize=function','-fsanitize-trap=function']),('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all'])]:
   objects=[]
   for i,q in enumerate([fixture,*sources,pipeline,pipeline]):
    old=i==len(sources)+2;extra=['-DNN_PIPELINE_COLOR_TARGETS='+('0'if old else'1')]
    if old:extra+=['-Dnn_pipeline_encode=nn_pipeline_encode_legacy','-Dnn_pipeline_preflight=nn_pipeline_preflight_legacy','-Dnn_pipeline_full_raster=nn_pipeline_full_raster_legacy']
    dest=out/(cc+str(i)+'.o');run([cc,*common,*defs,*extra,*san,'-fno-pie','-c',q,'-o',dest]);objects.append(dest)
   template=next(v for v in commands if v['argv'][0]==cpp and str(native/'tests/runtime_fdl.cc')in v['argv'])
   argv=[]
   for word in template['argv']:
    if word==str(native/'tests/runtime_fdl.cc'):argv.extend(map(str,objects))
    else:argv.append(word)
   binary=out/('packets-'+cc);argv[argv.index('-o')+1]=str(binary);run(argv,template['cwd']);result=json.loads(run([binary]).stdout)
   if result.get('encoded_states')!=69 or result.get('default_rgba8_normalized_commands_equal')is not True:raise ValueError('packet/default37 contract')
   results.append(result)
  ndk=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';inputs[str(ndk.resolve())]=sha(ndk);objects=[]
  for i,q in enumerate([fixture,*sources,pipeline,pipeline]):
   old=i==len(sources)+2;extra=['-DNN_PIPELINE_COLOR_TARGETS='+('0'if old else'1')]
   if old:extra+=['-Dnn_pipeline_encode=nn_pipeline_encode_legacy','-Dnn_pipeline_preflight=nn_pipeline_preflight_legacy','-Dnn_pipeline_full_raster=nn_pipeline_full_raster_legacy']
   dest=out/('ndk'+str(i)+'.o');run([ndk,*common,*defs,*extra,'-c',q,'-o',dest]);objects.append(dest)
  run([ndk,*objects,c['runtime_fdl_library'],'-Wl,--no-undefined','-lm','-o',out/'packets-api29'])
  if any(sha(q)!=h for q,h in inputs.items()):raise ValueError('source/qualified input changed')
  proof.update(passed=True,validator=oracle,packet_results=results,android_execution=False)
 except Exception as e:proof['error']=str(e)
 proof.update(inputs=inputs,runs=runs);proof['artifacts']={str(q):sha(q)for q in out.rglob('*')if q.is_file()}
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps({'passed':proof['passed'],'check':str(out/'check.json')}));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())

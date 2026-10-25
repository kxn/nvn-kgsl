#!/usr/bin/env python3
"""HOST/NDK metadata + packet encoding only; no GPU color-target qualification."""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path

def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','fdl','ndk','out'):p.add_argument('--'+n,type=Path,required=True)
 p.add_argument('--pipeline-source',type=Path,help='optional tiny integration candidate for actual common target/sampled role test')
 a=p.parse_args();root=a.root.resolve();out=a.out.resolve();own=root/'nvn-native'
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);inputs={};runs=[];proof={'schema':1,'scope':__doc__,'passed':False,'gpu_execution':False}
 def pin(q):q=Path(q).resolve();inputs[str(q)]=sha(q)
 def run(cmd,cwd=None):
  r=subprocess.run(list(map(str,cmd)),cwd=cwd,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  runs.append({'argv':list(map(str,cmd)),'cwd':str(cwd)if cwd else None,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
  if r.returncode:raise ValueError('command failed '+str(cmd)+'\n'+r.stderr)
  return r
 try:
  pin(__file__);module=own/'tools/build_runtime_fdl.py';pin(module)
  s=importlib.util.spec_from_file_location('color_fdl',module);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
  library,c=m.verify(root,a.fdl.resolve());inputs.update(c['inputs']);inputs.update(c['artifacts']);pin(a.fdl)
  modules=[own/'src/color_target.c'];fixture=own/'tests/color_target.c'
  for q in [fixture,*modules]:pin(q)
  for folder in [own/'include',own/'third_party/nvn']:
   for q in folder.rglob('*.h'):pin(q)
  command_record=a.fdl.resolve().parent/'commands.json';pin(command_record);commands=json.loads(command_record.read_text())
  common=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(own/'include'),'-I'+str(own/'third_party/nvn')]
  outputs=[]
  for cc,cpp,san in [('clang','clang++',['-fsanitize=function','-fsanitize-trap=function']),('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
   objects=[]
   for i,q in enumerate([fixture,*modules]):
    dest=out/(cc+str(i)+'.o');run([cc,*common,*san,'-fno-pie','-c',q,'-o',dest]);objects.append(dest)
   original=next(row for row in commands if row['argv'][0]==cpp and str(own/'tests/runtime_fdl.cc') in row['argv'])
   argv=[]
   for word in original['argv']:
    if word==str(own/'tests/runtime_fdl.cc'):argv.extend(map(str,objects))
    else:argv.append(word)
   argv[argv.index('-o')+1]=str(out/('host-'+cc));run(argv,original['cwd']);outputs.append(json.loads(run([out/('host-'+cc)]).stdout))
  if outputs[0]!=outputs[1]or outputs[0]['formats']!=22:raise ValueError('HOST disagreement')
  ndk=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';pin(ndk)
  run([ndk,*common,fixture,*modules,library,'-Wl,--no-undefined','-lm','-o',out/'color-target-api29'])
  if a.pipeline_source:
   pin(a.pipeline_source);role=own/'tests/color_target_pipeline.c';pin(role);role_runs=[]
   for enabled in (0,1):
    defs=['-DNN_PIPELINE_COLOR_TARGETS='+str(enabled),'-DNN_COLOR_PIPELINE_SOURCE='+json.dumps(str(a.pipeline_source.resolve()))]
    for cc,cpp,san in [('clang','clang++',['-fsanitize=function','-fsanitize-trap=function']),('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
     objects=[]
     for i,q in enumerate([role,*modules]):
      dest=out/(cc+'-role'+str(enabled)+'-'+str(i)+'.o');run([cc,*common,*defs,*san,'-ffunction-sections','-fdata-sections','-fno-pie','-c',q,'-o',dest]);objects.append(dest)
     original=next(row for row in commands if row['argv'][0]==cpp and str(own/'tests/runtime_fdl.cc') in row['argv'])
     argv=[]
     for word in original['argv']:
      if word==str(own/'tests/runtime_fdl.cc'):argv.extend(map(str,objects))
      else:argv.append(word)
     argv[argv.index('-o')+1]=str(out/(cc+'-role'+str(enabled)));argv.append('-Wl,--gc-sections');run(argv,original['cwd']);role_runs.append(json.loads(run([out/(cc+'-role'+str(enabled))]).stdout))
    run([ndk,*common,*defs,'-ffunction-sections','-fdata-sections',role,*modules,library,'-Wl,--gc-sections','-Wl,--no-undefined','-lm','-o',out/('role-api29-'+str(enabled))])
   if len(role_runs)!=4 or not all(v['sampled_gate_unchanged']for v in role_runs):raise ValueError('role scope')
   proof['pipeline_role']=role_runs
  source=Path(c['source']);facts={}
  for rel in ['src/freedreno/fdl/fd6_view.cc','src/freedreno/fdl/fd6_format_table.c','src/freedreno/registers/adreno/a6xx.xml','src/freedreno/registers/adreno/adreno_common.xml','src/gallium/drivers/freedreno/a6xx/fd6_gmem.cc','src/gallium/drivers/freedreno/a6xx/fd6_program.cc','src/gallium/drivers/freedreno/a6xx/fd6_blend.cc']:
   q=source/rel;pin(q);facts[rel]=sha(q)
  if any(sha(q)!=h for q,h in inputs.items()):raise ValueError('input changed')
  proof.update(passed=True,host=outputs[0],primary_local_sources=facts,android_execution=False,shader_output_contract='existing native recipe float32vec4; no integer output/half-output changes',runtime_capability_enabled=False)
 except Exception as e:proof['error']=str(e)
 proof.update(inputs=inputs,runs=runs);proof['artifacts']={str(q):sha(q)for q in out.rglob('*')if q.is_file()}
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps({'passed':proof['passed'],'check':str(out/'check.json')}));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())

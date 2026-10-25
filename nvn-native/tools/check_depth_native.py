#!/usr/bin/env python3
"""Normal IR3 v2 depth recipe + HOST native words/legacy gate tests. No GPU execution."""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def produce(root,ir3,casc,foundation,fdl_root,fdl_check,ndk,out,baseline_root=None,baseline_ir3=None,regression_stages=None):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 own=root/'nvn-native';common=module(own/'tools/compute_driverparams.py','depth_compiler_inputs');record,inputs,_,source_count=common.compiler_inputs(root,ir3)
 inputs[str(own/'tools/compute_driverparams.py')]=sha(own/'tools/compute_driverparams.py');normal=module(fdl_root/'nvn-native/tools/build_runtime_fdl.py','depth_normal_fdl');_,fdl=normal.verify(fdl_root,fdl_check)
 def checked(path,required_kind=None):
  c=json.loads(path.read_text());inputs[str(path)]=sha(path)
  if c.get('passed')is False or c.get('gpu_execution')is not False:raise ValueError('invalid HOST input record '+str(path))
  if required_kind and c.get('kind')!=required_kind:raise ValueError('wrong record kind')
  for mapping in [c['inputs'],c.get('artifacts',c.get('outputs',{}))]:
   for name,h in mapping.items():
    if sha(name)!=h:raise ValueError('input/product changed '+name)
    inputs[name]=h
  return c
 source=checked(casc);base=checked(foundation,'native-depth-fdl-foundation-v1');inputs[str(fdl_check)]=sha(fdl_check);inputs.update(fdl['inputs']);inputs.update(fdl['artifacts'])
 if Path(fdl['ndk'])!=ndk or source['native_backend_policy'].get('unit_render_scale')is not True:raise ValueError('wrong explicit native policy/NDK')
 pairs=[r for r in source['pairs']if r['passed']]
 if len(pairs)!=1:raise ValueError('select a normal CASC check with exactly one explicit original pair')
 pair=pairs[0];stages=[source['stages'][pair[k]]for k in ('vs','fs')];spv=[Path(s['prefix']+'.spv')for s in stages]
 if [s['stage']for s in stages]!=['vs','fs']:raise ValueError('not original VS/FS')
 for s in stages:
  if not s['frontend_passed'] or s['envelope']['inputs']['control']['size'] not in (2176,2200):raise ValueError('unvalidated source envelope')
 out.mkdir(parents=True);commands=[]
 def run(cmd,cwd=None,success=True,env=None):
  r=subprocess.run(list(map(str,cmd)),cwd=cwd,text=True,capture_output=True,env=env);commands.append(dict(argv=list(map(str,cmd)),cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if success and r.returncode:raise ValueError(r.stdout+r.stderr)
  return r
 prefix=out/'actual-original-depth';compiler=ir3/'build/src/freedreno/fdl/nvn-pipeline-ir3';run([compiler,'--unit-render-scale',*spv,prefix,'0','0x06050002'],env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip'))
 meta=json.loads(Path(str(prefix)+'.pipeline.json').read_text());depth=meta.get('depth_export')
 if depth!={'schema':1,'recipe_version':2,'writes_depth':True,'regid':14,'color_outputs':0,'late_z_required':True,'lrz_disabled_required':True}:raise ValueError('actual original depth output changed; inspect before updating qualification')
 native_equal=[]
 for suffix in ('.vs.bin','.fs.bin','.vs.asm','.fs.asm','.link.json'):
  a=Path(str(prefix)+suffix);b=Path(pair['prefix']+suffix)
  if a.read_bytes()!=b.read_bytes():raise ValueError('shader lowering changed with recipe-only extension '+suffix)
  native_equal.append(suffix)
 regressions=[]
 if any((baseline_root,baseline_ir3,regression_stages)):
  if not all((baseline_root,baseline_ir3,regression_stages)):raise ValueError('all three baseline regression options are required')
  baseline_common=module(baseline_root/'nvn-native/tools/compute_driverparams.py','depth_baseline_compiler');_,baseline_inputs,_,_=baseline_common.compiler_inputs(baseline_root,baseline_ir3);inputs.update(baseline_inputs)
  old_stages=checked(regression_stages);selected={}
  for row in old_stages['pairs']:
   if not row['passed']:continue
   group=bin(row['program']['fs']['nir_usage']['texture_usage']['texture_mask']).count('1')
   if group in (0,1,2)and group not in selected:selected[group]=row
  if set(selected)!={0,1,2}:raise ValueError('three original color resource groups not present')
  for group,row in sorted(selected.items()):
   pair_spv=[Path(old_stages['stages'][row[k]]['prefix']+'.spv')for k in ('vs','fs')];prefixes=[]
   for tag,build in [('baseline',baseline_ir3),('current',ir3)]:
    pre=out/(tag+'-color-'+str(group));prefixes.append(pre);run([build/'build/src/freedreno/fdl/nvn-pipeline-ir3','--unit-render-scale',*pair_spv,pre,'0','0x06050002'],env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip'))
   identical=[]
   for suffix in ('.vs.bin','.fs.bin','.vs.asm','.fs.asm','.link.json','.pipeline.h','.pipeline.json','.pipeline.types.json'):
    if Path(str(prefixes[0])+suffix).read_bytes()!=Path(str(prefixes[1])+suffix).read_bytes():raise ValueError('old color recipe changed '+str(group)+suffix)
    identical.append(suffix)
   regressions.append(dict(resource_count=group,byte_identical=identical))
 # NDK module uses the exact prior normal Android metadata configuration.
 original_commands=json.loads((fdl_check.parent/'commands.json').read_text());recipes=json.loads((fdl_check.parent/'source-closure/compile-recipes.json').read_text());fs=recipes[3]['flags'];tools=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin';cc=tools/'aarch64-linux-android29-clang';cpp=tools/'aarch64-linux-android29-clang++';
 for tool in [cc,cpp,tools/'llvm-nm',tools/'llvm-readelf']:inputs[str(tool)]=sha(tool)
 module_obj=out/'depth-native-api29.o';run([cpp,*fs,'-I'+str(own/'include'),'-Werror','-c',own/'src/depth_native.cc','-o',module_obj],fdl_check.parent/'android-build')
 fixture=own/'tests/d2r_bridge/depth_native.c';cflags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(own/'include'),'-DNN_PIPELINE_DEPTH_NATIVE=1','-DNN_DEPTH_RECIPE_HEADER="'+str(prefix)+'.pipeline.h"'];sources=['src/depth_attachment.c','src/pipeline_program.c','src/pipeline_shader.c','tests/d2r_bridge/depth_native.c']
 objs=[]
 for i,src in enumerate(sources):
  obj=out/('ndk'+str(i)+'.o');run([cc,*cflags,'-c',own/src,'-o',obj]);objs.append(obj)
 run([cc,*objs,module_obj,base['static_library'],'-Wl,--gc-sections','-Wl,--no-undefined','-lm','-o',out/'depth-native-api29-not-executed'])
 host=[]
 for compiler,ccompiler in [('g++','gcc'),('clang++','clang')]:
  t=next(c for c in original_commands if c['argv'][0]==compiler);a=t['argv'];start=a.index('-I'+str(fdl_root/'nvn-native/include'));end=a.index('-Wl,--as-needed');cpp_flags=a[1:start]+['-I'+str(own/'include'),'-I'+str(own/'third_party/nvn')];libs=a[end:-2];san=['-fsanitize=address,undefined','-fno-omit-frame-pointer']if compiler=='g++'else['-fsanitize=function','-fsanitize-trap=function'];objects=[]
  for i,src in enumerate(sources):
   obj=out/(ccompiler+str(i)+'.o');run([ccompiler,*cflags,*san,'-fno-pie','-c',own/src,'-o',obj]);objects.append(obj)
  binary=out/('host-'+ccompiler);run([compiler,*cpp_flags,*san,'-fno-pie','-no-pie',own/'src/depth_fdl.cc',own/'src/depth_native.cc',*objects,*libs,'-Wl,--gc-sections','-o',binary],t['cwd']);result=run([binary],t['cwd']);host.append(json.loads(result.stdout.strip().splitlines()[-1]));
  for c in (compiler,ccompiler):q=Path(shutil.which(c)).resolve();inputs[str(q)]=sha(q)
 if host[0]!=host[1]or host[0]['legacy_v2_rejected']is not True or host[0]['pixel_map_calls']or host[0]['gpu_execution']:raise ValueError('HOST native packet/legacy regression differs')
 for p in [Path(__file__),fixture,own/'tests/d2r_bridge/depth_attachment.c',*[own/s for s in sources],own/'src/depth_native.cc',own/'src/depth_fdl.cc']:
  inputs[str(p.resolve())]=sha(p)
 for p in (own/'include').rglob('*.h'):inputs[str(p)]=sha(p)
 for name,h in inputs.items():
  if sha(name)!=h:raise ValueError('source changed while checking '+name)
 c=dict(schema=1,kind='native-depth-recipe-foundation-v1',passed=True,host_only=True,gpu_execution=False,android_execution=False,original_source_envelopes=[s['envelope']for s in stages],depth_export=depth,unchanged_native=native_equal,host=host,color_regressions=regressions,compiler_record=str(ir3/'build-record.json'),compiler_source_files=source_count,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()},scope='Original immutable shader v2 recipe + native attachment packet HOST checks and nonexecuted real NDK link only; no depth draw/blit GPU qualification')
 (out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ['root','ir3','casc','foundation','fdl-root','fdl-check','ndk','out']:p.add_argument('--'+n,required=True,type=Path)
 for n in ['baseline-root','baseline-ir3','regression-stages']:p.add_argument('--'+n,type=Path)
 a=p.parse_args();print(produce(*[getattr(a,n).resolve()for n in ['root','ir3','casc','foundation','fdl_root','fdl_check','ndk','out']],*[getattr(a,n).resolve()if getattr(a,n)else None for n in ['baseline_root','baseline_ir3','regression_stages']]))

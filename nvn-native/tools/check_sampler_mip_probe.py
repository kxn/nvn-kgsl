#!/usr/bin/env python3
"""HOST ownership/oracle negatives and actual KGSL API29 link; no GPU execution."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
from sampler_mip_qualification import build,sha


def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in('root','check','ndk','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();root=a.root.resolve();n=Path(__file__).resolve().parents[1];out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);runs=[];inputs={};proof=dict(schema=1,kind='raw-native-sampler-mip-probe-host-v1',passed=False,gpu_execution=False,android_execution=False,runs=runs)
 def run(argv,cwd=None):
  argv=list(map(str,argv));x=subprocess.run(argv,cwd=cwd,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));runs.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=x.returncode,stdout=x.stdout,stderr=x.stderr));(out/'progress.json').write_text(json.dumps(runs,indent=2)+'\n')
  if x.returncode:raise ValueError('HOST/NDK failed: '+x.stderr[-6000:])
  return x.stdout
 try:
  import numpy
  header,c,inputs=build(a.check.resolve(),out/'compiled',root=root)
  for folder in('include','src','third_party','android'):
   for f in(n/folder).rglob('*'):
    if f.is_file()and f.suffix in('.c','.h','.cc','.inc'):inputs[str(f.resolve())]=sha(f)
  fixture=n/'tests/d2r_bridge/sampler_mip_probe_host.c';validator=n/'tests/test_sampler_mip_validation.py'
  for f in(Path(__file__).resolve(),n/'tools/build_android_probe.py',n/'tools/run_android_probe.py',n/'CMakeLists.txt',fixture,validator,n/'tests/d2r_bridge/memory_backend.c',n/'tests/d2r_bridge/window_session_backend.inc',Path(numpy.__file__).resolve(),Path(sys.executable).resolve()):inputs[str(f)]=sha(f)
  config=json.loads(a.check.read_text());fdl=json.loads(Path(config['runtime_fdl_check']).read_text());host=Path(fdl['normal_ir3'])/'build'
  spec=importlib.util.spec_from_file_location('mip_check_fdl',n/'tools/build_runtime_fdl.py');normal=importlib.util.module_from_spec(spec);spec.loader.exec_module(normal)
  entry=next(e for e in json.loads((host/'compile_commands.json').read_text())if e['file'].endswith('/fd6_view.cc'));_,flags=normal.flags(entry)
  command=shlex.split(run(['ninja','-C',host,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
  for value in command[1:]:
   if skip:skip=False;continue
   if value=='-o':skip=True;continue
   if not value.endswith('.o'):libs.append(value)
  for value in libs:
   f=(host/value).resolve()
   if f.is_file():inputs[str(f)]=sha(f)
  modules=[n/'src'/(s+'.c')for s in('offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','render_state','sampler_state','pipeline','sampled_image','texture_bc_shadow','compute','block_copy','texture_source')]
  common=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),'-I'+str(n/'tests')]
  defs=['-DNN_PIPELINE_SAMPLED_IMAGES=1']
  for k,v in{'NN_SAMPLER_MIP_MEMORY_BACKEND':n/'tests/d2r_bridge/memory_backend.c','NN_SAMPLER_MIP_PROBE_SOURCE':n/'tests/sampler_mip_probe.c','NVN_PIPELINE_QUALIFICATION_HEADER':header}.items():defs.append('-D'+k+'='+json.dumps(str(v)))
  results=[]
  for cc,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
   folder=out/cc;folder.mkdir();objects=[]
   for tool in(cc,cpp):f=Path(shutil.which(tool)).resolve();inputs[str(f)]=sha(f)
   for i,s in enumerate([fixture,*modules]):
    obj=folder/(str(i)+'.o');run([cc,*common,*defs,'-DNN_PIPELINE_SAMPLED_MIP_FILTER=1',*san,'-fno-pie','-c',s,'-o',obj]);objects.append(obj)
   ff=flags if cpp=='g++'else[x for x in flags if x not in('-flifetime-dse=1','-Wno-error=maybe-uninitialized')]
   obj=folder/'fdl.o';run([cpp,*ff,'-I'+str(n/'include'),'-Werror',*san,'-fno-pie','-c',n/'src/runtime_fdl.cc','-o',obj],host);objects.append(obj)
   exe=folder/'probe';run([cpp,'-no-pie',*san,*objects,*libs,'-lm','-o',exe],host);raw=folder/'raw';raw.mkdir();text=run([exe,raw]);data=json.loads(text)
   if data.get('passed')is not True or data.get('submissions')!=576:raise ValueError('HOST matrix incomplete')
   result=folder/'result.json';result.write_text(text);oracle=json.loads(run([sys.executable,validator,out/'compiled/check.json',result,raw]));results.append(oracle)
   if oracle.get('nonexecuting_HOST_pixels_rejected')is not True:raise ValueError('nonexecuting HOST seed accepted')
   obj=folder/'fixture-off.o';run([cc,*common,*defs,*san,'-fno-pie','-c',fixture,'-o',obj]);sampled=folder/'sampled-off.o';run([cc,*common,*defs,*san,'-fno-pie','-c',n/'src/sampled_image.c','-o',sampled])
   # Only the sampler role helper's explicit profile changes; no workaround is
   # injected into the common pipeline preflight or emitted descriptor table.
   off=folder/'off';rest=[o for o,s in zip(objects[:-1],[fixture,*modules])if s not in(fixture,n/'src/sampled_image.c')]
   run([cpp,'-no-pie',*san,obj,sampled,*rest,objects[-1],*libs,'-lm','-o',off],host);offdir=folder/'off-raw';offdir.mkdir()
   if 'defaultOFF closed'not in run([off,offdir]):raise ValueError('mip profile default OFF gate changed')
  if results[0]!=results[1]:raise ValueError('HOST oracle results differ')
  cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';inputs[str(cc.resolve())]=sha(cc.resolve())
  lib=out/'sampler-mip-real-api29.so'
  run([cc,*common,'-DNN_PIPELINE_SAMPLED_IMAGES=1','-DNN_PIPELINE_SAMPLED_MIP_FILTER=1','-DNVN_PIPELINE_QUALIFICATION_HEADER='+json.dumps(str(header)),'-I'+str(n/'third_party/kgsl'),'-fPIC','-shared','-Wl,--no-undefined',n/'tests/sampler_mip_probe.c',n/'src/kgsl.c',n/'src/kgsl_probe.c',*modules,c['runtime_fdl_library'],'-landroid','-ldl','-lm','-o',lib])
  run([cc,*common,'-fPIC','-c',n/'android/pipeline_probe_activity.c','-o',out/'activity-api29.o'])
  # No real ADB or build: invalid mixed modes must stop in argparse.
  for extra in('--nvn-api','--device-inspect','--native-gather-check'):
   argv=[sys.executable,n/'tools/build_android_probe.py','--sdk',a.ndk.parent.parent,'--ndk',a.ndk,'--out',out/'MUST_NOT_BUILD','--native-sampler-mip-check',a.check.resolve(),extra]
   if extra=='--native-gather-check':argv.append(str(a.check.resolve()))
   x=subprocess.run(list(map(str,argv)),capture_output=True,text=True)
   if x.returncode==0 or'independent'not in x.stderr or(out/'MUST_NOT_BUILD').exists():raise ValueError('CLI independent mode protection')
  if any(sha(f)!=h for f,h in inputs.items()):raise ValueError('source/qualified input changed during checks')
  proof.update(passed=True,HOST_nonexecuting_submits_per_sanitizer=576,actual_GPU_submits=0,ambiguous_owners_per_sanitizer=2,constructor_faults_per_sanitizer=5,negative_cases=results[0]['negative_cases'],cli_mode_negative_cases=3,default_OFF_closed=True,oracle=results[0])
 except Exception as error:proof['error']=str(error)
 proof.update(inputs=inputs,artifacts={str(f):sha(f)for f in out.rglob('*')if f.is_file()});(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],proof=str(out/'check.json'))));return 0 if proof['passed']else 1


if __name__=='__main__':raise SystemExit(main())

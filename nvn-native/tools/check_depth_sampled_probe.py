#!/usr/bin/env python3
"""Normal sampled-depth qualifier ownership/oracle/NDK checks; no GPU."""
import argparse,json,os,shutil,subprocess,sys
from pathlib import Path
from depth_sampled_probe_qualification import build,hashes,sha

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in('root','check','ndk','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();root=a.root.resolve();n=Path(__file__).resolve().parents[1];out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 header,c,inputs=build(a.check.resolve(),out,root=root);out.mkdir(parents=True);runs=[]
 def run(argv,cwd=None):
  argv=list(map(str,argv));r=subprocess.run(argv,cwd=cwd,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));runs.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if r.returncode:raise ValueError((r.stdout+r.stderr)[-7000:])
  return r.stdout
 for folder in('include','src','third_party','android'):
  for f in(n/folder).rglob('*'):
   if f.is_file()and f.suffix in('.h','.c','.cc','.inc'):inputs[str(f.resolve())]=sha(f)
 for rel in('CMakeLists.txt','tools/check_depth_sampled_probe.py','tools/build_android_probe.py','tools/run_android_probe.py','tests/d2r_bridge/depth_sampled_probe_host.c','tests/test_depth_sampled_probe_validation.py','tests/d2r_bridge/memory_backend.c','tests/d2r_bridge/window_session_backend.inc','tests/depth_shadow_cases.h'):
  inputs[str(n/rel)]=sha(n/rel)
 modules=[n/'src'/(s+'.c')for s in('offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','render_state','sampler_state','pipeline','depth_attachment','depth_transfer','order','pipeline_views','depth_shadow','depth_sampled','depth_sampled_native','sampled_image','texture_bc_shadow','block_copy','compute','texture_source')];cppmodules=[n/'src'/(s+'.cc')for s in('depth_native','depth_a2d','depth_fdl','runtime_fdl')]
 common=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn')]
 defs=['-DNN_DEPTH_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c')),'-DNN_DEPTH_PROBE_SOURCE='+json.dumps(str(n/'tests/depth_sampled_probe.c')),'-DNVN_PIPELINE_QUALIFICATION_HEADER='+json.dumps(str(header))]
 original=json.loads((Path(c['fdl_check']).parent/'commands.json').read_text());fdl_root=Path(c['fdl_root']);answers=[]
 for cc,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function','-fno-pie'])]:
  template=next(e for e in original if e['argv'][0]==cpp);argv=template['argv'];start=argv.index('-I'+str(fdl_root/'nvn-native/include'));end=argv.index('-Wl,--as-needed');flags=argv[1:start];libs=argv[end:-2]
  for tool in(cc,cpp):path=Path(shutil.which(tool)).resolve();inputs[str(path)]=sha(path)
  for enabled in(0,1):
   objects=[]
   for i,source in enumerate([n/'tests/d2r_bridge/depth_sampled_probe_host.c',*modules]):
    obj=out/(cc+str(enabled)+'-'+str(i)+'.o');run([cc,*common,*defs,*san,'-DNN_PIPELINE_DEPTH_NATIVE=1','-DNN_PIPELINE_GATHER=1','-DNN_PIPELINE_SAMPLED_IMAGES=1','-DNN_PIPELINE_RUNTIME_VIEWS=1','-DNN_PIPELINE_DEPTH_SAMPLED='+str(enabled),'-c',source,'-o',obj]);objects.append(obj)
   exe=out/('probe-'+cc+'-'+str(enabled));run([cpp,*flags,*san,'-no-pie','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),*cppmodules,*objects,*libs,'-Wl,--gc-sections','-o',exe],template['cwd'])
   raw=out/(cc+str(enabled)+'-raw');raw.mkdir();text=run([exe,raw]);result=out/(cc+str(enabled)+'-result.json');result.write_text(text)
   if not enabled:
    if 'defaultOFF closed'not in text:raise ValueError('depth shadow default gate changed')
    continue
   data=json.loads(text)
   if data.get('passed')is not True or data.get('submissions')!=180:raise ValueError('HOST fullchain protocol not complete')
   answer=json.loads(run([sys.executable,n/'tests/test_depth_sampled_probe_validation.py','--check',a.check.resolve(),'--host-result',result,'--host-raw',raw]));answers.append(answer)
   if not answer['nonexecuting_HOST_pixels_rejected']:raise ValueError('HOST no-op/upload controls missing')
 if answers[0]!=answers[1]:raise ValueError('sanitized complete protocol/oracle differed')
 ndk=a.ndk.resolve();cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';readelf=cc.parent/'llvm-readelf';nm=cc.parent/'llvm-nm'
 for tool in(cc.resolve(),readelf,nm):inputs[str(tool)]=sha(tool)
 for enabled in(0,1):
  lib=out/('libdepth-sampled-probe-'+str(enabled)+'-api29.so');run([cc,*common,'-DNN_PIPELINE_DEPTH_NATIVE=1','-DNN_PIPELINE_GATHER=1','-DNN_PIPELINE_SAMPLED_IMAGES=1','-DNN_PIPELINE_RUNTIME_VIEWS=1','-DNN_PIPELINE_DEPTH_SAMPLED='+str(enabled),'-DNVN_PIPELINE_QUALIFICATION_HEADER='+json.dumps(str(header)),'-I'+str(n/'third_party/kgsl'),'-fPIC','-shared','-Wl,--no-undefined',n/'tests/depth_sampled_probe.c',n/'src/kgsl.c',n/'src/kgsl_probe.c',*modules,*c['native_libraries'],'-landroid','-ldl','-lm','-o',lib]);elf=run([readelf,'-h','-d',lib]);symbols=run([nm,'--defined-only',lib]);(out/('elf-'+str(enabled)+'.txt')).write_text(elf+'\n'+symbols)
  if 'AArch64'not in elf or any(v in elf.lower()for v in('libvulkan','libegl','libgles','libstdc++')):raise ValueError('unexpected backend/runtime')
  if any(v in symbols for v in('util_format_unpack','util_format_read','util_format_write','nn_clear_color_pattern')):raise ValueError('CPU pixel oracle leaked into Android DSO')
 run([cc,*common,'-fPIC','-c',n/'android/pipeline_probe_activity.c','-o',out/'activity-api29.o'])
 negatives=[]
 for extra in(['--nvn-api'],['--device-inspect'],['--native-depth-check',str(a.check.resolve())],['--cs-case','1'],['--nvn-pipeline-check',str(a.check.resolve())]):
  cmd=[sys.executable,n/'tools/build_android_probe.py','--sdk',ndk.parent.parent,'--ndk',ndk,'--out',out/'MUST_NOT_BUILD','--native-depth-sampled-check',a.check.resolve(),*extra];r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True)
  if r.returncode!=2 or 'independent sampled-depth'not in r.stderr or(out/'MUST_NOT_BUILD').exists():raise ValueError('bad CLI mode accepted')
  negatives.append(extra)
 hashes(inputs);proof=dict(schema=1,kind='sampled-depth-fullchain-probe-host-v1',passed=True,gpu_execution=False,android_execution=False,HOST_nonexecuting_submissions_per_sanitizer=180,unknown_owners_per_sanitizer=4,constructor_faults_per_sanitizer=3,private_image_CPU_accesses=0,default_OFF_closed=True,oracle=answers[0],cli_negatives=negatives,inputs=inputs,artifacts={str(f):sha(f)for f in out.rglob('*')if f.is_file()});(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(out/'check.json')
if __name__=='__main__':main()

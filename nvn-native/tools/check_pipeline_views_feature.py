#!/usr/bin/env python3
"""HOST verified runtime target feature/options and actual original-host NDK link."""
import argparse,hashlib,importlib.util,json,shlex,shutil,subprocess,sys
from pathlib import Path
from workspace_paths import d2r_root
from pipeline_views_feature import prepare,audit,sha
def load(p,n):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def check(root,ndk,fdl,graphics,names,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 n=root/'nvn-native';library,manifest,inputs=prepare(root,fdl,graphics)
 from graphics_pipeline_cache import build
 header,pm=build(root,graphics);inputs.update(pm['inputs']);inputs[str(graphics)]=sha(graphics)
 for folder in (n,d2r_root()/'dm/host',d2r_root()/'dm/third_party/runtime-codecs',d2r_root()/'dm/third_party/libdeflate'):
  for p in folder.rglob('*'):
   if p.is_file()and'__pycache__'not in p.parts:inputs[str(p)]=sha(p)
 out.mkdir(parents=True);commands=[];results=[];assertions=0
 def require(v):
  nonlocal assertions
  if not v:raise ValueError('feature policy assertion failed')
  assertions+=1
 def run(argv):
  r=subprocess.run(list(map(str,argv)),capture_output=True,text=True);commands.append(dict(argv=list(map(str,argv)),returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError((r.stdout+r.stderr)[-5000:])
  return r.stdout
 require(prepare(root,None,None)==(None,None,{}));require(manifest['static_size_catalogue']is False and manifest['default_enabled']is False)
 for name,fn in [('graphics prerequisite',lambda:prepare(root,fdl,None)),('different FDL archives',lambda:prepare(root,fdl,graphics,Path('/unverified/archive.a')))]:
  try:fn()
  except ValueError:require(True)
  else:raise ValueError('accepted '+name)
 for script in ('build_d2r_activity.py','build_d2r_host.py'):
  r=subprocess.run([sys.executable,str(n/'tools'/script),'--exefs','/not-read','--ndk',str(ndk),'--out',str(out/'forbidden'),'--pipeline-target-fdl-check',str(fdl)],capture_output=True,text=True)
  require(r.returncode==2 and '--pipeline-target-fdl-check requires'in r.stderr and not(out/'forbidden').exists())
 fake=out/'argv-inventory';fake.write_text('HOST ONLY');mapping={str(fake):sha(fake)}
 options=dict(host_abi=2,rewrite_abi='elfdesc',rules='native',private_gates='1',abi='arm64-v8a',execute=False,deploy=False,library_kind='shared',python=sys.executable,exefs='/original/exefs',ndk=str(ndk),jobs=4)
 host=dict(options=options,modules=str(out/'modules'),artifacts=mapping,sources=mapping,tools=mapping)
 for enabled in (False,True):
  if enabled:options['pipeline_target_fdl_check']=str(fdl)
  for module,name in [('build_d2r_activity_apk.py','host_verify'),('run_d2r_host.py','host_verify_argv')]:
   m=load(n/'tools'/module,'views_'+name);options.update(host_abi=2 if name=='host_verify'else 1,rewrite_abi='elfdesc'if name=='host_verify'else'elf');argv=getattr(m,name)(host,root);require(('--pipeline-target-fdl-check'in argv)==enabled)
 for mode,enabled in [('activity',True),('host',True),('activity',False)]:
  dest=out/(mode+('-on'if enabled else'-off'));target='dmhost_activity_kgsl'if mode=='activity'else'dmhost_native_kgsl'
  run(['cmake','-S',n/'adapters/d2r'/mode,'-B',dest,'-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DNVN_D2R_NAMES_HEADER='+str(names),'-DNVN_D2R_GRAPHICS_PIPELINE_HEADER='+str(header),'-DNVN_D2R_PIPELINE_TARGET_FDL_LIBRARY='+(str(library)if enabled else'')]);run(['cmake','--build',dest,'--target',target,'-j','6'])
  actual=run(['ninja','-C',dest,'-t','commands',target]);audit(n,manifest if enabled else None,actual)
  compiled=[]
  for line in actual.splitlines():
   words=shlex.split(line)
   if '-c'in words:compiled.append(Path(words[words.index('-c')+1]).resolve())
  original=list(map(Path,(dest/'host-sources.txt').read_text().splitlines()));require(len(original)==36 and all(p.resolve()in compiled for p in original))
  elf=dest/('libdmhost_activity_kgsl.so'if mode=='activity'else'dmhost_native_kgsl');info=run([ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf','-h','-d','-s','--wide',elf]);require('AArch64'in info and not any(x in info.lower()for x in ('libvulkan','libegl','libgles')))
  if enabled:require('nn_pipeline_views_get'in info and 'nn_fdl_image'in info)
  else:require('nn_pipeline_views_get'not in info and 'nn_fdl_image'not in info)
  results.append(dict(mode=mode,enabled=enabled,compiled_sources=len(compiled),original_host_sources=36,elf=str(elf),sha256=sha(elf)))
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('source changed '+p)
 proof=dict(schema=1,kind='pipeline-runtime-views-production-feature-host-v1',passed=True,android_execution=False,gpu_execution=False,original_module_execution=False,assertions=assertions,results=results,feature=manifest,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()and p.suffix in ('.json','.a','.o','.so','.txt')});(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','ndk','fdl-check','graphics-check','names','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(check(a.root.resolve(),a.ndk.resolve(),a.fdl_check.resolve(),a.graphics_check.resolve(),a.names.resolve(),a.out.resolve()))

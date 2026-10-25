#!/usr/bin/env python3
"""HOST normal production option propagation + real original-host ARM64 link.
No original NSO/module, Android process, GPU, or pixel conversion executes.
"""
import argparse,copy,hashlib,importlib.util,json,shlex,subprocess,sys
from pathlib import Path
from workspace_paths import d2r_root
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p,n):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def check(root,ndk,names,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 out.mkdir(parents=True);n=root/'nvn-native';inputs={str(names):sha(names)};commands=[];results=[];assertions=0
 def ck(x):
  nonlocal assertions
  if not x:raise ValueError('feature assertion')
  assertions+=1
 def run(argv):
  p=subprocess.run(list(map(str,argv)),capture_output=True,text=True);commands.append(dict(argv=list(map(str,argv)),returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if p.returncode:raise ValueError((p.stdout+p.stderr)[-4000:])
  return p.stdout
 for folder in [n,d2r_root()/'dm/host',d2r_root()/'dm/third_party/runtime-codecs',d2r_root()/'dm/third_party/libdeflate']:
  for p in folder.rglob('*'):
   if p.is_file()and'__pycache__'not in p.parts:inputs[str(p)]=sha(p)
 fake=out/'argv-fixture';fake.write_text('HOST ONLY');mapping={str(fake):sha(fake)}
 base=dict(host_abi=2,rewrite_abi='elfdesc',rules='native',private_gates='1',abi='arm64-v8a',execute=False,deploy=False,library_kind='shared',python=sys.executable,exefs='/original/exefs',ndk=str(ndk),jobs=4)
 host=dict(options=base,modules=str(out/'modules'),artifacts=mapping,sources=mapping,tools=mapping)
 for enabled in [False,True]:
  h=copy.deepcopy(host);h['options']['bntx_packaged_layout_experiment']=enabled
  for module,fn in [('build_d2r_activity_apk.py','host_verify'),('run_d2r_host.py','host_verify_argv')]:
   h['options'].update(host_abi=2 if fn=='host_verify'else 1,rewrite_abi='elfdesc'if fn=='host_verify'else'elf')
   argv=getattr(load(n/'tools'/module,fn),fn)(h,root);ck(('--bntx-packaged-layout-experiment'in argv)==enabled)
 for mode,enabled in [('activity',True),('host',True),('activity',False)]:
  dest=out/(mode+('-on'if enabled else'-off'));target='dmhost_activity_kgsl'if mode=='activity'else'dmhost_native_kgsl'
  run(['cmake','-S',n/'adapters/d2r'/mode,'-B',dest,'-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DNVN_D2R_NAMES_HEADER='+str(names),'-DNVN_BNTX_PACKAGED_LAYOUT_EXPERIMENT='+('ON'if enabled else'OFF')]);run(['cmake','--build',dest,'--target',target,'-j','6']);actual=run(['ninja','-C',dest,'-t','commands',target])
  compiled=[]
  for line in actual.splitlines():
   a=shlex.split(line)
   if '-c'in a:compiled.append(Path(a[a.index('-c')+1]).resolve())
  original=list(map(Path,(dest/'host-sources.txt').read_text().splitlines()));ck(len(original)==36 and all(p.resolve()in compiled for p in original))
  ck((n/'src/packaged_bntx.c').resolve()in compiled if enabled else (n/'src/packaged_bntx.c').resolve()not in compiled);ck(('-DNN_NVN_BNTX_PACKAGED_EXPERIMENT=1'in actual)==enabled)
  ck(not any(x in actual.lower()for x in ['/gfx/metal/','/gfx/vulkan/','/hle_nvn.c','/tests/public_copy_probe.c']))
  binary=dest/('libdmhost_activity_kgsl.so'if mode=='activity'else'dmhost_native_kgsl');elf=run([ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf','-h','-d','-s','--wide',binary]);ck('AArch64'in elf and not any(x in elf.lower()for x in ['libvulkan','libgles','libegl']));ck(('nn_packaged_bntx_height'in elf)==enabled)
  results.append(dict(mode=mode,enabled=enabled,original_host_sources=36,compiled_sources=len(compiled),elf=str(binary),sha256=sha(binary)))
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('input changed '+p)
 value=dict(schema=1,passed=True,kind='experimental-bntx-writer-production-feature-host-v1',scope='HOST original-host source ARM64 link/option propagation only; no original modules/GPU/Android execution or normative NVN encoding claim',assertions=assertions,results=results,commands=commands,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()and p.suffix in ['.so','.json','.txt']});(out/'check.json').write_text(json.dumps(value,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ['root','ndk','names','out']:p.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();print(check(a.root.resolve(),a.ndk.resolve(),a.names.resolve(),a.out.resolve()))

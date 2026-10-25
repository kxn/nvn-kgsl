#!/usr/bin/env python3
"""Actual-FDL A650 depth A2D HOST/NDK metadata+packet checks. Never GPU execution."""
import argparse,hashlib,importlib.util,json,shlex,shutil,subprocess,tarfile
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p):
 s=importlib.util.spec_from_file_location('depth_a2d_fdl',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def check(root,fdl_root,fdl_check,depth_check,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 tool=fdl_root/'nvn-native/tools/build_runtime_fdl.py';_,fdl=module(tool).verify(fdl_root,fdl_check)
 inputs=dict(fdl['inputs']);inputs.update(fdl['artifacts']);inputs[str(fdl_check)]=sha(fdl_check);inputs[str(tool)]=sha(tool)
 dep=json.loads(depth_check.read_text());inputs[str(depth_check)]=sha(depth_check)
 if not dep.get('passed')or dep.get('gpu_execution')is not False or Path(fdl['ndk'])!=ndk:raise ValueError('wrong input records')
 for group in ['inputs','artifacts']:
  for p,h in dep[group].items():
   if sha(p)!=h:raise ValueError('depth input changed '+p)
   inputs[p]=h
 own=root/'nvn-native';files=['include/nvn_native/depth_a2d.h','src/depth_a2d.cc','tests/depth_a2d.cc','tools/check_depth_a2d.py','include/nvn_native/depth_fdl.h','src/depth_fdl.cc','include/nvn_native/pm4.h']
 for name in files:inputs[str(own/name)]=sha(own/name)
 out.mkdir(parents=True);commands=[]
 def run(a,cwd=None):
  a=list(map(str,a));r=subprocess.run(a,cwd=cwd,text=True,capture_output=True);commands.append(dict(argv=a,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stdout+r.stderr)
  return r.stdout
 original=json.loads((fdl_check.parent/'commands.json').read_text());host=[]
 for cpp,san in [('g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer']),('clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  t=next(x for x in original if x['argv'][0]==cpp);a=t['argv'];start=a.index('-I'+str(fdl_root/'nvn-native/include'));end=a.index('-Wl,--as-needed');flags=a[1:start];libs=a[end:-2];binary=out/('host-'+cpp.replace('+','p'))
  run([cpp,*flags,'-I'+str(own/'include'),'-I'+str(fdl_root/'nvn-native/third_party/nvn'),'-Werror',*san,'-fno-pie','-no-pie',own/'src/depth_a2d.cc',own/'src/depth_fdl.cc',own/'tests/depth_a2d.cc',*libs,'-o',binary],t['cwd']);host.append(json.loads(run([binary],t['cwd'])))
  p=Path(shutil.which(cpp)).resolve();inputs[str(p)]=sha(p)
 if len(host)!=2 or host[0]!=host[1]or host[0]['layout_cases']!=258:raise ValueError('HOST result mismatch')
 vendor=fdl_check.parent/'source-closure'
 with tarfile.open(fdl_check.parent/'metadata-source-closure.tar.gz')as archive:
  for m in archive:
   if not m.isfile():continue
   q=vendor/Path(m.name).relative_to('metadata-source-closure')
   if not q.is_relative_to(vendor)or q.read_bytes()!=archive.extractfile(m).read():raise ValueError('normal closure changed '+str(q))
 recipes=json.loads((vendor/'compile-recipes.json').read_text());flags=[f for f in recipes[3]['flags']if not f.startswith('-I'+str(vendor/'adapter'))];tools=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin';cpp=tools/'aarch64-linux-android29-clang++';cc=tools/'aarch64-linux-android29-clang';cwd=fdl_check.parent/'android-build'
 wrapper=out/'depth_a2d.o';run([cpp,*flags,'-I'+str(own/'include'),'-Werror','-MD','-MF',out/'depth_a2d.d','-c',own/'src/depth_a2d.cc','-o',wrapper],cwd)
 objs=[fdl_check.parent/'vendor-rebuild'/('source'+str(i)+'.o')for i in range(1,8)]
 for p in objs:
  if fdl['artifacts'].get(str(p))!=sha(p):raise ValueError('unrecorded original Mesa object')
 exports=['nn_depth_a2d','nn_depth_a2d_words'];partial=out/'depth_a2d_metadata.o';run([tools/'ld.lld','-r','--gc-sections',*[x for name in exports for x in ['-u',name]],wrapper,*objs,'-o',partial]);run([tools/'llvm-objcopy','--localize-hidden',partial]);defs=run([tools/'llvm-nm','-g','--defined-only',partial])
 if set(x.split()[-1]for x in defs.splitlines())!=set(exports):raise ValueError('non-adapter global exports')
 library=out/'libnn_depth_a2d.a';run([tools/'llvm-ar','rcs',library,partial]);shared=out/'libdepth-a2d-fdl-whole.so';run([cc,'-shared','-Wl,--whole-archive',library,dep['static_library'],fdl['static_library'],'-Wl,--no-whole-archive','-Wl,--no-undefined','-lm','-o',shared])
 run([cpp,*flags,'-I'+str(own/'include'),'-Werror',own/'tests/depth_a2d.cc',library,dep['static_library'],'-Wl,--no-undefined','-lm','-o',out/'api29-not-executed'],cwd)
 for x in shlex.split((out/'depth_a2d.d').read_text().replace('\\\n',' ').split(':',1)[1]):
  p=(cwd/x).resolve()
  if p.is_file():inputs[str(p)]=sha(p)
 for toolname in ['aarch64-linux-android29-clang','aarch64-linux-android29-clang++','ld.lld','llvm-ar','llvm-nm','llvm-objcopy']:
  p=tools/toolname;inputs[str(p)]=sha(p)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('input changed '+p)
 result=dict(schema=1,kind='native-depth-a2d-host-v1',passed=True,host_only=True,android_execution=False,gpu_execution=False,host=host,static_library=str(library),inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()},scope='Actual FDL TILE6_3 depth aliases/LINEAR readback and CP_BLIT bodies only. Not a standalone submit: owner, CCU ordering, GPU clear/write/test/readback remain to qualify.')
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ['root','fdl-root','fdl-check','depth-check','ndk','out']:p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(check(*[getattr(a,k).resolve()for k in ['root','fdl_root','fdl_check','depth_check','ndk','out']]))

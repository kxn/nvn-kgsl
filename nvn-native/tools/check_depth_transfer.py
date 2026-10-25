#!/usr/bin/env python3
"""Actual depth transfer owner HOST faults + real KGSL API29 links. No GPU."""
import argparse,hashlib,importlib.util,json,shutil,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p):
 s=importlib.util.spec_from_file_location('depth_transfer_fdl',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def check(root,fdl_root,fdl_check,depth_check,a2d_check,shader_check,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 own=root/'nvn-native';tool=fdl_root/'nvn-native/tools/build_runtime_fdl.py';_,fdl=module(tool).verify(fdl_root,fdl_check);inputs=dict(fdl['inputs']);inputs.update(fdl['artifacts']);inputs[str(fdl_check)]=sha(fdl_check);inputs[str(tool)]=sha(tool)
 def checked(p,kind):
  c=json.loads(p.read_text());inputs[str(p)]=sha(p)
  if c.get('kind')!=kind or not c.get('passed')or c.get('gpu_execution')is not False:raise ValueError('wrong check')
  for group in ['inputs','artifacts']:
   for path,h in c[group].items():
    if sha(path)!=h:raise ValueError('input changed '+path)
    inputs[path]=h
  return c
 depth=checked(depth_check,'native-depth-fdl-foundation-v1');a2d=checked(a2d_check,'native-depth-a2d-host-v1');checked(shader_check,'authored-native-depth-recipes-v1');platform=shader_check.parent/'device.platform.h';inputs[str(platform)]=sha(platform)
 sources=['src/depth_attachment.c','src/depth_transfer.c','src/order.c','tests/d2r_bridge/depth_transfer.c'];cppsources=['src/depth_fdl.cc','src/depth_a2d.cc'];names=sources+cppsources+['tools/check_depth_transfer.py','include/nvn_native/depth_a2d.h','include/nvn_native/depth_fdl.h','include/nvn_native/depth_transfer.h','include/nvn_native/depth_attachment.h','include/nvn_native/pipeline_platform.h','include/nvn_native/pm4.h','include/nvn_native/order.h','include/nvn_native/kgsl.h']
 for name in names:inputs[str(own/name)]=sha(own/name)
 out.mkdir(parents=True);commands=[]
 def run(a,cwd=None):
  a=list(map(str,a));r=subprocess.run(a,cwd=cwd,text=True,capture_output=True);commands.append(dict(argv=a,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stdout+r.stderr)
  return r.stdout
 original=json.loads((fdl_check.parent/'commands.json').read_text());cflags=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-fPIC','-ffunction-sections','-fdata-sections','-I'+str(own/'include'),'-DNN_DEPTH_PLATFORM="'+str(platform)+'"'];host=[]
 for cc,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  t=next(x for x in original if x['argv'][0]==cpp);a=t['argv'];start=a.index('-I'+str(fdl_root/'nvn-native/include'));end=a.index('-Wl,--as-needed');flags=a[1:start];libs=a[end:-2];objs=[]
  for i,name in enumerate(sources):
   obj=out/(cc+str(i)+'.o');run([cc,*cflags,*san,'-fno-pie','-c',own/name,'-o',obj]);objs.append(obj)
  binary=out/cc;run([cpp,*flags,'-I'+str(own/'include'),'-I'+str(fdl_root/'nvn-native/third_party/nvn'),*san,'-fno-pie','-no-pie',*[own/n for n in cppsources],*objs,*libs,'-o',binary],t['cwd']);host.append(json.loads(run([binary])))
  for name in [cc,cpp]:p=Path(shutil.which(name)).resolve();inputs[str(p)]=sha(p)
 if host[0]!=host[1]or host[0]['encoded_jobs']!=36 or host[0]['live_owners']:raise ValueError('HOST mismatch')
 tools=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin';cc=tools/'aarch64-linux-android29-clang';objs=[]
 for i,name in enumerate(sources):
  obj=out/('ndk'+str(i)+'.o');run([cc,*cflags,'-c',own/name,'-o',obj]);objs.append(obj)
 run([cc,*objs,a2d['static_library'],depth['static_library'],'-Wl,--no-undefined','-lm','-o',out/'api29-not-executed'])
 kgsl=fdl_root/'nvn-native/src/kgsl.c';kgslh=fdl_root/'nvn-native/third_party/kgsl/msm_kgsl.h';inputs[str(kgsl)]=sha(kgsl);inputs[str(kgslh)]=sha(kgslh);kobj=out/'kgsl.o';run([cc,*cflags,'-I'+str(kgslh.parent),'-c',kgsl,'-o',kobj]);run([cc,'-shared',*objs[:-1],kobj,a2d['static_library'],depth['static_library'],fdl['static_library'],'-Wl,--no-undefined','-lm','-o',out/'libdepth-transfer-real-kgsl.so'])
 for name in ['aarch64-linux-android29-clang','ld.lld']:p=tools/name;inputs[str(p)]=sha(p)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('input changed '+p)
 c=dict(schema=1,kind='native-depth-transfer-host-v1',passed=True,host_only=True,android_execution=False,gpu_execution=False,host=host,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()},scope='Real FDL/PM4/owner encoding and faults; no submitted clear/readback, GPU depth test or public NVN depth admission')
 (out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ['root','fdl-root','fdl-check','depth-check','a2d-check','shader-check','ndk','out']:p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(check(*[getattr(a,k).resolve()for k in ['root','fdl_root','fdl_check','depth_check','a2d_check','shader_check','ndk','out']]))

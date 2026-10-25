#!/usr/bin/env python3
"""Fresh independent depth FDL/owner HOST tests + real API29 links; never GPU execution."""
import argparse,hashlib,importlib.util,json,re,shutil,subprocess,tarfile
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p):
 s=importlib.util.spec_from_file_location('depth_original_fdl_builder',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def produce(root,fdl_root,fdl_check,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored ROOT/.local output required')
 normal_tool=fdl_root/'nvn-native/tools/build_runtime_fdl.py';normal=module(normal_tool);_,record=normal.verify(fdl_root,fdl_check)
 if record.get('passed')is not True:raise ValueError('normal FDL check did not pass')
 if Path(record['ndk'])!=ndk:raise ValueError('NDK must match the verified normal FDL build')
 inputs=dict(record['inputs']);inputs.update(record['artifacts']);inputs[str(fdl_check)]=sha(fdl_check);inputs[str(normal_tool)]=sha(normal_tool)
 own=root/'nvn-native';names=['include/nvn_native/depth_fdl.h','src/depth_fdl.cc','include/nvn_native/depth_attachment.h','src/depth_attachment.c','tests/depth_fdl.cc','tests/d2r_bridge/depth_attachment.c','src/kgsl.c','include/nvn_native/kgsl.h','third_party/kgsl/msm_kgsl.h','third_party/nvn/nvn.h','tools/check_depth_fdl.py']
 for name in names:inputs[str(own/name)]=sha(own/name)
 out.mkdir(parents=True);commands=[]
 def run(cmd,cwd=None):
  a=list(map(str,cmd));r=subprocess.run(a,cwd=cwd,text=True,capture_output=True);commands.append(dict(argv=a,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stdout+r.stderr)
  return r.stdout
 # The normal tar is hashed by its original build record. Verify every on-disk
 # closure byte before using its already declared Android recipes/objects.
 source_out=fdl_check.parent;vendor=source_out/'source-closure'
 with tarfile.open(source_out/'metadata-source-closure.tar.gz')as t:
  for item in t:
   if not item.isfile():continue
   rel=Path(item.name).relative_to('metadata-source-closure');p=vendor/rel
   if not p.is_relative_to(vendor)or p.read_bytes()!=t.extractfile(item).read():raise ValueError('normal FDL closure changed '+str(p))
 recipes=json.loads((vendor/'compile-recipes.json').read_text());expected=['fd6_format_table.c','fd6_layout.c','fd6_view.cc','freedreno_layout.c','freedreno_dev_info.c','u_format.c','u_format_table.c']
 if [Path(r['source']).name for r in recipes[1:]]!=expected:raise ValueError('not original seven-unit FDL metadata closure')
 original_objects=[source_out/'vendor-rebuild'/('source'+str(i)+'.o')for i in range(1,8)]
 for p in original_objects:
  if record['artifacts'].get(str(p))!=sha(p):raise ValueError('unrecorded Android Mesa object '+str(p))
 tools=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin';cc=tools/'aarch64-linux-android29-clang';cpp=tools/'aarch64-linux-android29-clang++'
 for p in [cc,cpp,tools/'ld.lld',tools/'llvm-ar',tools/'llvm-nm',tools/'llvm-readelf',tools/'llvm-objcopy']:inputs[str(p)]=sha(p)
 # New wrapper uses the original Android-generated config and compile flags.
 flags=[f for f in recipes[3]['flags'] if not f.startswith('-I'+str(vendor/'adapter'))]
 wrapper=out/'depth_fdl.o';run([cpp,*flags,'-I'+str(own/'include'),'-I'+str(own/'third_party/nvn'),'-Werror','-MD','-MF',out/'depth_fdl.d','-c',own/'src/depth_fdl.cc','-o',wrapper],source_out/'android-build')
 exports=['nn_depth_fdl_version','nn_depth_fdl_format','nn_depth_fdl','nn_depth_fdl_controls'];partial=out/'depth_metadata.o'
 run([tools/'ld.lld','-r','--gc-sections',*[x for n in exports for x in ('-u',n)],wrapper,*original_objects,'-o',partial]);run([tools/'llvm-objcopy','--localize-hidden',partial]);globals_report=run([tools/'llvm-nm','-g','--defined-only',partial])
 if {line.split()[-1]for line in globals_report.splitlines()}!=set(exports):raise ValueError('depth archive has non-adapter global definitions')
 lib=out/'libnn_depth_fdl.a';run([tools/'llvm-ar','rcs',lib,partial]);shared=out/'libnn_depth_fdl.so';run([cc,'-shared',partial,'-Wl,--no-undefined','-Wl,--gc-sections','-lm','-o',shared])
 defined=run([tools/'llvm-nm','--defined-only',partial]);dynamic_exports=run([tools/'llvm-nm','-D','--defined-only',shared]);dynamic=run([tools/'llvm-readelf','-d',shared]);elf=run([tools/'llvm-readelf','-h',shared]);
 if set(x.split()[-1]for x in dynamic_exports.splitlines())!=set(exports)or'AArch64'not in elf:raise ValueError('wrong depth metadata ELF/exports')
 if set(re.findall(r'Shared library: \[(.*?)\]',dynamic))!={'libm.so','libdl.so','libc.so'}:raise ValueError('unexpected driver/runtime dependency')
 forbidden=['unpack','decode','pack_rgba','copy_rect','tiled_memcpy','ir3_','nir_','vulkan','gallium','util_format_read','util_format_write']
 if any(word in line.split()[-1]for line in defined.splitlines()if line.split()for word in forbidden):raise ValueError('pixel/compiler/runtime code entered depth metadata library')
 # Link both complete real static libraries, not only currently referenced APIs.
 # DSO export filtering alone cannot prevent duplicate hidden GLOBAL symbols.
 fdl_archive=Path(record['static_library']);run([cc,'-shared','-Wl,--whole-archive',lib,fdl_archive,'-Wl,--no-whole-archive','-Wl,--no-undefined','-lm','-o',out/'libdepth-plus-runtime-fdl-whole-archive.so'])
 # Independently compile owner with the real Android KGSL backend; do not run it.
 cflags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-fPIC','-ffunction-sections','-fdata-sections','-I'+str(own/'include'),'-I'+str(own/'third_party/kgsl')]
 owner=out/'depth_attachment.o';platform=out/'kgsl.o';run([cc,*cflags,'-c',own/'src/depth_attachment.c','-o',owner]);run([cc,*cflags,'-c',own/'src/kgsl.c','-o',platform]);run([cc,'-shared',owner,platform,lib,'-Wl,--no-undefined','-Wl,--gc-sections','-lm','-o',out/'libnn_depth_owner_actual_kgsl.so'])
 # Normal HOST reference/link flags are an actual recorded FDL command, not a
 # guessed recreation of Mesa build configuration.
 original_commands=json.loads((source_out/'commands.json').read_text());host_results=[];owner_results=[]
 for compiler,ccompiler in [('g++','gcc'),('clang++','clang')]:
  template=next(c for c in original_commands if c['argv'][0]==compiler);a=template['argv'];start=a.index('-I'+str(fdl_root/'nvn-native/include'));end=a.index('-Wl,--as-needed');prefix=a[1:start];libs=a[end:-2];cwd=Path(template['cwd'])
  san=['-fsanitize=address,undefined','-fno-omit-frame-pointer']if compiler=='g++'else['-fsanitize=function','-fsanitize-trap=function']
  cpp_flags=[*prefix,'-I'+str(own/'include'),'-I'+str(own/'third_party/nvn'),*san,'-fno-pie','-no-pie']
  binary=out/('metadata-'+ccompiler);run([compiler,*cpp_flags,own/'tests/depth_fdl.cc',own/'src/depth_fdl.cc',*libs,'-o',binary],cwd);host_results.append(json.loads(run([binary],cwd)))
  cobj=[]
  for name in ['src/depth_attachment.c','tests/d2r_bridge/depth_attachment.c']:
   obj=out/(ccompiler+'-'+name.replace('/','-')+'.o');run([ccompiler,*cflags,*san,'-fno-pie','-c',own/name,'-o',obj]);cobj.append(obj)
  binary=out/('owner-'+ccompiler);run([compiler,*cpp_flags,own/'src/depth_fdl.cc',*cobj,*libs,'-o',binary],cwd);owner_results.append(json.loads(run([binary],cwd)))
  for c in [compiler,ccompiler]:p=Path(shutil.which(c)).resolve();inputs[str(p)]=sha(p)
 if len(host_results)!=2 or host_results[0]!=host_results[1]or host_results[0]['layout_cases']!=504 or host_results[0]['negative_cases']!=32:raise ValueError('HOST metadata reference results differ')
 if len(owner_results)!=2 or owner_results[0]!=owner_results[1]or any(owner_results[0][k]for k in ('pixel_map_calls','cache_calls','live_owners')):raise ValueError('HOST owner reference results differ')
 # Same C fault fixture also must strictly link as AArch64; not Android executed.
 mock=out/'ndk-owner-fixture.o';run([cc,*cflags,'-c',own/'tests/d2r_bridge/depth_attachment.c','-o',mock]);run([cc,mock,owner,lib,'-Wl,--no-undefined','-lm','-o',out/'owner-api29-not-executed'])
 # Actual new header dependency closure (includes original FDL headers/config).
 import shlex
 for word in shlex.split((out/'depth_fdl.d').read_text().replace('\\\n',' ').split(':',1)[1]):
  p=(source_out/'android-build'/word).resolve()
  if p.is_file():inputs[str(p)]=sha(p)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('input changed during build '+p)
 result=dict(schema=1,kind='native-depth-fdl-foundation-v1',passed=True,host_only=True,android_execution=False,gpu_execution=False,fdl_check=str(fdl_check),fdl_root=str(fdl_root),format_roles={'depth_only':[0x32,0x33,0x34],'classified_not_admitted':[0x31,0x35,0x36]},tile_mode='actual TILE6_3/nonUBWC/sample1',host_metadata=host_results,host_owner=owner_results,static_library=str(lib),global_definitions=globals_report.splitlines(),whole_archive_coexistence=True,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()},scope='Actual original FDL metadata + HOST ownership and real KGSL API29 strict link; no depth attachment GPU admission, no CPU pixel access')
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--fdl-root',type=Path,required=True);p.add_argument('--fdl-check',type=Path,required=True);p.add_argument('--ndk',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();print(produce(*[getattr(a,n).resolve()for n in ('root','fdl_root','fdl_check','ndk','out')]))

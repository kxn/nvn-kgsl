#!/usr/bin/env python3
"""Build original Mesa pure metadata/layout/view for Android ARM64 API29.

Fresh original Meson cross-config; never copy HOST config, compile a driver,
execute Android code, map pixels or generate a texture layout implementation.
"""
import argparse,hashlib,importlib.util,json,os,re,shlex,shutil,subprocess,tarfile
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p):
 s=importlib.util.spec_from_file_location('nn_fdl_normal_closure',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def flags(entry):
 a=shlex.split(entry['command']);out=[];skip=False
 for v in a[1:]:
  if skip:skip=False;continue
  if v in ('-o','-MQ','-MF'):skip=True;continue
  if v in ('-MD','-c')or v==entry['file']:continue
  out.append(v)
 return a[0],out

def output(entry):
 a=shlex.split(entry['command']);return a[a.index('-o')+1]

def produce(root,ir3,ndk,out,jobs):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored directory below ROOT/.local required')
 common=module(root/'nvn-native/tools/compute_driverparams.py');normal,inputs,_,count=common.compiler_inputs(root,ir3)
 source=ir3/'source/mesa-26.2.5';tools=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin';cc=tools/'aarch64-linux-android29-clang';cpp=tools/'aarch64-linux-android29-clang++'
 own=root/'nvn-native';wrapper=own/'src/runtime_fdl.cc';header=own/'include/nvn_native/runtime_fdl.h';fixture=own/'tests/runtime_fdl.cc'
 for p in [ir3/'build/compile_commands.json',Path(__file__).resolve(),root/'nvn-native/tools/compute_driverparams.py',wrapper,header,own/'include/nvn_native/texture_buffer_format.h',fixture,own/'third_party/nvn/nvn.h',ndk/'source.properties',tools/'ld.lld',tools/'llvm-strip',tools/'llvm-ar',tools/'llvm-readelf',tools/'llvm-nm',cc,cpp]:inputs[str(p)]=sha(p)
 meson=next(p for p in normal['tools']if Path(p).name=='meson');ninja=next(p for p in normal['tools']if Path(p).name=='ninja')
 env=os.environ.copy();env['PATH']=':'.join(dict.fromkeys([str(Path(p).parent)for p in normal['tools']if Path(p).name in ('meson','bison','flex','m4')]))+':'+env['PATH']
 out.mkdir(parents=True);commands=[]
 def run(cmd,cwd=None):
  r=subprocess.run(list(map(str,cmd)),cwd=cwd,text=True,capture_output=True,env=env);commands.append(dict(argv=list(map(str,cmd)),cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stdout+r.stderr)
  return r.stdout
 cross=out/'android-api29.ini';cross.write_text("[binaries]\nc = '"+str(cc)+"'\ncpp = '"+str(cpp)+"'\nar = '"+str(tools/'llvm-ar')+"'\nstrip = '"+str(tools/'llvm-strip')+"'\npkg-config = '/usr/bin/false'\n[host_machine]\nsystem = 'android'\ncpu_family = 'aarch64'\ncpu = 'armv8-a'\nendian = 'little'\n[properties]\nneeds_exe_wrapper = true\n[built-in options]\nc_args = ['-ffunction-sections','-fdata-sections']\ncpp_args = ['-ffunction-sections','-fdata-sections']\n")
 build=out/'android-build';run([meson,'setup',build,source,'--cross-file',cross,'--wrap-mode=nodownload','-Dbuildtype=debugoptimized','-Dwerror=true','-Dplatforms=[]','-Dgallium-drivers=[]','-Dvulkan-drivers=[]','-Dfreedreno-kmds=[]','-Dopengl=false','-Dgles1=disabled','-Dgles2=disabled','-Degl=disabled','-Dglx=disabled','-Dgbm=disabled','-Dllvm=disabled','-Dtools=freedreno,nir','-Dzstd=disabled','-Dzlib=disabled','-Dspirv-tools=disabled','-Dbuild-tests=false','-Dshader-cache=disabled','-Dlibunwind=disabled','-Dvalgrind=disabled','-Dexpat=disabled','-Dselinux=false'])
 entries=json.loads((build/'compile_commands.json').read_text());suffixes=['/fd6_format_table.c','/fd6_layout.c','/fd6_view.cc','/freedreno_layout.c','/freedreno_dev_info.c','/format/u_format.c','/format/u_format_table.c'];selected=[next(e for e in entries if e['file'].endswith(s))for s in suffixes]
 objects=[build/output(e)for e in selected];run([ninja,'-C',build,'-j'+str(jobs),*[str(p.relative_to(build))for p in objects]])
 deptext=run([ninja,'-C',build,'-t','deps',*[str(p.relative_to(build))for p in objects]]);(out/'actual-object-deps.txt').write_text(deptext)
 crossdeps={}
 for line in deptext.splitlines():
  if line.startswith('    '):
   p=(build/line.strip()).resolve()
   if p.is_file():crossdeps[str(p)]=sha(p)
 wcompiler,wflags=flags(next(e for e in selected if e['file'].endswith('/fd6_view.cc')));wo=out/'wrapper.o';wd=out/'wrapper.d';run([wcompiler,*wflags,'-I'+str(own/'include'),'-Werror','-MD','-MF',wd,'-c',wrapper,'-o',wo],build)
 for word in shlex.split(wd.read_text().replace('\\\n',' ').split(':',1)[1]):
  p=(build/word).resolve()
  if p.is_file():crossdeps[str(p)]=sha(p)
 for e in selected:
  p=(build/e['file']).resolve();crossdeps[str(p)]=sha(p)
 crossdeps[str(wrapper)]=sha(wrapper);crossdeps[str(header)]=sha(header)
 def link(objs,dest):
  dest.mkdir(parents=True,exist_ok=True);partial=dest/'metadata.o';run([tools/'ld.lld','-r','--gc-sections','-u','nn_fdl_image','-u','nn_fdl_buffer','-u','nn_fdl_metadata_version','-u','nn_fdl_volume','-u','nn_fdl_volume_metadata_version','-u','nn_fdl_image_format_info',*objs,'--Map='+str(dest/'metadata.map'),'-o',partial]);run([tools/'llvm-strip','--strip-debug',partial]);archive=dest/'libnn_fdl_metadata.a';run([tools/'llvm-ar','rcs',archive,partial]);so=dest/'libnn_fdl_metadata.so';run([cc,'-shared',partial,'-Wl,--no-undefined','-Wl,--gc-sections','-Wl,-soname,libnn_fdl_metadata.so','-lm','-o',so]);run([tools/'llvm-strip','--strip-debug',so])
  defined=run([tools/'llvm-nm','--defined-only',partial]);exports=run([tools/'llvm-nm','-D','--defined-only',so]);dynamic=run([tools/'llvm-readelf','-d',so]);undefined=run([tools/'llvm-nm','-u',so]);elf=run([tools/'llvm-readelf','-h',so]);
  if 'AArch64'not in elf or {r.split()[-1]for r in exports.splitlines()}!={'nn_fdl_image','nn_fdl_buffer','nn_fdl_metadata_version','nn_fdl_volume','nn_fdl_volume_metadata_version','nn_fdl_image_format_info'}:raise ValueError('wrong Android library ABI/exports')
  if set(re.findall(r'Shared library: \[(.*?)\]',dynamic))!={'libm.so','libdl.so','libc.so'}:raise ValueError('non-metadata dependency entered library')
  forbidden=['unpack','decode','pack_rgba','copy_rect','memcpy_tiled','tiled_memcpy','ir3_','nir_','vk','vulkan','gallium','glDraw','util_format_read','util_format_write']
  names=[r.split()[-1]for r in defined.splitlines()if r.split()]
  if any(word in name for word in forbidden for name in names):raise ValueError('pixel/driver/compiler implementation entered metadata library')
  reports=dict(defined=defined,exports=exports,dynamic=dynamic,undefined=undefined,elf=elf);(dest/'elf-report.json').write_text(json.dumps(reports,indent=2)+'\n');return reports
 android_report=link([wo,*objects],out/'library')
 # Vendored closure contains original source/headers + Android-generated
 # headers/config, not a HOST config. External NDK dependencies remain pinned.
 vendor=out/'source-closure';vendor.mkdir();mapping={};external={}
 for name,h in crossdeps.items():
  p=Path(name)
  if p.is_relative_to(source):rel=Path('mesa')/p.relative_to(source)
  elif p.is_relative_to(build):rel=Path('android-generated')/p.relative_to(build)
  elif p==wrapper:rel=Path('adapter/src/runtime_fdl.cc')
  elif p.is_relative_to(own/'include'):rel=Path('adapter/include')/p.relative_to(own/'include')
  else:external[name]=h;continue
  q=vendor/rel;q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(p.read_bytes());mapping[name]=str(rel)
 for rel in ['COPYING','docs/license.rst','licenses/MIT']:
  p=source/rel
  if p.exists():q=vendor/'licenses'/rel;q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(p.read_bytes());inputs[str(p)]=sha(p)
 # Compile the isolated closure again with captured actual Android flags. This
 # proves source/config completeness, not Android execution or pixel behavior.
 rebuilt=[];recipes=[]
 def rewrite(path):
  p=Path(path)
  if p.is_relative_to(source):return vendor/'mesa'/p.relative_to(source)
  if p.is_relative_to(build):return vendor/'android-generated'/p.relative_to(build)
  if p==own/'include':return vendor/'adapter/include'
  return p
 for i,(compiler,fs,src)in enumerate([(wcompiler,[*wflags,'-I'+str(own/'include')],wrapper),*[(flags(e)[0],flags(e)[1],(build/e['file']).resolve())for e in selected]]):
  argv=[]
  for v in fs:
   if v.startswith('-I'):v='-I'+str(rewrite((build/v[2:]).resolve()))
   argv.append(v)
  dst=out/'vendor-rebuild'/('source'+str(i)+'.o');dst.parent.mkdir(exist_ok=True);cmd=[compiler,*argv,'-Werror','-c',vendor/mapping[str(src)],'-o',dst];run(cmd,build);rebuilt.append(dst);recipes.append(dict(source=mapping[str(src)],compiler=compiler,flags=argv))
 (vendor/'compile-recipes.json').write_text(json.dumps(recipes,indent=2)+'\n');rebuilt_report=link(rebuilt,out/'vendor-rebuild/library')
 if android_report['exports'].splitlines()!=rebuilt_report['exports'].splitlines():
  if {x.split()[-1]for x in android_report['exports'].splitlines()}!={x.split()[-1]for x in rebuilt_report['exports'].splitlines()}:raise ValueError('closure rebuild exports differ')
 # HOST reference uses original normal FDL test flags/libraries, not Android
 # execution. Facade+fixture sanitizer instrumentation; FDL normal libs retain
 # their recorded normal build configuration.
 host=ir3/'build';he=next(e for e in json.loads((host/'compile_commands.json').read_text())if e['file'].endswith('/fd6_view.cc'));hc,hf=flags(he)
 hl=shlex.split(run([ninja,'-C',host,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
 for v in hl[1:]:
  if skip:skip=False;continue
  if v=='-o':skip=True;continue
  if v.endswith('.o'):continue
  libs.append(v)
 for v in libs:
  p=(host/v).resolve()
  if p.is_file():inputs[str(p)]=sha(p)
 for compiler,san in [('g++',['-fsanitize=address,undefined','-fno-omit-frame-pointer']),('clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  use_flags=hf if compiler=='g++'else[x for x in hf if x not in ('-flifetime-dse=1','-Wno-error=maybe-uninitialized')]
  binary=out/('host-'+compiler.replace('+','p'));run([compiler,*use_flags,'-Werror','-I'+str(own/'include'),'-I'+str(own/'third_party/nvn'),*san,'-fno-pie','-no-pie',fixture,wrapper,*libs,'-o',binary],host);run([binary],host);inputs[str(Path(shutil.which(compiler)).resolve())]=sha(Path(shutil.which(compiler)).resolve())
 manifest=json.loads((ir3/'source-hashes.json').read_text())
 for name,h in crossdeps.items():
  p=Path(name)
  if p.is_relative_to(source)and manifest.get(str(p.relative_to(source)))!=h:raise ValueError('cross source differs from fixed original compiler source '+name)
 inputs.update(crossdeps)
 for p in [cross,build/'compile_commands.json',build/'build.ninja',build/'meson-info/intro-buildoptions.json',build/'meson-info/intro-buildsystem_files.json',build/'meson-logs/meson-log.txt']:
  inputs[str(p)]=sha(p)
 for name in json.loads((build/'meson-info/intro-buildsystem_files.json').read_text()):
  p=Path(name)
  if p.is_file():
   h=sha(p)
   if p.is_relative_to(source)and manifest.get(str(p.relative_to(source)))!=h:raise ValueError('Meson source differs from fixed manifest '+str(p))
   inputs[str(p)]=h
 closure=dict(schema=1,source_mapping=mapping,external_ndk_deps=external,original_source_hashes=crossdeps,scope='actual minimal compiled source/header closure; Android generated config is isolated, not approved for copying into production',android_execution=False)
 (vendor/'closure.json').write_text(json.dumps(closure,indent=2)+'\n');tar=out/'metadata-source-closure.tar.gz'
 with tarfile.open(tar,'w:gz')as t:t.add(vendor,arcname='metadata-source-closure')
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('input changed during build '+p)
 artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()and not p.is_relative_to(build)and not p.is_relative_to(vendor)}
 result=dict(schema=1,kind='android-runtime-fdl-metadata-v1',passed=True,android_execution=False,gpu_execution=False,normal_compiler_source_files=count,source=str(source),normal_ir3=str(ir3),ndk=str(ndk),source_units=7,features=['image-v1','buffer-v1','volume-v1'],inputs=inputs,artifacts=artifacts,header=str(header),static_library=str(out/'library/libnn_fdl_metadata.a'),shared_library=str(out/'library/libnn_fdl_metadata.so'),vendored_source_files=len(mapping),external_ndk_header_files=len(external),scope='dynamic CPU FDL metadata only; HOST reference and Android ELF compile/link, not Android runtime/GPU qualification')
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'

def verify(root,path):
 c=json.loads(path.read_text())
 if c.get('schema')!=1 or c.get('kind')!='android-runtime-fdl-metadata-v1'or c.get('android_execution')is not False or c.get('gpu_execution')is not False:raise ValueError('unknown runtime FDL package')
 for p,h in {**c['inputs'],**c['artifacts']}.items():
  if sha(p)!=h:raise ValueError('FDL source/config/product changed '+p)
 for rel in ['tools/build_runtime_fdl.py','src/runtime_fdl.cc','include/nvn_native/runtime_fdl.h','include/nvn_native/texture_buffer_format.h','tests/runtime_fdl.cc']:
  p=root/'nvn-native'/rel
  if c['inputs'].get(str(p))!=sha(p):raise ValueError('runtime FDL package from different current source')
 return Path(c['static_library']),c
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);p.add_argument('--ir3',required=True,type=Path);p.add_argument('--ndk',required=True,type=Path);p.add_argument('--out',required=True,type=Path);p.add_argument('--jobs',type=int,default=8);a=p.parse_args();print(produce(a.root.resolve(),a.ir3.resolve(),a.ndk.resolve(),a.out.resolve(),a.jobs))

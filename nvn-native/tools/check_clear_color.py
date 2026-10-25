#!/usr/bin/env python3
"""Original Mesa one-texel clear constants + native HOST/Android admission.

Fresh Android Meson config; original packers are linked separately from pure
FDL metadata. No CPU image processing, Android execution or GPU claim.
"""
import argparse,hashlib,importlib.util,json,os,re,shlex,shutil,subprocess,tarfile
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def flags(e):
 a=shlex.split(e['command']);fs=[];skip=False
 for v in a[1:]:
  if skip:skip=False;continue
  if v in ('-o','-MQ','-MF'):skip=True;continue
  if v in ('-MD','-c')or v==e['file']:continue
  fs.append(v)
 return a[0],fs
def obj(e):
 a=shlex.split(e['command']);return a[a.index('-o')+1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);p.add_argument('--ir3',required=True,type=Path);p.add_argument('--kernel-check',required=True,type=Path);p.add_argument('--ndk',required=True,type=Path);p.add_argument('--out',required=True,type=Path);p.add_argument('--jobs',type=int,default=6);p.add_argument('--fdl-check',type=Path,help='Optional normal FDL archive: verify strict combined static link');a=p.parse_args()
 root=a.root.resolve();n=root/'nvn-native';ir3=a.ir3.resolve();out=a.out.resolve();ndk=a.ndk.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 normal,inputs,_,source_count=load(n/'tools/compute_driverparams.py','clear_normal').compiler_inputs(root,ir3)
 kernel=load(n/'tools/compile_clear_color.py','clear_kernel').verify(root,a.kernel_check.resolve());inputs.update(kernel['inputs']);inputs.update(kernel['artifacts']);inputs[str(a.kernel_check.resolve())]=sha(a.kernel_check)
 source=ir3/'source/mesa-26.2.5';host=ir3/'build';tools=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin';cc=tools/'aarch64-linux-android29-clang';cpp=tools/'aarch64-linux-android29-clang++'
 meson=next(Path(x)for x in normal['tools']if Path(x).name=='meson');ninja=next(Path(x)for x in normal['tools']if Path(x).name=='ninja')
 env=os.environ.copy();env['PATH']=':'.join(dict.fromkeys(str(Path(x).parent)for x in normal['tools']if Path(x).name in ('meson','bison','flex','m4')))+':'+env['PATH']
 out.mkdir(parents=True);commands=[];results=[];proof=dict(schema=1,kind='native-clear-color-constants-v1',passed=False,gpu_execution=False,android_execution=False,scope='ONE-texel Mesa constants; native layout/owner metadata and actual IR3 reflection, not NVN/GPU qualification')
 def run(argv,cwd=None):
  q=subprocess.run(list(map(str,argv)),cwd=cwd,env=env,capture_output=True,text=True);commands.append(dict(argv=list(map(str,argv)),cwd=str(cwd)if cwd else None,returncode=q.returncode,stdout=q.stdout,stderr=q.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if q.returncode:raise ValueError(q.stdout+q.stderr)
  return q.stdout
 cross_deps=set()
 def deps(path,base):
  for x in shlex.split(path.read_text().replace('\\\n',' ').split(':',1)[1]):
   p=(base/x).resolve()
   if p.is_file():inputs[str(p)]=sha(p);cross_deps.add(str(p))
 try:
  cross=out/'android-api29.ini';cross.write_text("[binaries]\nc = '"+str(cc)+"'\ncpp = '"+str(cpp)+"'\nar = '"+str(tools/'llvm-ar')+"'\nstrip = '"+str(tools/'llvm-strip')+"'\npkg-config = '/usr/bin/false'\n[host_machine]\nsystem = 'android'\ncpu_family = 'aarch64'\ncpu = 'armv8-a'\nendian = 'little'\n[properties]\nneeds_exe_wrapper = true\n[built-in options]\nc_args = ['-ffunction-sections','-fdata-sections']\ncpp_args = ['-ffunction-sections','-fdata-sections']\n")
  build=out/'android-build';run([meson,'setup',build,source,'--cross-file',cross,'--wrap-mode=nodownload','-Dbuildtype=debugoptimized','-Dwerror=true','-Dplatforms=[]','-Dgallium-drivers=[]','-Dvulkan-drivers=[]','-Dfreedreno-kmds=[]','-Dopengl=false','-Dgles1=disabled','-Dgles2=disabled','-Degl=disabled','-Dglx=disabled','-Dgbm=disabled','-Dllvm=disabled','-Dtools=freedreno,nir','-Dzstd=disabled','-Dzlib=disabled','-Dspirv-tools=disabled','-Dbuild-tests=false','-Dshader-cache=disabled','-Dlibunwind=disabled','-Dvalgrind=disabled','-Dexpat=disabled','-Dselinux=false'])
  entries=json.loads((build/'compile_commands.json').read_text());suffix=['/format/u_format_table.c','/format/u_format_other.c','/half_float.c','/format_srgb.c','/softfloat.c'];selected=[next(e for e in entries if e['file'].endswith(s))for s in suffix];objects=[build/obj(e)for e in selected]
  run([ninja,'-C',build,'-j'+str(a.jobs),*[str(x.relative_to(build))for x in objects]])
  dt=run([ninja,'-C',build,'-t','deps',*[str(x.relative_to(build))for x in objects]]);(out/'actual-object-deps.txt').write_text(dt)
  for line in dt.splitlines():
   if line.startswith('    '):
    x=(build/line.strip()).resolve()
    if x.is_file():inputs[str(x)]=sha(x);cross_deps.add(str(x))
  wcc,wf=flags(selected[1]);wrapper=n/'src/clear_color_pattern.c';wo=out/'pattern.o';wd=out/'pattern.d';inc=['-I'+str(n/'include'),'-I'+str(n/'third_party/nvn')]
  run([wcc,*wf,*inc,'-MD','-MF',wd,'-Werror','-c',wrapper,'-o',wo],build);deps(wd,build)
  partial=out/'clear-pattern.o';run([tools/'ld.lld','-r','--gc-sections','-u','nn_clear_color_pattern',wo,*objects,'--Map='+str(out/'clear-pattern.map'),'-o',partial]);run([tools/'llvm-strip','--strip-debug',partial])
  # Hidden GLOBAL is private in a DSO but still collides when two relocatable
  # closures contain Mesa metadata. Localize all hidden implementation symbols
  # during normal archive generation; only the adapter retains global linkage.
  run([tools/'llvm-objcopy','--localize-hidden',partial]);globals=run([tools/'llvm-nm','-g','--defined-only',partial])
  if {x.split()[-1]for x in globals.splitlines()}!={'nn_clear_color_pattern'}:raise ValueError('constant static archive leaks implementation globals')
  archive=out/'libnn_clear_color_pattern.a';run([tools/'llvm-ar','rcs',archive,partial])
  # Only the ONE-texel adapter is public. Unused decoders/codecs/drivers are
  # discarded; generated format-description metadata is harmless and retained.
  pattern_so=out/'libnn_clear_color_pattern.so';run([cc,'-shared',partial,'-Wl,--no-undefined','-lm','-o',pattern_so]);defined=run([tools/'llvm-nm','--defined-only',partial]);exports=run([tools/'llvm-nm','-D','--defined-only',pattern_so]);dynamic=run([tools/'llvm-readelf','-d',pattern_so])
  if {x.split()[-1]for x in exports.splitlines()}!={'nn_clear_color_pattern'}:raise ValueError('wrong constant adapter ABI')
  forbidden=['unpack','fetch_rgba','decode','util_format_read','util_format_write','ir3_','nir_','vulkan','vkCreate','glDraw','pipe_context']
  if any(word in name for word in forbidden for name in [x.split()[-1]for x in defined.splitlines()if x.split()]):raise ValueError('CPU image/driver implementation retained')
  if set(re.findall(r'Shared library: \[(.*?)\]',dynamic))!={'libm.so','libdl.so','libc.so'}:raise ValueError('unexpected constant library dependency')
  (out/'pattern-elf.json').write_text(json.dumps(dict(defined=defined,exports=exports,dynamic=dynamic),indent=2)+'\n')
  he=next(e for e in json.loads((host/'compile_commands.json').read_text())if e['file'].endswith('/format/u_format_other.c'));_,hf=flags(he)
  lib=host/'src/util/libmesa_util.a';inputs[str(lib)]=sha(lib)
  own=[n/'tests/clear_color.c',n/'src/clear_color.c',wrapper,n/'src/texture_source.c'];header=Path(kernel['header'])
  for compiler,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
   use=hf if compiler=='gcc'else[x for x in hf if x not in ('-Wno-error=maybe-uninitialized','-Wno-nonnull-compare')]
   binary=out/('host-'+compiler);run([compiler,*use,*inc,'-Werror','-DNN_CLEAR_COLOR_HEADER='+json.dumps(str(header)),*san,'-fno-pie','-no-pie',*own,lib,'-lm','-ldl','-lpthread','-o',binary],host)
   value=json.loads(run([binary],host));assert value['passed'];results.append(dict(compiler=compiler,**value));inputs[str(Path(shutil.which(compiler)).resolve())]=sha(Path(shutil.which(compiler)).resolve())
  native=out/'clear-real-kgsl-api29.so';run([cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-fPIC','-shared',*inc,'-I'+str(n/'third_party/kgsl'),n/'src/clear_color.c',n/'src/texture_source.c',n/'src/kgsl.c',archive,'-Wl,--no-undefined','-Wl,--gc-sections','-landroid','-ldl','-lm','-o',native]);elf=run([tools/'llvm-readelf','-h','-d',native]);assert'AArch64'in elf
  if a.fdl_check:
   fdl_check=a.fdl_check.resolve();library,fdl=load(n/'tools/build_runtime_fdl.py','clear_combined_fdl').verify(root,fdl_check)
   inputs.update({str(fdl_check):sha(fdl_check),**fdl['inputs'],**fdl['artifacts']});inputs[str(n/'tools/build_runtime_fdl.py')]=sha(n/'tools/build_runtime_fdl.py')
   combined=out/'clear-fdl-combined-api29.so'
   run([cc,'-shared','-Wl,--no-undefined','-Wl,--whole-archive',archive,library,'-Wl,--no-whole-archive','-lm','-o',combined])
   # Include the sampled-format metadata API declared by runtime_fdl.h. Keep
   # an exact export set: sharing the archive must not retain Mesa internals.
   symbols=run([tools/'llvm-nm','-D','--defined-only',combined]);expected={'nn_clear_color_pattern','nn_fdl_image','nn_fdl_image_format_info','nn_fdl_buffer','nn_fdl_metadata_version','nn_fdl_volume','nn_fdl_volume_metadata_version'}
   if {x.split()[-1]for x in symbols.splitlines()}!=expected:raise ValueError('combined clear/FDL API differs')
   proof['combined_fdl_link']=dict(passed=True,fdl_check=str(fdl_check),library=str(library),exports=sorted(expected),android_execution=False)
  # Capture and recompile the actual minimal original+Android-generated closure.
  vendor=out/'source-closure';vendor.mkdir();mapping={};external={}
  for name,h in list(inputs.items()):
   if name not in cross_deps:continue
   x=Path(name)
   if x.is_relative_to(source):rel=Path('mesa')/x.relative_to(source)
   elif x.is_relative_to(build):rel=Path('android-generated')/x.relative_to(build)
   elif x==wrapper:rel=Path('adapter/src/clear_color_pattern.c')
   elif x.is_relative_to(n/'include'):rel=Path('adapter/include')/x.relative_to(n/'include')
   elif x.is_relative_to(n/'third_party/nvn'):rel=Path('adapter/nvn')/x.relative_to(n/'third_party/nvn')
   else:external[name]=h;continue
   dst=vendor/rel;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(x.read_bytes());mapping[name]=str(rel)
  # Include original selected source units even if Ninja deps omitted their
  # source argument, as happens for already-up-to-date generated units.
  for e in selected:
   x=(build/e['file']).resolve();rel=Path('mesa')/x.relative_to(source)if x.is_relative_to(source)else Path('android-generated')/x.relative_to(build)
   dst=vendor/rel;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(x.read_bytes());mapping[str(x)]=str(rel);inputs[str(x)]=sha(x)
  def rewrite(x):
   if x.is_relative_to(source):return vendor/'mesa'/x.relative_to(source)
   if x.is_relative_to(build):return vendor/'android-generated'/x.relative_to(build)
   if x.is_relative_to(n/'include'):return vendor/'adapter/include'/x.relative_to(n/'include')
   if x.is_relative_to(n/'third_party/nvn'):return vendor/'adapter/nvn'/x.relative_to(n/'third_party/nvn')
   return x
  rebuilt=[];recipes=[]
  for i,(compiler,fs,x)in enumerate([(wcc,[*wf,*inc],wrapper),*[(flags(e)[0],flags(e)[1],(build/e['file']).resolve())for e in selected]]):
   fs=['-I'+str(rewrite((build/v[2:]).resolve()))if v.startswith('-I')else v for v in fs];dst=out/('vendor'+str(i)+'.o');run([compiler,*fs,'-Werror','-c',vendor/mapping[str(x)],'-o',dst],build);rebuilt.append(dst);recipes.append(dict(source=mapping[str(x)],compiler=compiler,flags=fs))
  rp=out/'vendor-pattern.o';run([tools/'ld.lld','-r','--gc-sections','-u','nn_clear_color_pattern',*rebuilt,'-o',rp]);run([tools/'llvm-objcopy','--localize-hidden',rp]);vendor_globals=run([tools/'llvm-nm','-g','--defined-only',rp])
  if {x.split()[-1]for x in vendor_globals.splitlines()}!={'nn_clear_color_pattern'}:raise ValueError('vendored closure static ABI differs')
  run([cc,'-shared',rp,'-Wl,--no-undefined','-lm','-o',out/'vendor-pattern.so'])
  for rel in ['COPYING','docs/license.rst','licenses/MIT']:
   x=source/rel
   if x.is_file():dst=vendor/'licenses'/rel;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(x.read_bytes());inputs[str(x)]=sha(x)
  (vendor/'closure.json').write_text(json.dumps(dict(source_mapping=mapping,external_deps=external,recipes=recipes,scope='original Mesa pack ONE constant; no CPU image conversion'),indent=2)+'\n')
  with tarfile.open(out/'clear-pattern-source-closure.tar.gz','w:gz')as t:t.add(vendor,arcname='clear-pattern-source-closure')
  manifest=json.loads((ir3/'source-hashes.json').read_text())
  for name,h in inputs.items():
   x=Path(name)
   if x.is_relative_to(source)and manifest.get(str(x.relative_to(source)))!=h:raise ValueError('original Mesa source changed '+name)
  proof.update(passed=True,static_library=str(archive),static_public_symbols=['nn_clear_color_pattern'],implementation_symbols_local=True,constant_functions=22,HOST=results,normal_compiler_source_files=source_count,source_closure_files=len(mapping))
 except Exception as error:proof['error']=str(error)
 finally:
  for x in [Path(__file__).resolve(),*sorted((n/'include/nvn_native').glob('*.h')),n/'src/clear_color.c',n/'src/clear_color_pattern.c',n/'src/texture_source.c',n/'src/texture_volume.inc',n/'src/kgsl.c',n/'tests/clear_color.c',n/'third_party/nvn/nvn.h',ndk/'source.properties',cc,cpp,*[tools/x for x in ['llvm-ar','llvm-strip','llvm-objcopy','llvm-nm','llvm-readelf','ld.lld']]]:inputs[str(x)]=sha(x)
  for x,h in inputs.items():
   if sha(x)!=h:proof.update(passed=False,error='input changed during check '+x);break
  proof.update(inputs=inputs,commands=commands,artifacts={str(x):sha(x)for x in out.rglob('*')if x.is_file()and not x.is_relative_to(out/'android-build')and not x.is_relative_to(out/'source-closure')})
  (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n')
 print(json.dumps(dict(passed=proof['passed'],error=proof.get('error'),check=str(out/'check.json'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())

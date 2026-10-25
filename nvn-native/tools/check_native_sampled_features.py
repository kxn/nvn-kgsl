#!/usr/bin/env python3
"""HOST policy/forwarding + actual original36source ABI1/ABI2 NDK linkage. No game/GPU."""
import argparse,copy,importlib.util,json,shlex,shutil,subprocess,sys
from pathlib import Path
from workspace_paths import d2r_root
from native_sampled_features import prepare,audit,validate_options,sha
from bc_sampled_feature import prepare as bc_prepare,audit as bc_audit

def load(p,name):
 sys.path.insert(0,str(p.parent))
 try:
  s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
 finally:sys.path.pop(0)

def check(root,ndk,fdl,graphics,bridge,block,names,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 own=Path(__file__).resolve().parents[1];out.mkdir(parents=True);inputs={};commands=[];assertions=0;rejections=[]
 def pin(p):
  p=Path(p).resolve();h=sha(p)
  if str(p)in inputs and inputs[str(p)]!=h:raise ValueError('input changed '+str(p))
  inputs[str(p)]=h
 def require(value):
  nonlocal assertions
  if not value:raise ValueError('sampled feature HOST assertion failed')
  assertions+=1
 def reject(name,fn):
  try:fn()
  except (ValueError,OSError,KeyError):rejections.append(name);return
  raise ValueError('invalid sampled feature accepted '+name)
 def run(argv,cwd=None,expected=0):
  r=subprocess.run(list(map(str,argv)),cwd=cwd,text=True,capture_output=True);commands.append(dict(argv=list(map(str,argv)),cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode!=expected:raise ValueError((r.stdout+r.stderr)[-8000:])
  return r
 fdltool=load(root/'nvn-native/tools/build_runtime_fdl.py','sampled_feature_fdl');library,fd=fdltool.verify(root,fdl)
 ph,pm=load(root/'nvn-native/tools/graphics_pipeline_cache.py','sampled_feature_graphics').build(root,graphics)
 bh,bm=load(root/'nvn-native/tools/texture_bridge_bundle.py','sampled_feature_bridge').build(root,bridge)
 km=load(root/'nvn-native/tools/compile_block_copy.py','sampled_feature_kernel').verify(root,block);kh=Path(km['header'])
 flags,manifest,mapping=prepare(root,True,True,True,graphics,fdl,block,None,library,None,True);inputs.update(mapping)
 _,bc,bcinputs=bc_prepare(root,fdl,graphics,bridge,block);inputs.update(bcinputs)
 for path,value in ((fdl,fd),(graphics,pm),(bridge,bm),(block,km)):
  pin(path);inputs.update(value['inputs']);inputs.update(value.get('artifacts',value.get('outputs',{})))
 for folder in (own,d2r_root()/'dm/host',d2r_root()/'dm/third_party/runtime-codecs',d2r_root()/'dm/third_party/libdeflate'):
  for p in folder.rglob('*'):
   if p.is_file()and'__pycache__'not in p.parts:pin(p)
 for p in (d2r_root()/'dm/gfx/hle_vi.c',d2r_root()/'dm/gfx/hle_nv.c',names):pin(p)
 require(prepare(root,False,False,False,None,None,None)==(['-DNVN_D2R_SAMPLED_IMAGES=OFF','-DNVN_D2R_SAMPLED_MIP_FILTER=OFF','-DNVN_D2R_GATHER=OFF'],None,{}))
 require(manifest['sampled_images']and manifest['sampled_mip_filter']and manifest['gather']and not manifest['gpu_qualification'])
 for mask in range(7):reject('sampled missing prerequisite '+str(mask),lambda mask=mask:validate_options(True,False,False,graphics if mask&1 else None,fdl if mask&2 else None,block if mask&4 else None,None))
 reject('mip without sampled',lambda:validate_options(False,True,False,graphics,fdl,block,bridge))
 reject('unknown bool',lambda:validate_options(1,False,False,graphics,fdl,block,bridge))
 reject('gather missing graphics',lambda:validate_options(False,False,True,None,fdl,None,None))
 reject('gather missing native views',lambda:validate_options(False,False,True,graphics,None,None,None))
 validate_options(False,False,True,graphics,None,None,bridge);validate_options(False,False,True,graphics,fdl,None,None);validate_options(True,False,True,graphics,fdl,block,None);require(True)
 reject('different target archive',lambda:prepare(root,True,False,False,graphics,fdl,block,None,Path('/unverified/other.a')))
 reject('different BC archive',lambda:prepare(root,True,False,False,graphics,fdl,block,None,library,Path('/unverified/other.a')))
 broken=out/'changed-fdl.json';v=json.loads(fdl.read_text());v['inputs'][next(iter(v['inputs']))]='0'*64;broken.write_text(json.dumps(v));reject('changed FDL source',lambda:prepare(root,True,False,False,graphics,broken,block,None,library))
 fixture=out/'argv-input';fixture.write_bytes(b'HOST verification fixture\n');m={str(fixture):sha(fixture)}
 options=dict(host_abi=2,rewrite_abi='elfdesc',rules='native',private_gates='1',abi='arm64-v8a',execute=False,deploy=False,library_kind='shared',python=sys.executable,exefs='/original/exefs',ndk=str(ndk),jobs=4)
 host=dict(options=options,modules=str(out/'modules'),artifacts=m,sources=m,tools=m)
 apk=load(own/'tools/build_d2r_activity_apk.py','sampled_feature_apk');cli=load(own/'tools/run_d2r_host.py','sampled_feature_cli')
 for mask in range(8):
  h=copy.deepcopy(host);o=h['options'];o.update(sampled_images=bool(mask&1),sampled_mip_filter=bool(mask&2),gather=bool(mask&4))
  for fn,abi,rewrite in ((apk.host_verify,2,'elfdesc'),(cli.host_verify_argv,1,'elf')):
   o.update(host_abi=abi,rewrite_abi=rewrite);a=fn(h,root)
   for k in ('sampled_images','sampled_mip_filter','gather'):require(('--'+k.replace('_','-')in a)==o[k])
  h['options']['gather']='false';reject('unknown forwarded bool '+str(mask),lambda:cli.host_verify_argv(h,root))
 for script in ('build_d2r_host.py','build_d2r_activity.py'):
  base=[sys.executable,own/'tools'/script,'--exefs','/no-original-read','--ndk',ndk,'--out',out/'must-not-exist']
  for options,diagnostic in ((['--sampled-images'],'--sampled-images requires'),(['--sampled-mip-filter'],'--sampled-mip-filter requires'),(['--gather'],'--gather requires')):
   r=run([*base,*options],expected=2);require(diagnostic in r.stderr and not(out/'must-not-exist').exists());rejections.append(script+' '+diagnostic)
 macros=['-DNN_PIPELINE_SAMPLED_IMAGES=1','-DNN_NVN_SAMPLED_IMAGES=1','-DNN_PIPELINE_SAMPLED_MIP_FILTER=1','-DNN_PIPELINE_GATHER=1','-DNN_NVN_BLOCK_COPY=1']
 row=lambda name:shlex.join(['clang',*macros,'-c',str(own/'src'/name),'-o','x.o'])
 bridge_row=shlex.join(['clang','-DNN_NVN_BLOCK_COPY_HEADER="'+str(kh)+'"','-c',str(own/'adapters/d2r/host_bridge.c'),'-o','bridge.o'])
 synthetic='\n'.join(row(x)for x in ('nvn.c','pipeline.c','sampler_state.c','sampled_image.c','texture_bc_shadow.c'))+'\n'+bridge_row+'\n'+shlex.join(['clang','x.o',str(library),'-o','host'])
 audit(own,manifest,synthetic);bc_audit(own,None,synthetic,manifest);require(True)
 for label,text in [('missing image module',synthetic.replace(row('sampled_image.c'),'')),('duplicate ledger',synthetic+'\n'+row('texture_bc_shadow.c')),('missing native macro',synthetic.replace('-DNN_PIPELINE_SAMPLED_IMAGES=1','')),('wrong macro',synthetic.replace('-DNN_PIPELINE_GATHER=1','-DNN_PIPELINE_GATHER=2')),('missing block core',synthetic.replace('-DNN_NVN_BLOCK_COPY=1','')),('missing actual Device kernel',synthetic.replace(bridge_row,'')),('wrong Device kernel',synthetic.replace(str(kh),'/unverified/kernel.h')),('missing staticlib',synthetic.replace(str(library),'/unverified/lib.a')),('qualification source',synthetic+'\nclang -c '+str(own/'tests/bc_sampled_probe.c')+' -o probe.o')]:reject(label,lambda text=text:audit(own,manifest,text))
 reject('unrequested ledger',lambda:bc_audit(own,None,synthetic))
 reject('unrequested BC macro',lambda:bc_audit(own,None,synthetic.replace('clang ','clang -DNN_PIPELINE_BC_SAMPLED=1 '),manifest))
 reject('unknown BC macro',lambda:bc_audit(own,None,synthetic.replace('clang ','clang -DNN_PIPELINE_BC_SAMPLED=2 '),manifest))
 combined=synthetic.replace('clang ','clang -DNN_PIPELINE_BC_SAMPLED=1 ');bc_audit(own,bc,combined,manifest);audit(own,manifest,combined,bc);require(True)
 # Build real original36source binaries, never rewritten modules. Each mode
 # uses the same normal FDL+kernel/source catalogue; no fake compiler target.
 configure_base=['cmake','-S',own,'-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DNVN_D2R_HOST_BRIDGE=ON','-DNVN_D2R_NAMES_HEADER='+str(names)]
 for index,(extra,text)in enumerate([(['-DNVN_D2R_SAMPLED_MIP_FILTER=ON'],'requires sampled images'),(['-DNVN_D2R_SAMPLED_IMAGES=ON'],'require graphics, runtime FDL and raw-block'),(['-DNVN_D2R_GATHER=ON'],'requires graphics and runtime FDL or native-view')]):
  r=run([*configure_base,'-B',out/('invalid-cmake-'+str(index)),*extra],expected=1);require(text in r.stderr);rejections.append('CMake '+text)
 results=[]
 modes=[('host',True,True,True,True,False),('activity',True,True,True,False,True),('activity',False,False,False,False,False),('host',False,False,True,False,False)]
 for index,(mode,sampled,mip,gather,bc_enabled,color_targets)in enumerate(modes):
  dest=out/('normal-'+str(index)+'-'+mode);target='dmhost_activity_kgsl'if mode=='activity'else'dmhost_native_kgsl'
  localflags,localmanifest,_=prepare(root,sampled,mip,gather,graphics,fdl,block,bridge if bc_enabled else None,library,library if bc_enabled else None,color_targets)
  args=['cmake','-S',own/'adapters/d2r'/mode,'-B',dest,'-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DNVN_D2R_NAMES_HEADER='+str(names),'-DNVN_D2R_GRAPHICS_PIPELINE_HEADER='+str(ph),'-DNVN_D2R_TEXTURE_BRIDGE_HEADER='+(str(bh)if bc_enabled else''),'-DNVN_D2R_BLOCK_COPY_HEADER='+(str(kh)if sampled or bc_enabled else''),'-DNVN_D2R_PIPELINE_TARGET_FDL_LIBRARY='+str(library),'-DNVN_D2R_BC_SAMPLED_FDL_LIBRARY='+(str(library)if bc_enabled else''),'-DNVN_D2R_COLOR_TARGETS='+('ON'if color_targets else'OFF'),*localflags]
  run(args);run(['cmake','--build',dest,'--target',target,'--parallel','4']);actual=run(['ninja','-C',dest,'-t','commands',target]).stdout
  audit(own,localmanifest,actual,bc if bc_enabled else None);bc_audit(own,bc if bc_enabled else None,actual,localmanifest)
  if any(s in actual.lower()for s in ('/gfx/metal/','/gfx/vulkan/','/hle_nvn.c','/graphics_boundary.c','/tests/nvn_pipeline_','/tests/bc_sampled_probe')):raise ValueError('forbidden source in actual host closure')
  compiled=[]
  for line in actual.splitlines():
   a=shlex.split(line)
   if '-c'in a:p=Path(a[a.index('-c')+1]).resolve();compiled.append(p);pin(p)
  hs=list(map(Path,(dest/'host-sources.txt').read_text().splitlines()));require(len(hs)==36 and len(set(hs))==36 and all(p.resolve()in compiled for p in hs))
  deps=run(['ninja','-C',dest,'-t','deps']).stdout
  for line in deps.splitlines():
   if line.startswith('    '):
    p=Path(line.strip());p=p if p.is_absolute()else dest/p
    if p.is_file():pin(p)
  elf=dest/('libdmhost_activity_kgsl.so'if mode=='activity'else'dmhost_native_kgsl');readelf=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf';info=run([readelf,'-h','-d','-s','--wide',elf]).stdout
  require('AArch64'in info and not any(x in info.lower()for x in ('libvulkan','libegl','libgles','libstdc++')))
  require(('nn_sampled_image_shadow_create'in info)==sampled);require(('nn_bc_shadow_destroy'in info)==(sampled or bc_enabled))
  (dest/'link-audit.txt').write_text(actual+'\n'+info);results.append(dict(mode=mode,sampled_images=sampled,mip_filter=mip,gather=gather,bc_enabled=bc_enabled,color_targets=color_targets,original_sources=36,compiled_sources=len(compiled),elf=str(elf),sha256=sha(elf),android_execution=False))
 for t in (ndk/'source.properties',ndk/'build/cmake/android.toolchain.cmake',ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/clang',ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/ld.lld',Path(shutil.which('cmake')),Path(shutil.which('ninja'))):pin(t)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('feature input changed '+p)
 result=dict(schema=1,kind='original-host-native-sampled-features-host-v1',passed=True,assertions=assertions,rejections=rejections,results=results,manifest=manifest,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()and p.suffix in('.so','.a','.o','.txt','.json')},android_execution=False,gpu_execution=False,original_module_execution=False)
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','ndk','fdl-check','graphics-check','bridge-check','block-check','names','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(check(a.root.resolve(),a.ndk.resolve(),a.fdl_check.resolve(),a.graphics_check.resolve(),a.bridge_check.resolve(),a.block_check.resolve(),a.names.resolve(),a.out.resolve()))

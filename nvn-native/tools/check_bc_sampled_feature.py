#!/usr/bin/env python3
"""HOST policy/verify-argv and real original-host NDK link. No module/GPU execution."""
import argparse,copy,importlib.util,json,os,shlex,shutil,subprocess,sys
from pathlib import Path
from workspace_paths import d2r_root
from bc_sampled_feature import prepare,audit,sha

def load(p,name):
 sys.path.insert(0,str(p.parent))
 try:
  spec=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
 finally:sys.path.pop(0)

def check(root,ndk,fdl,graphics,bridge,block,names,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 own=Path(__file__).resolve().parents[1];out.mkdir(parents=True)
 library,manifest,inputs=prepare(root,fdl,graphics,bridge,block)
 ph,pm=load(root/'nvn-native/tools/graphics_pipeline_cache.py','feature_pipeline').build(root,graphics)
 bh,bm=load(root/'nvn-native/tools/texture_bridge_bundle.py','feature_bridge').build(root,bridge)
 km=load(root/'nvn-native/tools/compile_block_copy.py','feature_kernel').verify(root,block);kh=Path(km['header'])
 inputs.update(pm['inputs']);inputs.update(bm['inputs']);inputs.update(bm['artifacts']);inputs.update(km['inputs']);inputs.update(km['artifacts']);inputs.update({str(bridge):sha(bridge),str(block):sha(block),str(names):sha(names)})
 def pin(path):
  path=Path(path).resolve();value=sha(path);key=str(path)
  if key in inputs and inputs[key]!=value:raise ValueError('input changed during production feature check '+key)
  inputs[key]=value
 for folder in (own,d2r_root()/'dm/host',d2r_root()/'dm/third_party/runtime-codecs',d2r_root()/'dm/third_party/libdeflate'):
  for path in folder.rglob('*'):
   if path.is_file()and'__pycache__'not in path.parts:pin(path)
 for path in (d2r_root()/'dm/gfx/hle_vi.c',d2r_root()/'dm/gfx/hle_nv.c'):pin(path)
 # HOST policy tests never replace GPU or original-module execution.
 assertions=0;rejections=[]
 def require(v):
  nonlocal assertions
  if not v:raise ValueError('feature HOST assertion failed')
  assertions+=1
 def reject(name,fn):
  try:fn()
  except (ValueError,OSError,KeyError):rejections.append(name);return
  raise ValueError('accepted bad feature '+name)
 require(prepare(root,None,None,None,None)==(None,None,{}))
 for mask in range(7):reject('missing feature inputs '+str(mask),lambda mask=mask:prepare(root,fdl,graphics if mask&1 else None,bridge if mask&2 else None,block if mask&4 else None))
 require(manifest['formats']==[65,66,67,68,69,70,71,72]and not manifest['qualifier_dependency']and not manifest['default_enabled']and library==Path(manifest['runtime_fdl_library']))
 fixture=out/'argv-inventory';fixture.write_bytes(b'HOST ONLY verify argv fixture\n');mapping={str(fixture):sha(fixture)}
 base=dict(host_abi=2,rewrite_abi='elfdesc',rules='native',private_gates='1',abi='arm64-v8a',execute=False,deploy=False,library_kind='shared',python=sys.executable,exefs='/original/exefs',ndk=str(ndk),jobs=4)
 mods=out/'modules';host=dict(options=base,modules=str(mods),artifacts=mapping,sources=mapping,tools=mapping)
 apk=load(own/'tools/build_d2r_activity_apk.py','bc_feature_apk');cli=load(own/'tools/run_d2r_host.py','bc_feature_cli')
 for enabled in (False,True):
  h=copy.deepcopy(host);o=h['options']
  if enabled:o.update(bc_sampled_fdl_check=str(fdl),graphics_pipeline_check=str(graphics),texture_bridge_check=str(bridge),block_copy_check=str(block))
  argv=apk.host_verify(h,root);require(('--bc-sampled-fdl-check'in argv)==enabled);require(('--block-copy-check'in argv)==enabled)
  if enabled:require(argv[argv.index('--bc-sampled-fdl-check')+1]==str(fdl))
  o.update(host_abi=1,rewrite_abi='elf');argv=cli.host_verify_argv(h,root);require(('--bc-sampled-fdl-check'in argv)==enabled);require(('--block-copy-check'in argv)==enabled)
  if enabled:require(argv[argv.index('--bc-sampled-fdl-check')+1]==str(fdl))
 # Reject missing prerequisites before any NSO hashing/rewrite/output creation.
 commands=[]
 for script in ('build_d2r_activity.py','build_d2r_host.py'):
  argv=[sys.executable,str(own/'tools'/script),'--exefs','/no-original-read','--ndk',str(ndk),'--out',str(out/'must-not-exist'),'--bc-sampled-fdl-check',str(fdl)]
  r=subprocess.run(argv,capture_output=True,text=True)
  require(r.returncode==2 and '--bc-sampled-fdl-check requires'in r.stderr and not(out/'must-not-exist').exists());rejections.append(script+' prerequisite gate')
 # Normal FDL verifier detects changed metadata/artifact content; original
 # record and archive are never modified by this negative fixture.
 broken=out/'fdl-bad.json';v=json.loads(fdl.read_text());key=next(iter(v['inputs']));v['inputs'][key]='0'*64;broken.write_text(json.dumps(v))
 reject('stale verified FDL source',lambda:prepare(root,broken,graphics,bridge,block))
 # Audit fixed tokens for actual source/macro/link requirements before using
 # the same checker on real Ninja commands below.
 n=own
 line=lambda name,extra='':'clang -DNN_PIPELINE_BC_SAMPLED=1 -DNN_NVN_BLOCK_COPY=1 '+extra+' -c '+str(n/'src'/name)+' -o test.o'
 synthetic='\n'.join(line(x)for x in ('nvn.c','pipeline.c','texture_bc_shadow.c'))+'\nclang x.o '+str(library)+' -o host'
 audit(n,manifest,synthetic);require(True)
 for name,text in [('no module',synthetic.replace(line('texture_bc_shadow.c'),'')),('no macro',synthetic.replace('-DNN_PIPELINE_BC_SAMPLED=1','')),('no kernel',synthetic.replace('-DNN_NVN_BLOCK_COPY=1','')),('wrong archive',synthetic.replace(str(library),'/unverified/archive.a')),('qualifier source',synthetic+'\nclang -c '+str(n/'tests/bc_sampled_probe.c')+' -o x.o')]:reject(name,lambda text=text:audit(n,manifest,text))
 audit(n,None,'clang -c '+str(n/'src/nvn.c')+' -o x.o');require(True)
 reject('unexpected BC default ON',lambda:audit(n,None,synthetic))
 def run(argv):
  r=subprocess.run(list(map(str,argv)),text=True,capture_output=True);commands.append(dict(argv=list(map(str,argv)),returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stderr or r.stdout)
  return r.stdout
 # The candidate tree is a complete immutable native source copy, with original
 # dm/ source symlinks supplied by the ignored preparation harness only. These
 # CMake targets compile all 36 ORIGINAL host/HLE/platform sources, no NSO.
 results=[]
 for mode,enabled in (('activity',True),('host',True),('activity',False)):
  dest=out/(mode+('-on'if enabled else'-off'));target='dmhost_activity_kgsl'if mode=='activity'else'dmhost_native_kgsl'
  argv=['cmake','-S',own/'adapters/d2r'/mode,'-B',dest,'-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DNVN_D2R_NAMES_HEADER='+str(names),'-DNVN_D2R_GRAPHICS_PIPELINE_HEADER='+str(ph),'-DNVN_D2R_TEXTURE_BRIDGE_HEADER='+str(bh),'-DNVN_D2R_BLOCK_COPY_HEADER='+str(kh),'-DNVN_D2R_BC_SAMPLED_FDL_LIBRARY='+(str(library)if enabled else'')]
  run(argv);run(['cmake','--build',dest,'--target',target,'-j','6'])
  actual=run(['ninja','-C',dest,'-t','commands',target]);audit(own,manifest if enabled else None,actual)
  if any(s in actual.lower()for s in ('/gfx/metal/','/gfx/vulkan/','/hle_nvn.c','/graphics_boundary.c','/tests/nvn_pipeline_bc','/tests/bc_sampled_probe')):raise ValueError('unexpected qualification/renderer source in production host')
  compile_sources=[]
  for line in actual.splitlines():
   parts=shlex.split(line)
   if '-c'in parts:
    source=Path(parts[parts.index('-c')+1]).resolve();compile_sources.append(source);pin(source)
  deps=run(['ninja','-C',dest,'-t','deps'])
  for dep in deps.splitlines():
   if dep.startswith('    '):
    path=Path(dep.strip());path=path if path.is_absolute()else dest/path
    if path.is_file():pin(path)
  host_sources=list(map(Path,(dest/'host-sources.txt').read_text().splitlines()))
  require(len(host_sources)==36 and len(set(host_sources))==36 and all(x.resolve()in compile_sources for x in host_sources))
  elf=dest/('libdmhost_activity_kgsl.so'if mode=='activity'else'dmhost_native_kgsl');readelf=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf'
  info=run([readelf,'-h','-d','-s','--wide',elf]);require('AArch64'in info and not any(x in info.lower()for x in ('libvulkan','libegl','libgles','libstdc++')))
  if enabled:require('nn_bc_shadow_create'in info and 'nn_fdl_image'in info)
  else:require('nn_bc_shadow_create'not in info)
  (dest/'link-audit.txt').write_text(actual+'\n'+info)
  results.append(dict(mode=mode,enabled=enabled,target=target,compiled_source_count=len(compile_sources),original_host_sources=36,elf=str(elf),sha256=sha(elf),android_execution=False))
 for p in own.rglob('*'):
  if p.is_file()and'__pycache__'not in p.parts:pin(p)
 for tool in (ndk/'source.properties',ndk/'build/cmake/android.toolchain.cmake',ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/clang',ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/ld.lld',Path(shutil.which('cmake')).resolve(),Path(shutil.which('ninja')).resolve()):inputs[str(tool)]=sha(tool)
 for p,h in inputs.items():
  if sha(Path(p))!=h:raise ValueError('feature input changed '+p)
 result=dict(schema=1,kind='production-bc-feature-host-v1',passed=True,assertions=assertions,rejections=rejections,results=results,feature=manifest,inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()and p.suffix in ('.so','.a','.o','.txt','.json')},android_execution=False,gpu_execution=False,original_module_execution=False,scope='HOST verification-option/default gates and normal actual original 36source host+independent KGSL NDK link. No NSO rewriting/loading or device/GPU work.')
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','ndk','fdl-check','graphics-check','bridge-check','block-check','names','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(check(a.root.resolve(),a.ndk.resolve(),a.fdl_check.resolve(),a.graphics_check.resolve(),a.bridge_check.resolve(),a.block_check.resolve(),a.names.resolve(),a.out.resolve()))

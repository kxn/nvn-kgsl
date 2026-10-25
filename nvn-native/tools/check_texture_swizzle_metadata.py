#!/usr/bin/env python3
"""Actual public texture swizzle/mode metadata and strict sampled consumer gates.
HOST real core/RAM, no GPU execution; actual Android KGSL strict link.
"""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--ndk',type=Path,required=True);p.add_argument('--fdl-check',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();root=a.root.resolve();n=root/'nvn-native';out=a.out.resolve()
 if out.exists():p.error('fresh output required')
 out.mkdir(parents=True);proof=dict(schema=1,passed=False,scope=__doc__,GPU_execution=False,Android_execution=False,commands=[],inputs={})
 for folder in ['src','include','third_party/nvn','third_party/kgsl','android','tests/d2r_bridge']:
  for f in(n/folder).rglob('*'):
   if f.is_file():proof['inputs'][str(f)]=sha(f)
 proof['inputs'][str(Path(__file__).resolve())]=sha(__file__)
 modules=['compute','block_copy','graphics','external_image','order','render_state','sampler_state','counter','texture_source','texture_bridge','offline_graphics','pipeline_program','pipeline_shader','pipeline_vertex','pipeline','pipeline_archive','pipeline_source','pipeline_frontend','pipeline_resource_queries'];sources=[n/'src'/f'{m}.c'for m in modules]
 flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-DNN_NVN_BLOCK_COPY=1','-DNN_NVN_GRAPHICS_SOURCE_CACHE=1',*['-I'+str(n/f)for f in ['include','third_party/nvn','third_party/kgsl','src','tests']]]
 defs=['-DNN_VIEW_CORE='+json.dumps(str(n/'src/nvn.c')),'-DNN_VIEW_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c'))]
 def run(label,argv):
  argv=list(map(str,argv));r=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));proof['commands'].append(dict(label=label,argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'progress.json').write_text(json.dumps(proof,indent=2)+'\n')
  if r.returncode:raise ValueError(label+': '+r.stderr[-5000:])
  return r.stdout
 try:
  # Validate the unmodified normal producer at its actual source location.
  # An ignored candidate may share that exact facade by byte identity; never
  # rewrite an existing record to pretend it originated at another root.
  path=a.fdl_check.resolve();fdl=json.loads(path.read_text());keys=[Path(k)for k in fdl['inputs']if k.endswith('/nvn-native/src/runtime_fdl.cc')];assert len(keys)==1;fdl_root=keys[0].parents[2];builder=fdl_root/'nvn-native/tools/build_runtime_fdl.py';spec=importlib.util.spec_from_file_location('nn_swizzle_normal_fdl',builder);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);archive,fdl=module.verify(fdl_root,path)
  for rel in ['src/runtime_fdl.cc','include/nvn_native/runtime_fdl.h','tests/runtime_fdl.cc','tools/build_runtime_fdl.py']:
   if sha(n/rel)!=sha(fdl_root/'nvn-native'/rel):raise ValueError('actual linked FDL facade differs from current source '+rel)
  proof['inputs'][str(path)]=sha(path);proof['inputs'].update(fdl['inputs']);proof['inputs'].update(fdl['artifacts']);proof['FDL_record_origin_root']=str(fdl_root);proof['FDL_current_source_byte_identity']=True
  proof['results']=[];proof['source_resource_regression']=[]
  for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
   binary=out/('texture-swizzle-'+cc);tool=Path(subprocess.check_output(['which',cc],text=True).strip()).resolve();proof['inputs'][str(tool)]=sha(tool);run(cc+' actual canonical view core compile',[cc,*flags,*defs,*san,n/'tests/d2r_bridge/texture_swizzle_metadata.c',*sources,'-lm','-o',binary]);result=json.loads(run(cc+' public swizzle metadata HOST execute',[binary]));assert result['passed']is True and result['swizzle_combinations']==1296 and result['subset_consumer_rejections']>0 and result['GPU_submissions']==0;proof['results'].append(result)
   binary=out/('source-resources-'+cc);old=['-DNN_RESOURCE_CORE='+json.dumps(str(n/'src/nvn.c')),'-DNN_RESOURCE_MEMORY_BACKEND='+json.dumps(str(n/'tests/d2r_bridge/memory_backend.c'))];run(cc+' existing source resource compile',[cc,*flags,*old,*san,n/'tests/d2r_bridge/source_resource.c',*sources,'-lm','-o',binary]);result=json.loads(run(cc+' existing source resource HOST execute',[binary]));assert result['passed']is True;proof['source_resource_regression'].append(result)
   binary=out/('subresource-regression-'+cc);run(cc+' existing subresource compile',[cc,*flags,*defs,*san,n/'tests/d2r_bridge/texture_subresource_registration.c',*sources,'-lm','-o',binary]);result=json.loads(run(cc+' existing subresource HOST execute',[binary]));assert result['passed']is True;proof.setdefault('subresource_regression',[]).append(result)
  cc=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';proof['inputs'][str(cc)]=sha(cc);proof['ndk']={}
  for bc in [0,1]:
   binary=out/f'libtexture-subresources-bc{bc}-api29.so';extra=[n/'src/texture_bc_shadow.c',archive]if bc else[];run('NDK actual KGSL backend strict link BC '+str(bc),[cc,*flags,f'-DNN_PIPELINE_BC_SAMPLED={bc}','-fPIC','-shared','-Wl,--no-undefined',n/'src/nvn.c',n/'src/kgsl.c',*sources,*extra,*[n/'android'/f'{m}.c'for m in ['window_session','window_android','present','present_pacer','window_display','ahb']],'-landroid','-ldl','-lm','-o',binary]);elf=run('NDK ELF dependencies BC '+str(bc),[cc.parent/'llvm-readelf','-h','-d',binary]);assert'AArch64'in elf and not any(x in elf.lower()for x in ['libvulkan','libegl','libgles']);proof['ndk'][str(bc)]=dict(path=str(binary),sha256=sha(binary))
  for f,h in proof['inputs'].items():
   if sha(f)!=h:raise ValueError('source changed '+f)
  proof['passed']=True
 except Exception as e:proof['error']=str(e)
 proof['artifacts']={str(f):sha(f)for f in out.iterdir()if f.is_file()and f.name not in ['progress.json','check.json']};(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],check=str(out/'check.json'),error=proof.get('error'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())

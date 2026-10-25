#!/usr/bin/env python3
"""Normal raw-native BC sampling ELF/inputs; never deploys or runs Android/GPU.

Explicitly selects the verified original 967/a767 pair. Qualification uses
authored BC endpoint/transparent blocks, nearest/clamp, gamma=contrast1/mix0.
No new Maxwell envelope, archive-first production fallback or CPU codec.
"""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,sys
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p,name):
 sys.path.insert(0,str(p.parent))
 try:s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
 finally:sys.path.pop(0)
def expected(format,x,y,repeat):
 bx,by,lx,ly=x//4,y//4,x%4,y%4;out=[0,0,0,255]
 if format<0x43:
  code=(0,1,3,1)[(lx+2*ly+bx+3*by+repeat)%4]
  if code==1:out[:3]=[255]*3
  if format==0x42 and code==3:out[3]=0
 else:
  out[(lx+2*ly+by+repeat)%2]=255
  out[3]=((lx+ly+bx+repeat)%4)*85 if format==0x43 else (0,255,0,255)[(lx+ly+bx+repeat)%4]
 return bytes([17,29,43,59]if out[3]==0 else out)
def build(root,fdl_path,kernel_path,pipeline_path,pair_index,ndk,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 dep=root/'nvn-native';own=Path(__file__).resolve().parents[1]
 library,fdl=load(dep/'tools/build_runtime_fdl.py','bc_fdl').verify(root,fdl_path)
 block=load(dep/'tools/compile_block_copy.py','bc_block').verify(root,kernel_path)
 p=json.loads(pipeline_path.read_text());pack=load(dep/'tools/package_graphics_pipelines.py','bc_pipeline')
 header=pack.build(root,Path(p['stage_check']),Path(p['compiler_record']).parent,p['gmem_bytes'],pipeline_path.parent,True)
 if p.get('chip')!=0x06050002 or p.get('gmem_bytes')!=1179648 or not 0<=pair_index<len(p['pairs']):raise ValueError('bad explicit pipeline selection')
 pair=p['pairs'][pair_index];stages=list(json.loads(Path(p['stage_check']).read_text())['stages'].values())
 selected=[stages[pair[n+'_index']]for n in ('vs','fs')]
 identities=[('c1906ac598c15e11a3029bff6db444f45b4343ec56ffb15624eff0be5f7d7369','96757bb36a024b91e6f161b424af3cc3db2f035f059842e5151ba7fb1ff89cc4'),('a98bd42200fa86b7c94c19ecb528449043c6fb71c235dafbf46ce20f7c8fdbda','a7672f1d3bfeb7d0312812c8e98eea92565d5934bad06bbb5f345e5a684f92e5')]
 for stage,(code,control)in zip(selected,identities):
  v=stage['envelope']['inputs']
  if v['code']['sha256']!=code or v['control']['sha256']!=control or v['c1']['size']!=0:raise ValueError('not the exact original stage underlying the closed oracle')
 metadata=pair['metadata']
 if any(metadata.get(k)!=v for k,v in dict(texture_mask=2,semantic_sampler_mask=2,vertex_inputs=3,has_kill=True,fragcoord_mask=0).items()):raise ValueError('changed original resource contract')
 inputs={str(fdl_path):sha(fdl_path),str(kernel_path):sha(kernel_path),str(pipeline_path):sha(pipeline_path),**fdl['inputs'],**fdl['artifacts'],**block['inputs'],**block['artifacts'],**p['inputs'],**p['outputs']}
 sources=[own/'src/texture_bc_shadow.c',own/'src/pipeline.c',*[dep/'src'/n for n in ('pipeline_program.c','pipeline_shader.c','pipeline_vertex.c','offline_graphics.c','render_state.c','sampler_state.c','block_copy.c','texture_source.c','compute.c','texture_bridge.c','order.c','kgsl.c')]]
 cc=ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';readelf=cc.parent/'llvm-readelf'
 fixture=own/'tests/bc_sampled_probe.c'
 for f in [Path(__file__).resolve(),Path(__file__).with_name('validate_bc_sampled.py'),fixture,own/'tests/bc_sampled_probe.h',own/'android/bc_sampled_activity.c',own/'tests/bc_sampled_cases.h',own/'tests/bc_sampled_pattern.c',*sources,cc.resolve(),readelf,*sorted((dep/'include/nvn_native').glob('*.h')),*sorted((own/'include/nvn_native').glob('*.h')),dep/'third_party/nvn/nvn.h',dep/'third_party/kgsl/msm_kgsl.h']:inputs[str(f)]=sha(f)
 out.mkdir(parents=True);qualified=out/'bc_sampled_qualification.h'
 qualified.write_text('/* Explicit authored qualification; not game runtime resource selection. */\n#include '+json.dumps(str(header))+'\n#include '+json.dumps(block['header'])+'\n#define NN_BC_BLOCK_IMAGE NVN_COMPILED_IMAGE\n#define NN_BC_SAMPLED_PAIR '+str(pair_index)+'u\n')
 flags=['-DNN_PIPELINE_BC_SAMPLED=1','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(dep/'third_party/nvn'),'-I'+str(own/'include'),'-I'+str(dep/'include'),'-I'+str(dep/'include/nvn_native'),'-I'+str(dep/'third_party/kgsl')]
 define='-DNN_BC_SAMPLED_QUALIFICATION='+json.dumps(str(qualified));elf=out/'bc-sampled-api29'
 argv=list(map(str,[cc,*flags,define,'-DNN_BC_SAMPLED_STANDALONE=1',fixture,*sources,library,'-lm','-Wl,--no-undefined','-o',elf]))
 result=subprocess.run(argv,capture_output=True,text=True);commands=[dict(argv=argv,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr)]
 (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
 if result.returncode:raise ValueError(result.stderr)
 info=subprocess.run([str(readelf),'-h','-d',str(elf)],check=True,capture_output=True,text=True).stdout
 if 'AArch64'not in info or any(x in info.lower()for x in ('libvulkan','libgles','libegl')):raise ValueError('wrong Android ELF boundary')
 (out/'elf.txt').write_text(info)
 cases=[]
 for f in range(0x41,0x45):
  for width,height in ((8,8),(37,21),(128,65)):
   index=len(cases)//2
   for repeat in (0,1):
    data=b''.join(expected(f,x,y,repeat)for y in range(height)for x in range(width))
    cases.append(dict(index=index,format=f,width=width,height=height,repeat=repeat,file=f'bc-{index:02}-{repeat}.rgba',bytes=len(data),expected_sha256=hashlib.sha256(data).hexdigest()))
 all_expected=b''.join(expected(c['format'],x,y,c['repeat'])for c in cases for y in range(c['height'])for x in range(c['width']))
 oracle_checks=[]
 for compiler,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
  compiler_path=Path(shutil.which(compiler)).resolve();inputs[str(compiler_path)]=sha(compiler_path);binary=out/('oracle-'+compiler)
  argv=list(map(str,[compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror','-fno-pie','-no-pie',*san,own/'tests/bc_sampled_pattern.c','-o',binary]))
  r=subprocess.run(argv,capture_output=True,text=True);commands.append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
  if r.returncode:raise ValueError(r.stderr)
  r=subprocess.run([str(binary)],capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  if r.returncode or r.stdout!=all_expected:raise ValueError('C/Python authored closed-form oracle mismatch')
  oracle_checks.append(dict(compiler=compiler,channel_bytes=len(r.stdout),sha256=hashlib.sha256(r.stdout).hexdigest(),gpu_execution=False))
 (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
 for f,h in inputs.items():
  if sha(f)!=h:raise ValueError('input changed during normal qualifier build '+f)
 record=dict(schema=1,kind='raw-bc-sampled-qualification-v1',passed=True,gpu_execution=False,android_execution=False,public_nvn=False,game_frame=False,
  runtime_fdl_check=str(fdl_path),runtime_fdl_library=str(library),kernel_check=str(kernel_path),pipeline_check=str(pipeline_path),qualification_header=str(qualified),
  original_stage_inputs=[s['envelope']['inputs']for s in selected],explicit_pair_index=pair_index,qualification_sampler='nearest/clamp-to-edge, normalized2D, selected mip0',
  submissions=24,finishes=24,same_job_replays=12,channel_checks=sum(c['bytes']for c in cases),cases=cases,elf=str(elf),inputs=inputs,commands=commands,
  limitations=['HOST/build only until root Android run','BC1 RGB/RGBA + BC2/BC3 UNORM only','qualification level1/layer1 2D; metadata/mip/array conversion has separate HOST coverage','no real game frame or arbitrary sampler guarantee','no CPU decompression/image conversion','historical qualifier control bytes exactly as verified archive, no new full caller-span claim'],
  oracle_checks=oracle_checks,artifacts={str(f):sha(f)for f in out.iterdir()if f.is_file()})
 (out/'check.json').write_text(json.dumps(record,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__)
 for n in ('root','runtime-fdl-check','kernel-check','pipeline-check','ndk','out'):parser.add_argument('--'+n,type=Path,required=True)
 parser.add_argument('--pair-index',type=int,required=True);a=parser.parse_args()
 print(build(a.root.resolve(),a.runtime_fdl_check.resolve(),a.kernel_check.resolve(),a.pipeline_check.resolve(),a.pair_index,a.ndk.resolve(),a.out.resolve()))

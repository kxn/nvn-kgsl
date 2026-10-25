#!/usr/bin/env python3
"""Normal raw canonical-depth full-chain qualifier inputs; no GPU execution."""
import argparse,hashlib,importlib.util,json,os,shlex,subprocess,sys
from pathlib import Path
from depth_shadow_probe_validation import source_metadata,expected_input,up
KIND='raw-native-canonical-depth-qualification-v1'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def hashes(d):
 for p,h in d.items():
  if sha(p)!=h:raise ValueError('changed normal source/artifact '+p)
def produce(root,compiler,shader_check,fdl_root,fdl_check,depth_check,a2d_check,runtime_check,shadow_check,kernel_check,out):
 n=Path(__file__).resolve().parents[1]
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 from package_graphics_pipelines import compiler_record
 inputs={str(Path(__file__).resolve()):sha(__file__)};_,_,compiler_manifest=compiler_record(compiler,n,inputs)
 def verified(path,kind):
  c=json.loads(path.read_text())
  if c.get('kind')!=kind or not c.get('passed')or c.get('gpu_execution')is not False:raise ValueError('wrong normal dependency '+str(path))
  hashes(c['inputs']);hashes(c['artifacts']);inputs.update(c['inputs']);inputs.update(c['artifacts']);inputs[str(path)]=sha(path);return c
 shader=verified(shader_check,'authored-native-depth-recipes-v1');depth=verified(depth_check,'native-depth-fdl-foundation-v1');a2d=verified(a2d_check,'native-depth-a2d-host-v1');runtime=verified(runtime_check,'native-depth-zero-color-host-v1');shadow=verified(shadow_check,'canonical-depth-shadow-host-v1')
 if shader['compiler_build']!=compiler_manifest or [r['mode']for r in shader['recipes']]!=['color','only']:raise ValueError('authored normal compiler/sidecar identity mismatch')
 for r in shader['recipes']:
  d=r['recipe']['depth_export']
  if d.get('recipe_version')!=2 or not d.get('writes_depth')or d.get('color_outputs')!=(1 if r['mode']=='color'else 0):raise ValueError('real authored depth output facts required')
 fdltool=module(fdl_root/'nvn-native/tools/build_runtime_fdl.py','shadow_normal_fdl');library,fdl=fdltool.verify(fdl_root,fdl_check)
 kernel=json.loads(kernel_check.read_text());origins=[Path(p).parents[2]for p in kernel['inputs']if p.endswith('/nvn-native/shaders/block_copy.comp')]
 if len(origins)!=1:raise ValueError('ambiguous normal block origin')
 k=module(origins[0]/'nvn-native/tools/compile_block_copy.py','shadow_normal_block').verify(origins[0],kernel_check)
 for c in(fdl,k):inputs.update(c['inputs']);inputs.update(c['artifacts'])
 for p in(fdl_check,kernel_check):inputs[str(p)]=sha(p)
 # Normal records may originate at an immutable different tree, but all actual
 # adapter/runtime sources consumed here must independently match byte-for-byte.
 rels=('src/depth_shadow.c','include/nvn_native/depth_shadow.h','src/depth_transfer.c','include/nvn_native/depth_transfer.h','src/depth_a2d.cc','src/depth_fdl.cc','src/depth_native.cc','src/runtime_fdl.cc','include/nvn_native/runtime_fdl.h','src/pipeline.c','src/pipeline_program.c','src/render_state.c','include/nvn_native/pipeline.h','tools/compile_block_copy.py','shaders/block_copy.comp')
 for rel in rels:
  matches=[h for c in(shadow,a2d,runtime,fdl,k)for p,h in c['inputs'].items()if p.endswith('/nvn-native/'+rel)]
  if matches and any(h!=sha(n/rel)for h in matches):raise ValueError('normal current source mismatch '+rel)
 for rel in('tests/depth_shadow_probe.c','tests/depth_shadow_cases.h','tests/depth_shadow_layout.cc','tests/pipeline_probe.c','tools/depth_shadow_probe_validation.py','tools/depth_shadow_probe_runner.py',*rels):inputs[str(n/rel)]=sha(n/rel)
 out.mkdir(parents=True);runs=[]
 def run(argv,cwd=None,binary=False):
  argv=list(map(str,argv));r=subprocess.run(argv,cwd=cwd,capture_output=True,text=not binary,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));runs.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=None if binary else r.stdout,stdout_bytes=len(r.stdout)if binary else None,stdout_sha256=hashlib.sha256(r.stdout).hexdigest()if binary else None,stderr=r.stderr.decode(errors='replace')if binary else r.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if r.returncode:raise ValueError(str(r.stderr)[-6000:])
  return r.stdout
 normal=json.loads((fdl_check.parent/'commands.json').read_text());cases=None;fixture=None
 for cc,cpp,san in [('gcc','g++',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']),('clang','clang++',['-fsanitize=function','-fsanitize-trap=function'])]:
  template=next(c for c in normal if c['argv'][0]==cpp);args=template['argv'];start=args.index('-I'+str(fdl_root/'nvn-native/include'));end=args.index('-Wl,--as-needed');flags=args[1:start];libs=args[end:-2];objects=[]
  for i,name in enumerate(('depth_shadow','block_copy','texture_source','pipeline_views')):
   src=n/'src'/(name+'.c');obj=out/(cc+'-'+str(i)+'.o');run([cc,'-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-fno-pie',*san,'-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),'-c',src,'-o',obj]);objects.append(obj);inputs[str(src)]=sha(src)
  exe=out/('layout-'+cc);run([cpp,*flags,'-O2','-ffunction-sections','-fdata-sections','-fno-pie','-no-pie',*san,'-I'+str(n/'include'),'-I'+str(n/'tests'),'-I'+str(n/'third_party/nvn'),n/'tests/depth_shadow_layout.cc',n/'src/runtime_fdl.cc',n/'src/depth_fdl.cc',n/'src/depth_a2d.cc',*objects,*libs,'-Wl,--gc-sections','-o',exe],template['cwd'])
  rows=json.loads(run([exe]));source_metadata(rows);raw=run([exe,'--inputs'],binary=True)
  independent=b''.join(expected_input(c,r)for c in rows for r in range(2))
  if raw!=independent:raise ValueError('independent GOB scalar/index fixture disagrees')
  if cases is not None and(cases!=rows or fixture!=raw):raise ValueError('sanitizer source/FDL/input differences')
  cases=rows;fixture=raw
 (out/'cases.json').write_text(json.dumps(cases,indent=2)+'\n');(out/'canonical-inputs.raw').write_bytes(fixture)
 obj=runtime_check.parent/'depth-native-api29.o'
 if runtime['artifacts'].get(str(obj))!=sha(obj):raise ValueError('native depth object not in normal record')
 libraries=[str(library),depth['static_library'],a2d['static_library'],str(obj)]
 header=out/'depth_shadow_qualification.h';header.write_text('#pragma once\n#include "nvn_native/compute.h"\n#include '+json.dumps(shader['header'])+'\n#include '+json.dumps(k['header'])+'\n#define NN_DEPTH_SHADOW_BLOCK_IMAGE NVN_COMPILED_IMAGE\n')
 readonly=up(k['program']['binary_bytes'],4096)+2*4096+2*8192+2*up(40964,4096)+up(40964,4096)+up(45060,4096)+3*4096+sum(up(s['code_bytes'],4096)for s in shader['recipes'][0]['recipe']['stages'])-24
 hashes(inputs);record=dict(schema=1,kind=KIND,producer_sha256=sha(__file__),passed=True,gpu_execution=False,android_execution=False,nvn_api=False,game_modules_executed=False,header=str(header),compiler_record=str(compiler/'build-record.json'),fdl_root=str(fdl_root),fdl_check=str(fdl_check),shader_check=str(shader_check),depth_check=str(depth_check),a2d_check=str(a2d_check),runtime_check=str(runtime_check),shadow_check=str(shadow_check),kernel_check=str(kernel_check),native_libraries=libraries,cases=cases,readonly_bytes_per_chain=readonly,formats=[50,51,52],handles=54,repeats=2,draws=108,submissions=648,finishes=108,files=180,source_levels=3,private_owners=9,canonical_plus_owner_refs=10,chain_order=['different_private_clear','GOB_to_LINEAR_CS','A2D_UPLOAD','actual_depth_Draw','A2D_READBACK','LINEAR_to_GOB_CS'],cpu_private_depth_access=False,inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});(out/'check.json').write_text(json.dumps(record,indent=2)+'\n');return out/'check.json'
def build(check,out,root=None):
 c=json.loads(check.read_text());n=Path(__file__).resolve().parents[1]
 if c.get('kind')!=KIND or c.get('producer_sha256')!=sha(__file__)or c.get('submissions')!=648 or c.get('gpu_execution')is not False:raise ValueError('unknown/stale normal depth shadow qualifier')
 hashes(c['inputs']);hashes(c['artifacts']);source_metadata(c['cases'])
 from package_graphics_pipelines import compiler_record
 compiler_record(Path(c['compiler_record']).parent,n,{})
 return Path(c['header']),c,{str(check):sha(check),**c['inputs'],**c['artifacts']}
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 keys=['root','compiler','shader-check','fdl-root','fdl-check','depth-check','a2d-check','runtime-check','shadow-check','kernel-check','out']
 for k in keys:p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(produce(*[getattr(a,k.replace('-','_')).resolve()for k in keys]))

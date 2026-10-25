#!/usr/bin/env python3
"""Normal original CASC public Gather+FragDepth inputs. No device execution."""
import argparse,json
from pathlib import Path
from prepare_casc_gather_depth import produce as original,sha,module
KIND='public-casc-gather-fragdepth-qualification-v1'
VS='9e3d4dd50a9252cef8426d1849df7d87e0abb15f25c9f83a80932711c1000621'
FS='8fc28f2324458b06b22bd80abe77205446a05756db6d502681270bd1cf409d0a'
OWN=('tools/casc_gather_depth_qualification.py','tools/prepare_casc_gather_depth.py','tools/casc_gather_depth_oracle.py','tools/casc_gather_depth_validation.py','tools/casc_gather_depth_runner.py','tools/check_casc_gather_depth_probe.py','tests/test_casc_gather_depth_validation.py','tests/test_casc_gather_depth_oracle.py','tests/casc_gather_depth_layout.c','tests/nvn_gather_depth_probe.c','tests/nvn_pipeline_probe.c','tests/d2r_bridge/casc_gather_depth_failure.c','android/nvn_pipeline_probe_activity.c')
def checked(path,inputs):
 c=json.loads(path.read_text());inputs[str(path)]=sha(path)
 for group in('inputs','artifacts','outputs'):
  for p,h in c.get(group,{}).items():
   if sha(p)!=h:raise ValueError('normal input/product changed '+p)
   inputs[p]=h
 return c

def produce(root,pipeline,fdl_check,depth_check,a2d_check,runtime_check,block_check,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh worktree/.local output required')
 own=Path(__file__).resolve().parents[1];normal=root/'nvn-native';inputs={}
 records={k:checked(p,inputs)for k,p in [('fdl',fdl_check),('depth',depth_check),('a2d',a2d_check),('runtime',runtime_check),('block',block_check)]}
 package=json.loads(pipeline.read_text())
 if package.get('publish_depth_recipes')is not True or package.get('defer_depth_recipes')is True:raise ValueError('explicitly published exact depth archive required')
 fdl_lib,fdl=module(normal/'tools/build_runtime_fdl.py','casc_real_fdl').verify(root,fdl_check)
 block=module(normal/'tools/compile_block_copy.py','casc_real_block').verify(root,block_check)
 for key,kind in [('depth','native-depth-fdl-foundation-v1'),('a2d','native-depth-a2d-host-v1'),('runtime','native-depth-zero-color-host-v1')]:
  c=records[key]
  if c.get('kind')!=kind or c.get('passed')is not True or c.get('gpu_execution')is not False:raise ValueError('normal depth helper '+key)
  for rel in ('src/depth_fdl.cc','src/depth_a2d.cc','src/depth_native.cc','src/depth_transfer.c','src/pipeline.c','src/pipeline_program.c'):
   matches=[h for p,h in c['inputs'].items()if p.endswith('/nvn-native/'+rel)]
   if matches and any(h!=sha(own/rel)for h in matches):raise ValueError('actual helper/runtime source differs '+rel)
 libraries=[records['depth']['static_library'],records['a2d']['static_library'],str(runtime_check.parent/'depth-native-api29.o')]
 for lib in libraries:
  if not any(c.get('artifacts',{}).get(lib)==sha(lib)for c in records.values()):raise ValueError('unrecorded normal depth link product '+lib)
 out.mkdir(parents=True);sourcecheck=original(root,pipeline,VS,FS,out/'original');source=json.loads(sourcecheck.read_text());inputs.update(source['inputs']);inputs.update(source['artifacts']);inputs[str(sourcecheck)]=sha(sourcecheck)
 pair=source['selected_pair'];pairs=package['pairs'];idx=pairs.index(pair);header=out/'casc_gather_depth_qualification.h'
 header.write_text('/* Exact original CASC qualifier source; no runtime fallback. */\n#include '+json.dumps(source['pipeline_header'])+'\n#include '+json.dumps(block['header'])+'\n#define NN_CASC_GATHER_DEPTH_BLOCK_IMAGE NVN_COMPILED_IMAGE\n#define NN_CASC_GATHER_DEPTH_PAIR '+str(idx)+'u\n#define NN_CASC_GATHER_DEPTH_VS '+str(pair['vs_index'])+'u\n#define NN_CASC_GATHER_DEPTH_FS '+str(pair['fs_index'])+'u\n#include '+json.dumps(source['case_header'])+'\n')
 for rel in OWN:inputs[str(own/rel)]=sha(own/rel)
 c=dict(schema=1,kind=KIND,passed=True,gpu_execution=False,android_execution=False,producer_sha256=sha(__file__),normal_root=str(root),header=str(header),pipeline_check=str(pipeline),runtime_fdl_check=str(fdl_check),depth_check=str(depth_check),a2d_check=str(a2d_check),runtime_check=str(runtime_check),block_copy_check=str(block_check),runtime_fdl_library=str(fdl_lib),native_libraries=libraries,source_check=str(sourcecheck),original_identities=source['identities'],pair_index=idx,vs_index=pair['vs_index'],fs_index=pair['fs_index'],canonical=source['canonical'],cases=source['cases'],submissions=72,finishes=72,files=72,original_C1_bytes=256,public_depth_mode=0,texture_selector=10,sampler_selector=3,source_uniform_index=3,source_format=10,original_control_bytes=2176,full_0x898_control_qualified=False,clear_depth_endpoints_only=True,D24_active_X8_defined=False,scope='Explicit authored public NVN original CASC Gather+FragDepth source fixture; not captured D2R binding state, not game/frame proof',inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()})
 if any(sha(p)!=h for p,h in inputs.items()):raise ValueError('normal source changed during preparation')
 (out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'
def build(path,out,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();c=json.loads(path.read_text());locked={};checked(path,locked)
 if c.get('kind')!=KIND or c.get('producer_sha256')!=sha(__file__)or c.get('submissions')!=72 or c.get('gpu_execution')is not False:raise ValueError('unknown/stale CASC qualification')
 normal_root=Path(c['normal_root']);package=json.loads(Path(c['pipeline_check']).read_text());m=module(normal_root/'nvn-native/tools/package_graphics_pipelines.py','casc_verified_package');m.build(normal_root,Path(package['stage_check']),Path(package['compiler_record']).parent,package['gmem_bytes'],Path(c['pipeline_check']).parent,True,publish_depth_recipes=True)
 # External checked compiler records retain their declared original root.
 # A test snapshot may consume them only with identical compiler-facing bytes.
 current=Path(__file__).resolve().parents[1]
 for path,digest in package['inputs'].items():
  p=Path(path)
  if p.is_relative_to(normal_root/'nvn-native'):
   counterpart=current/p.relative_to(normal_root/'nvn-native')
   if not counterpart.is_file()or sha(counterpart)!=digest:raise ValueError('snapshot compiler-facing source differs '+str(counterpart))
 for rel in OWN:
  p=Path(__file__).resolve().parents[1]/rel
  if c['inputs'].get(str(p))!=sha(p):raise ValueError('qualifier must use exact checked source '+rel)
 return Path(c['header']),c,locked
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in('root','pipeline-check','fdl-check','depth-check','a2d-check','runtime-check','block-check','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(produce(*[getattr(a,k.replace('-','_')).resolve()for k in('root','pipeline-check','fdl-check','depth-check','a2d-check','runtime-check','block-check','out')]))

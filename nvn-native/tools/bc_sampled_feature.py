"""Verified production BC consumer link policy; no qualifier/shader selection."""
import hashlib,re,shlex
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def prepare(root,check,graphics_check,bridge_check,block_check):
 if check is None:return None,None,{}
 if not all((graphics_check,bridge_check,block_check)):raise ValueError('BC sampling requires explicit graphics-pipeline, texture-bridge and block-copy checks')
 from build_runtime_fdl import verify as verify_runtime_fdl
 path=Path(check).resolve();library,fdl=verify_runtime_fdl(root,path)
 inputs={str(path):sha(path),**fdl['inputs'],**fdl['artifacts']}
 native=root/'nvn-native'
 for p in (Path(__file__).resolve(),native/'src/texture_bc_shadow.c',native/'include/nvn_native/texture_bc_shadow.h',native/'include/nvn_native/runtime_fdl.h'):
  inputs[str(p)]=sha(p)
 manifest=dict(schema=1,kind='native-bc-sampled-production-feature-v1',runtime_fdl_check=str(path),runtime_fdl_record_sha256=sha(path),runtime_fdl_library=str(library),runtime_fdl_library_sha256=sha(library),inputs=inputs,consumer='actual NVN descriptor/handle/sampler snapshots and source query/recipe mapping',formats=[0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48],domain='ordinary2D single selected mip/layer UNORM/sRGB; actual runtime admission remains authoritative',canonical_source_preserved=True,cpu_pixel_conversion=False,qualifier_dependency=False,game_logic=False,default_enabled=False,activation='explicit --bc-sampled-fdl-check; no fallback or pair/index selection')
 return library,manifest,inputs

def audit(native,manifest,commands,shared_ledger=None):
 source=(native/'src/texture_bc_shadow.c').resolve();compiled=[]
 for line in commands.splitlines():
  parts=shlex.split(line)
  if '-c'in parts:compiled.append((Path(parts[parts.index('-c')+1]).resolve(),parts))
 selected=[parts for p,parts in compiled if p==source]
 if manifest is None:
  shared=bool(shared_ledger and shared_ledger.get('kind')=='original-host-native-sampled-features-v1' and shared_ledger.get('sampled_images')is True)
  if len(selected)!=(1 if shared else 0)or any(any(v=='-DNN_PIPELINE_BC_SAMPLED'or v.startswith('-DNN_PIPELINE_BC_SAMPLED=')for v in parts)for _,parts in compiled):raise ValueError('BC sampling unexpectedly enabled without explicit verified feature')
  if shared and '-DNN_PIPELINE_SAMPLED_IMAGES=1'not in selected[0]:raise ValueError('shared owner ledger lacks the sampled consumer macro')
  return
 if len(selected)!=1:raise ValueError('BC sampling source missing or duplicated in actual compilation')
 for name in ('nvn.c','pipeline.c','texture_bc_shadow.c'):
  rows=[parts for p,parts in compiled if p==native/'src'/name]
  if len(rows)!=1 or '-DNN_PIPELINE_BC_SAMPLED=1'not in rows[0]:raise ValueError('BC consumer macro missing from actual '+name+' command')
 core=next(parts for p,parts in compiled if p==native/'src/nvn.c')
 if '-DNN_NVN_BLOCK_COPY=1'not in core:raise ValueError('BC source conversion kernel path not compiled')
 library=manifest['runtime_fdl_library']
 if not any(library in shlex.split(line)for line in commands.splitlines()if ' -c 'not in line):raise ValueError('verified runtime FDL archive missing from actual production link')
 if any('bc_sampled_probe' in str(p)or'nvn_pipeline_bc_probe'in str(p)for p,_ in compiled):raise ValueError('qualification code present in production host')
 for p,h in manifest['inputs'].items():
  if sha(Path(p))!=h:raise ValueError('BC feature dependency changed '+p)

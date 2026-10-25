"""Verified actual-FDL dynamic RGBA8 target metadata policy, no shader oracle."""
import hashlib,shlex
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def prepare(root,check,graphics_check,bc_library=None):
 if check is None:return None,None,{}
 if not graphics_check:raise ValueError('runtime target views require --graphics-pipeline-check')
 from build_runtime_fdl import verify
 check=Path(check).resolve();library,fdl=verify(root,check)
 if bc_library and Path(bc_library).resolve()!=library.resolve():raise ValueError('target and BC features must use the same verified FDL archive')
 inputs={str(check):sha(check),**fdl['inputs'],**fdl['artifacts']}
 for name in ('tools/pipeline_views_feature.py','src/pipeline_views.c','include/nvn_native/pipeline_views.h','include/nvn_native/runtime_fdl.h'):
  p=root/'nvn-native'/name;inputs[str(p)]=sha(p)
 value=dict(schema=1,kind='runtime-fdl-rgba8-pipeline-views-v1',runtime_fdl_check=str(check),runtime_fdl_library=str(library),runtime_fdl_library_sha256=sha(library),inputs=inputs,domain='ordinary RGBA8 2D single mip/layer/sample native LINEAR nonUBWC; target and actual RGBA8 sampled view metadata',cache='Device-owned immutable stable nodes, no eviction until safe DeviceFinalize',static_size_catalogue=False,public_extent_changed=False,cpu_pixel_conversion=False,runtime_shader_compiler=False,default_enabled=False,activation='explicit --pipeline-target-fdl-check')
 return library,value,inputs
def audit(native,manifest,commands):
 compiled=[]
 for line in commands.splitlines():
  parts=shlex.split(line)
  if '-c'in parts:compiled.append((Path(parts[parts.index('-c')+1]).resolve(),parts))
 module=(native/'src/pipeline_views.c').resolve();selected=[p for p,flags in compiled if p==module]
 if manifest is None:
  if selected or any('-DNN_PIPELINE_RUNTIME_VIEWS=1'in flags for p,flags in compiled):raise ValueError('runtime target views enabled without verified option')
  return
 if len(selected)!=1:raise ValueError('runtime target views module missing/duplicated')
 for name in ('nvn.c','pipeline_views.c'):
  rows=[flags for p,flags in compiled if p==(native/'src'/name).resolve()]
  if len(rows)!=1 or '-DNN_PIPELINE_RUNTIME_VIEWS=1'not in rows[0]:raise ValueError('missing actual runtime view compile mode '+name)
 if not any(manifest['runtime_fdl_library']in shlex.split(line)for line in commands.splitlines()if ' -c 'not in line):raise ValueError('verified FDL archive absent from actual link')
 for p,h in manifest['inputs'].items():
  if sha(p)!=h:raise ValueError('runtime target metadata source changed '+p)

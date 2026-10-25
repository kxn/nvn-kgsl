"""Consume explicit normally verified shader-only pipeline archive; default OFF.
No game address, fixture/math oracle, runtime compiler or graphics-image template.
This provider catalogues shaders only. Actual NVN Draw additionally requires
the generic frontend's runtime specialization, resources and native views;
draw_supported=False here describes this provider's standalone capability.
"""
import json
from pathlib import Path
from package_graphics_pipelines import build as verify_package,checked,sha

def build(root,check):
 root=Path(root).resolve();check=Path(check).resolve()
 if not check.is_relative_to(root/'.local'):raise ValueError('generated graphics catalogue must stay ignored')
 record=json.loads(check.read_text())
 if(record.get('schema')!=1 or record.get('kind')!='native-graphics-pipeline-recipes-v1' or record.get('gpu_execution')is not False or record.get('nvn_runtime_registered')is not False):raise ValueError('unknown shader-only pipeline catalogue')
 header=verify_package(root,Path(record['stage_check']),Path(record['compiler_record']).parent,record['gmem_bytes'],check.parent,True,defer_depth_recipes=record.get('defer_depth_recipes',False),publish_depth_recipes=record.get('publish_depth_recipes',False),publish_storage_recipes=record.get('publish_storage_recipes',False))
 if record['outputs'].get(str(header)) != sha(header):raise ValueError('catalogue header outside recorded output location')
 inputs={str(check):sha(check),str(Path(__file__).resolve()):sha(__file__)}
 checked(record['inputs'],inputs);checked(record['outputs'],inputs)
 manifest={'schema':1,'kind':'native-graphics-source-only-provider-v1','inputs':inputs,'header':str(header),
  'stage_count':len(record['requirements']),'pair_count':len(record['pairs']),'chip':record['chip'],
  'gmem_bytes':record['gmem_bytes'],'runtime_compiler':False,'source_only':True,'draw_supported':False,'depth_archive':bool(record.get('publish_depth_recipes',False)),'storage_archive':record.get('publish_storage_recipes',False),'native_texture_types':all('native_texture_types'in pair for pair in record['pairs'])}
 return header,manifest

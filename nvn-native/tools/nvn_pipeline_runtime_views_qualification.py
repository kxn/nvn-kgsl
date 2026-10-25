#!/usr/bin/env python3
"""Normal public-NVN target-cache qualification; views are allocation metadata only."""
import argparse,hashlib,json,sys
from pathlib import Path

SHAPES=((73,41),(257,113),(1153,641))
KIND='public-nvn-runtime-target-qualification-v1'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def checked(root,pipeline,views,fdl):
 sys.path.insert(0,str(root/'nvn-native/tools'))
 from pipeline_qualification import checked as color
 from build_runtime_fdl import verify
 p,ph,vh,index,pair,_,inputs=color(root,pipeline,views)
 lib,record=verify(root,fdl);inputs.update(record['inputs']);inputs.update(record['artifacts']);inputs[str(fdl)]=sha(fdl)
 values=json.loads(views.read_text())['views'];selected=[]
 for w,h in SHAPES:
  found=[(i,v)for i,v in enumerate(values)if (v['width'],v['height'])==(w,h)and v['format']==37 and v['target']==1 and v['levels']==v['layers']==v['samples']==1 and not v['tile_mode']and not v['ubwc']]
  if len(found)!=1:raise ValueError('one actual noncatalogued target metadata view required '+str((w,h)))
  selected.append(found[0])
 return ph,vh,index,pair,selected,lib,inputs

def produce(root,pipeline,views,fdl,layout,out):
 if out.exists()or not out.is_relative_to(root/'.local')or layout not in ('linear','gob'):raise ValueError('fresh ignored output and known source layout required')
 ph,vh,index,pair,selected,lib,inputs=checked(root,pipeline,views,fdl)
 from texture_bridge_bundle import build as bridge
 from nvn_pipeline_gob_validation import source_shape
 bh,br=bridge(root,views);inputs.update(br['inputs']);inputs.update(br['artifacts'])
 inputs[str(Path(__file__).resolve())]=sha(__file__)
 for rel in ('nvn_pipeline_runtime_views_validation.py','nvn_pipeline_gob_validation.py','pipeline_validation.py','texture_conversion_validation.py'):
  f=root/'nvn-native/tools'/rel;inputs[str(f)]=sha(f)
 out.mkdir(parents=True);h=out/'nvn_runtime_target_qualification.h'
 h.write_text('/* Allocation/oracle metadata only: NEVER register these views. */\n#include "'+str(ph)+'"\n#include "'+str(vh)+'"\n#include "'+str(bh)+'"\n#define NN_PIPELINE_QUAL_PAIR_INDEX '+str(index)+'u\nstatic const struct nn_tex_native_view *const NN_PIPELINE_QUAL_VIEWS[]={'+','.join('&NN_TEXTURE_NATIVE_VIEWS['+str(i)+']'for i,v in selected)+'};\n')
 cases=[]
 for shape,(i,v)in enumerate(selected):
  w,hgt=SHAPES[shape];payload=source_shape(w,hgt)[0]if layout=='gob'else v['bytes']
  for attribute in range(3):
   for mode in range(4):cases.append(dict(case=len(cases),width=w,height=hgt,layout=attribute,mode=mode,payload_bytes=payload,pool_bytes=8192+((payload+4095)//4096)*4096,pitch=v['pitch']))
 result=dict(schema=1,kind=KIND,producer_sha256=sha(__file__),source_layout=layout,pipeline_check=str(pipeline),native_views_check=str(views),runtime_fdl_check=str(fdl),runtime_fdl_library=str(lib),header=str(h),selected_pair=index,pair=pair,views=[v for i,v in selected],cases=cases,handles=36,submissions=72,channel_checks=24*4*sum(w*h for w,h in SHAPES),runtime_target_views=True,static_view_registrations=0,gpu_execution=False,scope='Original source Program/public NVN Draw to noncatalogued dimensions; runtime actual FDL cache, no static metadata registration, CPU image conversion or game frame claim.',inputs=inputs,artifacts={str(h):sha(h)})
 if any(sha(p)!=v for p,v in inputs.items()):raise ValueError('qualification input mutated')
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'

def build(check,out,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();c=json.loads(check.read_text())
 if c.get('schema')!=1 or c.get('kind')!=KIND or c.get('producer_sha256')!=sha(__file__)or c.get('source_layout')not in ('linear','gob')or c.get('runtime_target_views')is not True or c.get('static_view_registrations')!=0:raise ValueError('unknown runtime target qualifier')
 for p,h in {**c['inputs'],**c['artifacts']}.items():
  if sha(p)!=h:raise ValueError('runtime target dependency changed '+p)
 *_,lib,inputs=checked(root,Path(c['pipeline_check']),Path(c['native_views_check']),Path(c['runtime_fdl_check']))
 if str(lib)!=c['runtime_fdl_library']:raise ValueError('different actual FDL library')
 return Path(c['header']),c,{**c['inputs'],**c['artifacts'],str(check):sha(check)}

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for name in ('root','pipeline-check','native-views','runtime-fdl-check','out'):p.add_argument('--'+name,type=Path,required=True)
 p.add_argument('--source-layout',choices=('linear','gob'),required=True);a=p.parse_args()
 print(produce(a.root.resolve(),a.pipeline_check.resolve(),a.native_views.resolve(),a.runtime_fdl_check.resolve(),a.source_layout,a.out.resolve()))

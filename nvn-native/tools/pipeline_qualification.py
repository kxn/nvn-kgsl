#!/usr/bin/env python3
"""Lock a normal original color recipe and actual FDL views for raw-native qualification.
No fixed graphics image, runtime compiler, guessed shader resources or GPU run.
"""
import argparse,hashlib,importlib.util,json,sys
from pathlib import Path
SHAPES=((64,64),(127,73),(2880,1620))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(path,name):
 sys.path.insert(0,str(path.parent))
 try:
  s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
 finally:sys.path.pop(0)

def checked(root,pipeline,views):
 p=json.loads(pipeline.read_text());v=json.loads(views.read_text())
 if p.get('kind')!='native-graphics-pipeline-recipes-v1' or p.get('chip')!=0x06050002 or p.get('gmem_bytes')!=1179648 or p.get('draw_template') is not False:raise ValueError('normal A650 dynamic recipe required')
 producer=load(root/'nvn-native/tools/package_graphics_pipelines.py','nn_pipeline_qual_actual_package')
 h=producer.build(root,Path(p['stage_check']),Path(p['compiler_record']).parent,p['gmem_bytes'],pipeline.parent,verify=True)
 native=load(root/'nvn-native/tools/texture_bridge_bundle.py','nn_pipeline_qual_actual_fdl');native.build(root,views)
 # Explicit authored-test selection, never archive-first runtime fallback. Full
 # stage/code/control/specialization identities are locked by the verified
 # normal package, while the C caller also validates every actual UBO patch.
 candidates=[(i,x) for i,x in enumerate(p['pairs']) if x['vs_index']==2 and x['fs_index']==3]
 if len(candidates)!=1:raise ValueError('explicit original color pair absent or ambiguous')
 index,pair=candidates[0];m=pair['metadata']
 if (m['vertex_inputs']!=2 or m['texture_mask'] or m['semantic_sampler_mask'] or m['fragcoord_mask'] or m['has_kill'] or
     [s['code_bytes'] for s in m['stages']]!=[384,128]):raise ValueError('unsupported original color resource contract')
 header=views.parent/'texture_native_views.h'
 if str(header) not in v['artifacts']:raise ValueError('actual FDL view header not recorded')
 selected=[]
 for shape in SHAPES:
  match=[(i,x) for i,x in enumerate(v['views']) if (x['width'],x['height'])==shape and x['format']==37 and x['target']==1 and x['levels']==x['layers']==x['samples']==1 and not x['tile_mode'] and not x['ubwc']]
  if len(match)!=1:raise ValueError('one explicit actual FDL view required: '+str(shape))
  selected.append(match[0])
 inputs={**p['inputs'],**p['outputs'],**v['inputs'],**v['artifacts'],str(pipeline):sha(pipeline),str(views):sha(views),str(Path(__file__).resolve()):sha(__file__)}
 return p,h,header,index,pair,selected,inputs

def produce(root,pipeline,views,out):
 if out.exists() or not out.is_relative_to(root/'.local'):raise ValueError('fresh output below worktree/.local required')
 p,ph,vh,index,pair,selected,inputs=checked(root,pipeline,views);out.mkdir(parents=True)
 h=out/'pipeline_qualification.h'
 h.write_text('/* Normal immutable original shader recipe + actual FDL views, authored qualifier only. */\n#include "'+str(ph)+'"\n#include "'+str(vh)+'"\n#define NN_PIPELINE_QUAL_PAIR_INDEX '+str(index)+'u\nstatic const struct nn_tex_native_view *const NN_PIPELINE_QUAL_VIEWS[]={'+','.join('&NN_TEXTURE_NATIVE_VIEWS['+str(i)+']' for i,x in selected)+'};\n')
 if any(sha(k)!=v for k,v in inputs.items()):raise ValueError('input changed during qualification packaging')
 record=dict(schema=1,kind='raw-native-dynamic-pipeline-qualification-v1',producer_sha256=sha(__file__),pipeline_check=str(pipeline),native_views_check=str(views),header=str(h),inputs=inputs,artifacts={str(h):sha(h)},selected_pair=index,pair=pair,views=[x for i,x in selected],layouts=3,draw_modes=4,repeats=2,jobs=36,submissions=72,native_vs_ubo_binding=1,original_maxwell_cb=3,ubo_bytes=64,channel_checks=24*4*sum(w*h for w,h in SHAPES),scope='Authored raw native dynamic target/VFD/direct/index/indirect/replay with verified original color VS/FS. No NVN API mapping, fixed graphics image, original game frame or mixed conversion consumer qualification.')
 (out/'check.json').write_text(json.dumps(record,indent=2)+'\n');return out/'check.json'

def build(check,out,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();c=json.loads(check.read_text())
 if c.get('schema')!=1 or c.get('kind')!='raw-native-dynamic-pipeline-qualification-v1' or c.get('producer_sha256')!=sha(__file__):raise ValueError('unknown/stale dynamic qualifier producer')
 for p,h in {**c['inputs'],**c['artifacts']}.items():
  if sha(p)!=h:raise ValueError('dynamic pipeline dependency changed: '+p)
 checked(root,Path(c['pipeline_check']),Path(c['native_views_check']))
 return Path(c['header']),c,{**c['inputs'],**c['artifacts'],str(check):sha(check)}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--pipeline-check',type=Path,required=True);p.add_argument('--native-views',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();print(produce(a.root.resolve(),a.pipeline_check.resolve(),a.native_views.resolve(),a.out.resolve()))
if __name__=='__main__':main()

#!/usr/bin/env python3
"""Explicit authored public-NVN color qualifier; generic runtime never selects this pair by index."""
import argparse,hashlib,importlib.util,json,sys
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(path,name):
 sys.path.insert(0,str(path.parent))
 try:
  s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
 finally:sys.path.pop(0)
def produce(root,pipeline,views,out):
 if out.exists() or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 raw=load(root/'nvn-native/tools/pipeline_qualification.py','nvn_actual_raw_contract')
 p,ph,vh,index,pair,selected,inputs=raw.checked(root,pipeline,views)
 bridge=load(root/'nvn-native/tools/texture_bridge_bundle.py','nvn_actual_source_bridge');bh,br=bridge.build(root,views)
 inputs.update(br['inputs']);inputs.update(br['artifacts']);inputs[str(Path(__file__).resolve())]=sha(__file__)
 out.mkdir(parents=True);header=out/'nvn_pipeline_qualification.h'
 header.write_text('/* Authored source selection only. ProgramSetShaders still performs exact full source/control matching. */\n#include "'+str(ph)+'"\n#include "'+str(vh)+'"\n#include "'+str(bh)+'"\n#define NN_PIPELINE_QUAL_PAIR_INDEX '+str(index)+'u\nstatic const struct nn_tex_native_view *const NN_PIPELINE_QUAL_VIEWS[]={'+','.join('&NN_TEXTURE_NATIVE_VIEWS['+str(i)+']' for i,x in selected)+'};\n')
 if any(sha(k)!=v for k,v in inputs.items()):raise ValueError('changed qualifier source/artifact')
 record=dict(schema=1,kind='public-nvn-dynamic-pipeline-qualification-v1',source_layout='linear',producer_sha256=sha(__file__),pipeline_check=str(pipeline),native_views_check=str(views),inputs=inputs,artifacts={str(header):sha(header)},header=str(header),selected_pair=index,pair=pair,views=[x for i,x in selected],layouts=3,draw_modes=4,repeats=2,handles=12,submissions=24,channel_checks=393216,source_control_match='complete original code/C1/control; no specialization chosen by caller',scope='Authored public NVN source Program, API UBO0/sourceCB3, runtime specialization and actual native view; LINEAR64 first qualification. No game Draw/frame claim.')
 (out/'check.json').write_text(json.dumps(record,indent=2)+'\n');return out/'check.json'
def build(check,out,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();c=json.loads(check.read_text())
 if c.get('schema')!=1 or c.get('kind')!='public-nvn-dynamic-pipeline-qualification-v1' or c.get('source_layout')!='linear' or c.get('producer_sha256')!=sha(__file__):raise ValueError('unknown/stale public NVN qualifier')
 for p,h in {**c['inputs'],**c['artifacts']}.items():
  if sha(p)!=h:raise ValueError('changed public NVN dependency: '+p)
 raw=load(root/'nvn-native/tools/pipeline_qualification.py','nvn_recheck_raw_contract');raw.checked(root,Path(c['pipeline_check']),Path(c['native_views_check']))
 return Path(c['header']),c,{**c['inputs'],**c['artifacts'],str(check):sha(check)}
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','pipeline-check','native-views','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(produce(a.root.resolve(),a.pipeline_check.resolve(),a.native_views.resolve(),a.out.resolve()))
if __name__=='__main__':main()

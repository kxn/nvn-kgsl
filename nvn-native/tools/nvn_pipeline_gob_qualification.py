#!/usr/bin/env python3
"""Separate public-NVN GOB/flags9 color qualification, preserving the LINEAR check."""
import argparse,hashlib,json,sys
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def produce(root,pipeline,views,out):
 sys.path.insert(0,str(root/'nvn-native/tools'))
 from nvn_pipeline_qualification import produce as base_produce
 base_produce(root,pipeline,views,out)
 c=json.loads((out/'check.json').read_text());c.update(kind='public-nvn-gob-pipeline-qualification-v1',source_layout='gob',source_flags=9,target_pool_flags=164,source_kind=254,source_policy='immutable-native-no-NVIDIA-compression-Generic16Bx2-v1',producer_sha256=sha(__file__),handles=36,submissions=72,channel_checks=449180832,device_channel_checks=0,device_guard_byte_checks=589824,readonly_byte_checks=1474560,scope='Public NVN original color Draw to actual canonical GOB source target, flags9 DISPLAY|COMPRESSIBLE under explicit uncompressed Generic-kind native policy. Offline independent oracle required; no game frame claim.')
 c['inputs'][str(Path(__file__).resolve())]=sha(__file__)
 (out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'
def build(check,out,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();sys.path.insert(0,str(root/'nvn-native/tools'));from pipeline_qualification import checked
 c=json.loads(check.read_text())
 if c.get('schema')!=1 or c.get('kind')!='public-nvn-gob-pipeline-qualification-v1' or c.get('source_layout')!='gob' or c.get('source_flags')!=9 or c.get('target_pool_flags')!=164 or c.get('source_kind')!=254 or c.get('source_policy')!='immutable-native-no-NVIDIA-compression-Generic16Bx2-v1' or c.get('producer_sha256')!=sha(__file__):raise ValueError('unknown/stale public NVN GOB policy')
 for p,h in {**c['inputs'],**c['artifacts']}.items():
  if sha(p)!=h:raise ValueError('changed GOB qualifier dependency: '+p)
 checked(root,Path(c['pipeline_check']),Path(c['native_views_check']))
 return Path(c['header']),c,{**c['inputs'],**c['artifacts'],str(check):sha(check)}
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ('root','pipeline-check','native-views','out'):p.add_argument('--'+key,type=Path,required=True)
 a=p.parse_args();print(produce(a.root.resolve(),a.pipeline_check.resolve(),a.native_views.resolve(),a.out.resolve()))
if __name__=='__main__':main()

#!/usr/bin/env python3
"""Lock normal original sampled VS/FS and normal native conversion artifacts.

Only packages existing verified producer output. No compiler, runtime shader
rewrite, CPU converter, GPU run, original game/display success claim.
"""
import argparse,hashlib,importlib.util,json
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(path,name):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def build(check_path,out,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();c=json.loads(check_path.read_text())
 if c.get('schema')!=1 or c.get('kind')!='nvn-texture-consumer-qualification-v1' or c.get('producer_sha256')!=sha(__file__):raise ValueError('unknown/stale bridge qualification producer')
 for p,h in c['inputs'].items():
  if sha(p)!=h:raise ValueError('bridge qualification source/input changed: '+p)
 helper=load(Path(__file__).with_name('texture_bridge_bundle.py'),'nn_bridge_actual_artifacts')
 bh,b=helper.build(root,Path(c['bridge_check']))
 original=load(root/'nvn-native/adapters/d2r/texture_api_bundle.py','nn_bridge_original_inputs')
 th,t,hashes=original.build(Path(c['texture_check']),out)
 if t.get('modes') is not True or t['image']['schema']!=3 or t['program']['chip_id']!=0x06050002 or t['image']['profile']!=1:raise ValueError('schema3 A650 original sampled pair required')
 hashes.update(c['inputs']);hashes.update(b['inputs']);hashes.update(b['artifacts']);hashes[str(check_path)]=sha(check_path)
 return th,bh,c,hashes

def produce(root,texture_check,bridge_check,out):
 if out.exists() or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 out.mkdir(parents=True);helper=load(Path(__file__).with_name('texture_bridge_bundle.py'),'nn_bridge_check')
 bh,b=helper.build(root,bridge_check)
 original=load(root/'nvn-native/adapters/d2r/texture_api_bundle.py','nn_bridge_original_check')
 th,t,products=original.build(texture_check,out/'inputs')
 if t.get('modes') is not True or t['image']['schema']!=3 or t['program']['chip_id']!=0x06050002:raise ValueError('original pair must be schema3/A650')
 paths=[texture_check,bridge_check,Path(__file__).resolve(),Path(__file__).with_name('texture_bridge_bundle.py')]
 for n in ('texture_api_bundle.py','texture_bundle.py','graphics_api_bundle.py','graphics_bundle.py'):paths.append(root/'nvn-native/adapters/d2r'/n)
 for stage in ('vs','fs'):
  fixture=Path(t['inputs'][stage]['fixture']);paths.append(fixture)
  for blob in json.loads(fixture.read_text())['blobs'].values():paths.append((fixture.parent/blob['path']).resolve())
 inputs={str(p):sha(p) for p in paths};inputs.update(t.get('check_sources',{}));inputs.update(b['inputs']);inputs.update(b['artifacts'])
 for key in ('frontend_build','ir3_build'):inputs.update(t[key]['artifacts'])
 for path,h in t['artifacts'].items():inputs[str((texture_check.parent/path).resolve())]=h
 c=dict(schema=1,kind='nvn-texture-consumer-qualification-v1',producer_sha256=sha(__file__),texture_check=str(texture_check),bridge_check=str(bridge_check),inputs=inputs,
  scope='normal actual NVN GOB Register/Clear/Draw/writeback/Buffer alias qualification, authored caller with original VS/FS; no game/frame/fullsize/CPU_NO_ACCESS claim',
  cases=4,source_flags=8,target_flags=9,pool_flags=0xa4,source_width=2,source_height=2,target_width=64,target_height=64,submits=4,finishes=4,channels=65536,
  raw_names=['bridge-%d.bin'%i for i in range(4)],raw_bytes=65536)
 if any(sha(p)!=h for p,h in inputs.items()):raise ValueError('input changed during qualification packaging')
 (out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--texture-check',type=Path,required=True);p.add_argument('--bridge-check',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();print(produce(a.root.resolve(),a.texture_check.resolve(),a.bridge_check.resolve(),a.out.resolve()))
if __name__=='__main__':main()

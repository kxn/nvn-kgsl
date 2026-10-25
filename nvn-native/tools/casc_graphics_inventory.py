#!/usr/bin/env python3
"""Original CASC shader inventory/explicit bounded graphics extraction. No runtime or GPU.
Logical resources and source IDs select inputs, never imply shader state/variant answers.
"""
import argparse,hashlib,importlib.util,json,struct
from pathlib import Path
from workspace_paths import d2r_root

def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def load(p,n):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def inventory(root,romfs,states,out,resources=(),source_ids=()):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 if source_ids and not resources:raise ValueError('source IDs require explicit logical resources')
 reader=d2r_root()/'dm/tools/casc_overlay.py';casc=load(reader,'original_casc');shared=load(Path(__file__).with_name('offline_graphics_stages.py'),'graphics_container_contract')
 manifest=romfs/'data/build_manifest.psv';profile=json.loads(states.read_text());out.mkdir(parents=True)
 deps={str(q):sha(q)for q in(reader,Path(__file__).resolve(),Path(shared.__file__).resolve(),manifest,states)}
 rows=casc.read_manifest(romfs);selected=set(resources);wanted=set(source_ids);matched=set();selected_ids=set();result=[];entries=[];archives={};fixtures={}
 paths=sorted(p for p in rows if p.startswith('data/hd/shaders/')and p.endswith('.bin'))
 if selected-set(paths):raise ValueError('unknown logical resource '+repr(sorted(selected-set(paths))))
 for logical in paths:
  fields=rows[logical];ckey,decoded_size,ekey_hex,encoded_size,encoding=fields[4:9];ekey=bytes.fromhex(ekey_hex);encoded_size=int(encoded_size);decoded_size=int(decoded_size)
  if len(ekey)!=16 or len(bytes.fromhex(ckey))!=16 or min(encoded_size,decoded_size)<=0:raise ValueError('invalid CASC manifest shape')
  archive=(romfs/'data'/casc.data_filename(ekey[9],ekey[10])).resolve();offset=int.from_bytes(ekey[11:16],'big');archive_size=archive.stat().st_size
  if offset>archive_size or encoded_size>archive_size-offset:raise ValueError('encoded archive bounds '+logical)
  if str(archive)not in archives:archives[str(archive)]={'bytes':archive_size,'sha256':sha(archive)};deps[str(archive)]=archives[str(archive)]['sha256']
  with archive.open('rb')as f:f.seek(offset);encoded=f.read(encoded_size)
  if len(encoded)!=encoded_size:raise ValueError('short encoded resource')
  raw=casc.read_data_file(romfs/'data',ekey,encoded_size,encoding)
  if len(raw)!=decoded_size or hashlib.md5(raw).hexdigest()!=ckey:raise ValueError('original decoded length/ckey mismatch '+logical)
  def bounded(o,n):return 0<=o<=len(raw)and 0<=n<=len(raw)-o
  containers=[]
  for item in shared.declared_containers(raw,bounded):
   blobs={name:raw[item[name+'_offset']:item[name+'_offset']+item[name+'_bytes']]for name in('code','control','c1')}
   identity={name:{'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}for name,data in blobs.items()}
   sid=hashlib.sha256(json.dumps({'native_stage':item['native_stage'],'blobs':identity},sort_keys=True,separators=(',',':')).encode()).hexdigest()
   item={**item,'source_id':sid,'identity':identity};containers.append(item);matched.add(sid)
   if logical not in selected or(wanted and sid not in wanted):continue
   selected_ids.add(sid)
   if not item['supported_stage']:continue
   if sid not in fixtures:
    blobfiles={}
    for name,data in blobs.items():
     f=out/(sid+'.'+name+'.bin');f.write_bytes(data);blobfiles[name]={'path':f.name,'offset':0,'size':len(data),'sha256':sha(f)}
    doc={'schema':1,'stage':item['stage'],'profile':{'name':'embedded-graphics-container-v5','control_extent':{'kind':'caller-container-declared','bytes':item['control_bytes'],'container_version':item['container_version']}},'compiler':profile['compiler'],'state':profile['states'][item['stage']],'blobs':blobfiles,'provenance':{'resource_sha256':hashlib.sha256(raw).hexdigest(),'logical_resource':logical,'archive_sha256':archives[str(archive)]['sha256'],'encoded_offset':offset,'encoded_bytes':encoded_size,'container_offset':item['container_offset'],'code_offset':item['code_offset'],'control_offset':item['control_offset'],'caller_code_bytes':item['caller_code_bytes'],'caller_control_bytes':item['control_bytes'],'extractor_sha256':sha(__file__),'state_profile_sha256':sha(states),'state_basis':'explicit specialization input; not captured Draw/query state'}}
    fixture=out/(sid+'.fixture.json');fixture.write_text(json.dumps(doc,indent=2)+'\n');fixtures[sid]=(fixture,sha(fixture))
   fixture,h=fixtures[sid];entries.append({'fixture':str(fixture),'fixture_sha256':h,'stage':item['stage'],'source_id':sid,'logical_resource':logical,'container_offset':item['container_offset'],'code_bytes':item['code_bytes'],'c1_bytes':item['c1_bytes'],'control_bytes':item['control_bytes']})
  result.append({'logical_resource':logical,'archive':str(archive),'archive_bytes':archive_size,'encoded_offset':offset,'encoded_bytes':encoded_size,'encoded_sha256':hashlib.sha256(encoded).hexdigest(),'encoding':encoding,'ekey':ekey_hex,'ckey_md5':ckey,'decoded_bytes':len(raw),'decoded_sha256':hashlib.sha256(raw).hexdigest(),'control_magic_count':raw.count(bytes.fromhex('34127698')),'validated_containers':containers,'unrecognized_control_candidates':raw.count(bytes.fromhex('34127698'))-len(containers)})
 if wanted-matched:raise ValueError('unknown source identity')
 if wanted-selected_ids:raise ValueError('source identity is outside selected logical resources')
 if selected and not entries:raise ValueError('selected resources contain no supported VS/FS containers')
 for q,h in deps.items():
  if sha(q)!=h:raise ValueError('source changed '+q)
 doc={'schema':1,'kind':'declared-graphics-container-inventory-v1','scope':'original CASC all-resource metadata; selected complete v5 source/control/C1; no captured shader state/runtime/GPU claim','source_inputs':deps,'archives':archives,'resource_count':len(result),'resources':result,'selected_resources':sorted(selected),'selected_source_ids':sorted(selected_ids),'unique_extracted_stages':len(fixtures),'entries':entries,'unsupported_stage_counts':{str(st):sum(x['native_stage']==st for r in result for x in r['validated_containers'])for st in sorted({x['native_stage']for r in result for x in r['validated_containers']if not x['supported_stage']})}}
 p=out/'inventory.json';p.write_text(json.dumps(doc,indent=2)+'\n');return p

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--root',type=Path,required=True);a.add_argument('--out',type=Path,required=True);sub=a.add_subparsers(dest='mode',required=True)
 e=sub.add_parser('extract');e.add_argument('--romfs',type=Path,required=True);e.add_argument('--states',type=Path,required=True);e.add_argument('--resource',action='append',default=[]);e.add_argument('--source-id',action='append',default=[])
 c=sub.add_parser('compile');c.add_argument('--inventory',type=Path,required=True);c.add_argument('--frontend',type=Path,required=True);c.add_argument('--ir3',type=Path,required=True);c.add_argument('--spirv-tools',type=Path,required=True);c.add_argument('--unit-render-scale',action='store_true');c.add_argument('--pair',action='append',default=[],help='explicit VS-source-id:FS-source-id; default no Cartesian linking')
 x=a.parse_args();root=x.root.resolve();out=x.out.resolve()
 if x.mode=='extract':print(inventory(root,x.romfs.resolve(),x.states.resolve(),out,x.resource,x.source_id));return
 shared=load(Path(__file__).with_name('offline_graphics_stages.py'),'graphics_container_compile');inv=json.loads(x.inventory.read_text());pairs=[];kinds={e['source_id']:e['stage']for e in inv['entries']}
 for value in x.pair:
  p=value.split(':')
  if len(p)!=2 or kinds.get(p[0])!='vs'or kinds.get(p[1])!='fs':raise ValueError('explicit pair source identities/stages absent')
  pairs.append(p)
 print(shared.compile_all(root,x.inventory.resolve(),x.frontend.resolve(),x.ir3.resolve(),x.spirv_tools.resolve(),out,x.unit_render_scale,pairs))
if __name__=='__main__':main()

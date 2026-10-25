#!/usr/bin/env python3
"""Certified complete original VS/FS catalogue, independent from native variants.
Raw blobs remain ignored. No compiler, expected query, pair or game-address table.
"""
import argparse,hashlib,importlib.util,json,struct
from pathlib import Path
from workspace_paths import d2r_root

def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def produce(root,inventories,out,all_sources=False,source_ids=()):
 n=Path(__file__).resolve().parents[1]
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 if bool(all_sources)==bool(source_ids):raise ValueError('explicit --all-sources OR --source-id required')
 shared=module(n/'tools/offline_graphics_stages.py','source_containers');reader=module(d2r_root()/'dm/tools/casc_overlay.py','source_casc');fixture=module(n/'tools/shader_fixture.py','source_fixture')
 inputs={str(p):sha(p)for p in(Path(__file__).resolve(),Path(shared.__file__),Path(reader.__file__),Path(fixture.__file__),n/'include/nvn_native/shader_source.h')};entries={};unsupported={};origins=[];wanted=set(source_ids);matched=set()
 def pin(path,expected=None):
  path=Path(path).resolve();h=sha(path)
  if expected is not None and h!=expected:raise ValueError('changed normal original/tool '+str(path))
  inputs[str(path)]=h
 def add(stage,code,control,c1,declared,origin,expected_sid=None):
  native_stage=1 if stage=='vs'else 5
  identity={k:dict(bytes=len(b),sha256=hashlib.sha256(b).hexdigest())for k,b in [('code',code),('control',control),('c1',c1)]}
  sid=hashlib.sha256(json.dumps(dict(native_stage=native_stage,blobs=identity),sort_keys=True,separators=(',',':')).encode()).hexdigest()
  if expected_sid and sid!=expected_sid:raise ValueError('normal source identity changed')
  matched.add(sid)
  if not all_sources and sid not in wanted:return
  if declared not in(0x880,0x898)or len(control)!=declared or len(code)<128 or len(code)%8 or not 0<len(code)+len(c1)<=1048576 or (len(code)+len(c1))%8:raise ValueError('invalid complete declared source spans')
  if struct.unpack_from('<I',code)[0]!=0x12345678 or (struct.unpack_from('<I',code,0x30)[0]>>10)&15!=native_stage or struct.unpack_from('<I',control)[0]!=0x98761234 or struct.unpack_from('<II',control,0x700)!=(len(code),len(code)+len(c1)):raise ValueError('original stage/header/bounds contradiction')
  key=(native_stage,code,control,c1)
  if key not in entries:entries[key]=dict(stage=0 if stage=='vs'else 1,profile=1 if declared==0x880 else 2,source=code+c1,control=control,code_bytes=len(code),source_id=sid,origins=[])
  entries[key]['origins'].append(origin)
 for inv in inventories:
  inv=inv.resolve();d=json.loads(inv.read_text());pin(inv)
  if d.get('kind')!='declared-graphics-container-inventory-v1':raise ValueError('normal declared inventory required')
  for p,h in d['source_inputs'].items():pin(p,h)
  if d.get('resources'):
   manifest=next(Path(p)for p in d['source_inputs']if p.endswith('/data/build_manifest.psv'));romfs=manifest.parent.parent
   for resource in d['resources']:
    raw=reader.read_data_file(romfs/'data',bytes.fromhex(resource['ekey']),resource['encoded_bytes'],resource['encoding'])
    if len(raw)!=resource['decoded_bytes']or hashlib.sha256(raw).hexdigest()!=resource['decoded_sha256']or hashlib.md5(raw).hexdigest()!=resource['ckey_md5']:raise ValueError('decoded original MD5/SHA/length mismatch')
    def bounded(o,z):return 0<=o<=len(raw)and 0<=z<=len(raw)-o
    observed=list(shared.declared_containers(raw,bounded));old=resource['validated_containers']
    if len(observed)!=len(old):raise ValueError('declared container count changed')
    for x,y in zip(observed,old):
     if any(y.get(k)!=v for k,v in x.items()):raise ValueError('declared source metadata changed')
     if not x['supported_stage']:unsupported[str(x['native_stage'])]=unsupported.get(str(x['native_stage']),0)+1;continue
     blobs={k:raw[x[k+'_offset']:x[k+'_offset']+x[k+'_bytes']]for k in('code','control','c1')}
     add(x['stage'],blobs['code'],blobs['control'],blobs['c1'],x['control_bytes'],dict(inventory=str(inv),logical_resource=resource['logical_resource'],container_offset=x['container_offset']),y['source_id'])
  else:
   for e in d['entries']:
    p=Path(e['fixture']);pin(p,e['fixture_sha256']);v=json.loads(p.read_text());fixture.validate(v,p.parent)
    if v['stage']not in('vs','fs'):unsupported[v['stage']]=unsupported.get(v['stage'],0)+1;continue
    profile=v['profile'];extent=profile.get('control_extent',{})
    if profile.get('name')!='embedded-graphics-container-v5'or extent.get('kind')!='caller-container-declared'or extent.get('container_version')!=5:raise ValueError('only certified complete v5 caller-declared graphics inputs')
    b={}
    for k,row in v['blobs'].items():
     f=(p.parent/row['path']).resolve();pin(f);raw=f.read_bytes();b[k]=raw[row['offset']:row['offset']+row['size']]
    add(v['stage'],b['code'],b['control'],b['c1'],extent['bytes'],dict(inventory=str(inv),fixture=str(p),container_offset=e['container_offset']))
  origins.append(str(inv))
 if wanted-matched:raise ValueError('unknown source IDs')
 rows=sorted(entries.values(),key=lambda e:(len(e['control']),e['control'],len(e['source']),e['source']))
 if not rows or {r['stage']for r in rows}!={0,1}or len(rows)>65536:raise ValueError('complete VS/FS catalogue required')
 out.mkdir(parents=True);blob=bytearray();table=[];metadata=[];pool={}
 def store(raw):
  key=hashlib.sha256(raw).digest()
  if key in pool:
   off,size=pool[key]
   if size!=len(raw)or blob[off:off+size]!=raw:raise ValueError('digest collision')
   return off
  off=len(blob);blob.extend(raw);pool[key]=(off,len(raw));return off
 for r in rows:
  s=store(r['source']);c=store(r['control']);sid=bytes.fromhex(r['source_id']);table.append('{'+str(r['stage'])+','+str(r['profile'])+',nn_complete_source_blob+'+str(s)+',nn_complete_source_blob+'+str(c)+','+str(len(r['source']))+','+str(len(r['control']))+','+str(r['code_bytes'])+',{'+','.join(map(str,sid))+'}}')
  metadata.append({k:v for k,v in r.items()if k not in('source','control')}|dict(source_bytes=len(r['source']),control_bytes=len(r['control']),source_sha256=hashlib.sha256(r['source']).hexdigest(),control_sha256=hashlib.sha256(r['control']).hexdigest(),source_offset=s,control_offset=c))
 raw=out/'shader_sources.bin';raw.write_bytes(blob)
 asm=out/'shader_sources.S';asm.write_text('.section .rodata\n.balign 8\n.global nn_complete_source_blob\n.hidden nn_complete_source_blob\n.type nn_complete_source_blob,%object\nnn_complete_source_blob:\n.incbin '+json.dumps(str(raw))+'\n.size nn_complete_source_blob, .-nn_complete_source_blob\n.section .note.GNU-stack,"",%progbits\n')
 header=out/'shader_sources.h';header.write_text('#pragma once\n#include "nvn_native/shader_source.h"\nextern const struct nn_shader_source_archive NN_COMPLETE_GRAPHICS_SOURCE_ARCHIVE;\n')
 source=out/'shader_sources.c';source.write_text('#include "nvn_native/shader_source.h"\nextern const unsigned char nn_complete_source_blob[];\nstatic const struct nn_shader_source_entry entries[]={\n'+',\n'.join(table)+'\n};\nconst struct nn_shader_source_archive NN_COMPLETE_GRAPHICS_SOURCE_ARCHIVE={1,entries,sizeof(entries)/sizeof(*entries)};\n')
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('normal input changed '+p)
 manifest=dict(schema=1,kind='certified-original-graphics-source-catalogue-v1',passed=True,gpu_execution=False,shader_compilation=False,query_expectations=False,native_variants=0,entries=metadata,stage_counts={str(s):sum(r['stage']==s for r in rows)for s in(0,1)},source_bytes_before_blob_dedup=sum(len(r['source'])+len(r['control'])for r in rows),blob_bytes=len(blob),unsupported_stage_counts=unsupported,inventory_inputs=origins,header=str(header),source=str(source),assembly=str(asm),blob=str(raw),inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});(out/'check.json').write_text(json.dumps(manifest,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--inventory',type=Path,action='append',required=True);p.add_argument('--out',type=Path,required=True);g=p.add_mutually_exclusive_group(required=True);g.add_argument('--all-sources',action='store_true');g.add_argument('--source-id',action='append',default=[]);a=p.parse_args();print(produce(a.root.resolve(),a.inventory,a.out.resolve(),a.all_sources,a.source_id))

#!/usr/bin/env python3
"""Package immutable full-source graphics stage archive, without a draw template."""
import argparse,hashlib,json,struct
from pathlib import Path

def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def interface_check(program):
 # Common NIR/IR3 owns IO lowering, type declarations, write masks and undef
 # facts. No SPIR-V AccessChain/pointer/store parser is maintained here.
 vs=program['vs'];fs=program['fs'];usage=vs.get('nir_usage');consumer=fs.get('nir_usage')
 if not usage or not consumer or usage.get('schema')!=1 or consumer.get('schema')!=1 or usage.get('all_paths_initialized_proven')is not False or consumer.get('all_paths_initialized_proven')is not False:raise ValueError('missing bounded NIR interface reflection')
 def table(entries):
  result={}
  for e in entries:
   slot=e['slot']
   if type(slot)is not int or not 0<=slot<64 or slot in result:raise ValueError('unknown or duplicate NIR IO slot')
   result[slot]=e
  return result
 outputs=table(usage['outputs']);producer=table(usage['interfaces']['outputs']);inputs=table(consumer['interfaces']['inputs']);checks=[]
 for e in fs['inputs']:
  if not e['bary'] or not e['compmask']:continue
  slot=e['slot'];p=producer.get(slot);q=inputs.get(slot);writes=outputs.get(slot)
  if not p or not q or not writes or any(x.get('known')is not True for x in (p,q,writes)):raise ValueError('unknown NIR producer/consumer interface at slot'+str(slot))
  keys=('base_type','bits','components','component','interpolation','centroid','sample')
  if any(p[k]!=q[k] for k in keys):raise ValueError('NIR varying type/component/interpolation mismatch at slot'+str(slot))
  if e['compmask']&~writes['may_write_mask']:raise ValueError('fragment varying reads components absent from known NIR producer may-write mask at slot'+str(slot))
  checks.append({'slot':slot,'consumer_read_mask':e['compmask'],'producer':writes,'declaration':p})
 return {'basis':'common NIR lowered IO/SSA, native IR3 input compmask','component_coverage_only':True,'all_paths_initialized_proven':False,'checks':checks}

def texture_bindings(reflection):
 # Preserve trusted compiler reflection; do not reinterpret legacy
 # handle_sources labels or infer a source domain from native Type/Separate.
 result=[];seen=set()
 rows=reflection.get('textures')
 if not isinstance(rows,list) or len(rows)>16:raise ValueError('invalid texture reflection count')
 for row in rows:
  keys=('Binding','Set','Type','Format','ArrayLength','Flags','Separate','HandleIndex','CbufSlot')
  if not isinstance(row,dict) or any(k not in row for k in keys):raise ValueError('missing texture reflection field')
  for key in keys:
   if key=='Separate':
    if type(row[key])is not bool:raise ValueError('invalid texture reflection boolean')
   elif type(row[key])is not int:raise ValueError('invalid texture reflection integer')
  if not 0<=row['Binding']<16 or row['Binding'] in seen:raise ValueError('duplicate or unrepresentable texture binding')
  if any(not 0<=row[k]<1<<32 for k in ('Set','Type','Format','ArrayLength','Flags')):raise ValueError('unrepresentable texture reflection field')
  if row['ArrayLength']==0 or any(not -(1<<31)<=row[k]<(1<<31) for k in ('HandleIndex','CbufSlot')):raise ValueError('unrepresentable texture source reference')
  seen.add(row['Binding'])
  result.append({'binding':row['Binding'],'set':row['Set'],'type':row['Type'],'format':row['Format'],
   'array_length':row['ArrayLength'],'flags':row['Flags'],'separate':int(row['Separate']),
   'handle_word':row['HandleIndex'],'cbuf_slot':row['CbufSlot']})
 return result

def storage_bindings(reflection):
 rows=reflection.get('sbufs')
 if not isinstance(rows,list)or len(rows)>16:raise ValueError('invalid storage reflection count')
 result=[];seen=set()
 for row in rows:
  keys={'Slot','Binding','SbCbSlot','SbCbOffset','Flags'}
  if not isinstance(row,dict)or set(row)!=keys or any(type(v)is not int for v in row.values()):raise ValueError('invalid storage reflection fields')
  if not 0<=row['Slot']<16 or not 0<=row['Binding']<16 or row['Binding']in seen or any(not 0<=row[k]<1<<32 for k in keys)or row['Flags']&~1:raise ValueError('unknown storage reflection domain')
  seen.add(row['Binding']);result.append(dict(slot=row['Slot'],binding=row['Binding'],cbuf_slot=row['SbCbSlot'],cbuf_word=row['SbCbOffset'],flags=row['Flags']))
 return result

def gather_bindings(usage):
 # Independently retained actual common-NIR tg4 facts. An ordinary "other"
 # count is never interpreted as gather without this complete corroboration.
 g=usage.get('gather_usage')
 if g is None:return {}
 keys={'schema','basis','count','coordinate_domain','lod_mode','bindings'}
 if not isinstance(g,dict)or set(g)!=keys or type(g['schema'])is not int or g['schema']!=1 or g['basis']!='actual common NIR tg4 instructions' or g['coordinate_domain']!='normalized 2D texture operation' or g['lod_mode']!='level_zero':raise ValueError('unknown actual gather proof')
 if type(g['count'])is not int or not 0<g['count']<1<<32 or not isinstance(g['bindings'],list)or not 0<len(g['bindings'])<=16:raise ValueError('invalid gather count/table')
 resource=usage.get('resource_semantics',{})
 if resource.get('schema')!=1 or resource.get('basis')!='actual common NIR texture instructions':raise ValueError('gather lacks actual resource proof')
 rows=resource.get('bindings',[]);by_binding={}
 for row in rows:
  if not isinstance(row,dict)or set(row)!={'binding','fetches','size_queries','samples','offsets','other'}or any(type(v)is not int or not 0<=v<1<<32 for v in row.values())or row['binding']>=16 or row['binding']in by_binding:raise ValueError('invalid gather resource proof')
  by_binding[row['binding']]=row
 result={}
 for row in g['bindings']:
  if not isinstance(row,dict)or set(row)!={'binding','count','component_mask','semantic_sampler_mask','offsets'}or any(type(v)is not int or not 0<=v<1<<32 for v in row.values()):raise ValueError('invalid gather row')
  b=row['binding'];r=by_binding.get(b)
  if b>=16 or b in result or not row['count']or not 0<row['component_mask']<=15 or row['semantic_sampler_mask']!=1<<b or row['offsets']or not r or r['other']!=row['count']or r['offsets']:raise ValueError('uncorroborated/unsupported gather binding')
  result[b]=dict(row)
 if sum(r['count']for r in result.values())!=g['count']:raise ValueError('gather total differs')
 return result

def stage_texture_uses(check,key):
 known=True;facts=[]
 for pair in check['pairs']:
  if not pair['passed']:continue
  for stage in ('vs','fs'):
   if pair[stage]!=key:continue
   use=pair['program'][stage].get('nir_usage',{}).get('resource_semantics')
   if not isinstance(use,dict) or use.get('schema')!=1 or use.get('basis')!='actual common NIR texture instructions':
    known=False;continue
   rows=use.get('bindings')
   if not isinstance(rows,list) or len(rows)>16:raise ValueError('invalid actual NIR texture uses')
   checked=[];seen=set()
   for row in rows:
    keys=('binding','fetches','size_queries','samples','offsets','other')
    if not isinstance(row,dict) or set(row)!=set(keys) or any(type(row[k])is not int or not 0<=row[k]<1<<32 for k in keys):raise ValueError('unrepresentable actual NIR texture use')
    if row['binding']>=16 or row['binding'] in seen:raise ValueError('duplicate actual NIR texture use')
    seen.add(row['binding']);checked.append(dict(row))
   facts.append((sorted(checked,key=lambda v:v['binding']),gather_bindings(pair['program'][stage]['nir_usage'])))
 if facts and any(f!=facts[0] for f in facts):raise ValueError('same-stage native pair texture-use disagreement')
 if not known or not facts:return False,{}
 return True,{r['binding']:r for r in facts[0][0]}

def build(check,out,*,root=None,verify=False):
 root=(root or Path(__file__).resolve().parents[2]).resolve();check=Path(check).resolve();out=Path(out).resolve()
 if not out.is_relative_to(root/'.local'):raise ValueError('ignored output required')
 if out.exists() and not verify:raise ValueError('fresh ignored output required; use verify for an existing archive')
 c=json.loads(check.read_text());inputs={str(check):sha(check),str(Path(__file__).resolve()):sha(__file__)}
 if c.get('kind')!='offline-graphics-stage-compilation-v1' or c.get('gpu_execution') is not False or c.get('draw_pipeline_available') is not False or c.get('chip_id') not in (0x06050002,0x43030b00):raise ValueError('unknown stage compiler profile')
 def checked(path,h):
  path=Path(path)
  if sha(path)!=h:raise ValueError('changed stage dependency '+str(path))
  inputs[str(path)]=h
 for mapping in (c['inputs'],c['artifacts']):
  for p,h in mapping.items():checked(p,h)
 lines=['/* Generated immutable shader-only archive; no NVN/draw template/oracle. */','#pragma once','#include "nvn_native/offline_graphics.h"'];ids={};summary=[]
 def array(name,data,alignment=None):
  lines.append('static const unsigned char '+name+'[]'+(' __attribute__((aligned('+str(alignment)+')))'if alignment else'')+'={'+','.join(map(str,data))+'};');return name
 def string(name,value):
  lines.append('static const char '+name+'[]='+json.dumps(json.dumps(value,sort_keys=True,separators=(',',':')))+';');return name
 stage_init=[]
 for key,e in c['stages'].items():
  if not e['frontend_passed']:continue
  fixture=Path(e['fixture']);checked(fixture,e['fixture_sha256']);d=json.loads(fixture.read_text());blobs={}
  for n,b in d['blobs'].items():
   p=fixture.parent/b['path'];raw=p.read_bytes()[b['offset']:b['offset']+b['size']]
   if len(raw)!=b['size'] or hashlib.sha256(raw).hexdigest()!=b['sha256'] or {'size':len(raw),'sha256':b['sha256']}!=e['envelope']['inputs'][n]:raise ValueError('stage source changed')
   inputs[str(p.resolve())]=sha(p);blobs[n]=raw
  extent=d['profile'].get('control_extent',{});control=blobs['control'];code=blobs['code'];c1=blobs['c1']
  if d['profile'].get('name')!='embedded-graphics-container-v5' or extent!={'kind':'caller-container-declared','bytes':len(control),'container_version':5} or len(control) not in (0x880,0x898) or control[:4]!=bytes.fromhex('34127698') or struct.unpack_from('<II',control,0x700)!=(len(code),len(code)+len(c1)):raise ValueError('unknown full control/code/C1 provenance')
  prefix=Path(e['prefix']);spv=Path(str(prefix)+'.spv');resources=json.loads(Path(str(prefix)+'.resources.json').read_text());reflection=e['resources']
  if resources!=reflection or reflection['stage']!=d['stage'] or reflection['code_sha256']!=hashlib.sha256(code).hexdigest() or reflection['spv_sha256']!=sha(spv):raise ValueError('stage reflection mismatch')
  native_policy=c.get('native_backend_policy',{'unit_render_scale':False})
  if native_policy not in ({'unit_render_scale':False},{'unit_render_scale':True}):raise ValueError('unknown native backend policy')
  specialization={k:d[k] for k in ['schema','stage','profile','compiler','state']}
  if native_policy['unit_render_scale']:specialization['native_backend_policy']=native_policy
  state_blob=json.dumps(specialization,sort_keys=True,separators=(',',':'),allow_nan=False).encode();state_key=hashlib.sha256(state_blob).digest()
  i=len(ids);ids[key]=i;name='nn_ogs_'+str(i)
  array(name+'_source',code+c1);array(name+'_control',control);array(name+'_state',state_key);array(name+'_spv',spv.read_bytes(),alignment=4);string(name+'_reflection',reflection)
  cb=reflection['cbufs']
  if len(cb)>16 or any(type(b['Slot'])is not int or type(b['Binding'])is not int or not 0<=b['Slot']<16 or not 0<=b['Binding']<32 for b in cb):raise ValueError('invalid cbuf reflection')
  if cb:lines.append('static const struct nn_offline_cbuf '+name+'_cbufs[]={'+','.join('{'+str(b['Slot'])+','+str(b['Binding'])+'}' for b in cb)+'};')
  tb=texture_bindings(reflection)
  use_known,uses=stage_texture_uses(c,key)
  for t in tb:
   use=uses.get(t['binding'])
   t.update(use_known=int(use_known and use is not None),**{k:(use[k] if use else 0) for k in ('fetches','size_queries','samples','offsets','other')})
  if tb:lines.append('static const struct nn_offline_texture_binding '+name+'_textures[]={'+','.join('{'+','.join(str(v[k]) for k in ('binding','set','type','format','array_length','flags','separate','handle_word','cbuf_slot','use_known','fetches','size_queries','samples','offsets','other'))+'}' for v in tb)+'};')
  sb=storage_bindings(reflection)
  if sb:lines.append('static const struct nn_offline_storage_binding '+name+'_storage[]={'+','.join('{'+','.join(str(v[k])for k in ('slot','binding','cbuf_slot','cbuf_word','flags'))+'}'for v in sb)+'};')
  stage_init.append('{'+f'.stage={0 if d["stage"]=="vs" else 1},.source={name}_source,.control={name}_control,.source_bytes={len(code)+len(c1)},.control_bytes={len(control)},.code_bytes={len(code)},.specialization={name}_state,.spirv={name}_spv,.spirv_bytes={spv.stat().st_size},.cbufs='+ (name+'_cbufs' if cb else 'NULL')+f',.cbuf_count={len(cb)},.reflection_json={name}_reflection,.textures='+ (name+'_textures' if tb else 'NULL')+f',.texture_count={len(tb)}'+((',.storage='+name+'_storage,.storage_count='+str(len(sb)))if sb else'')+'}')
  summary.append({'index':i,'request_sha256':key,'stage':d['stage'],'specialization_sha256':state_key.hex(),'source_bytes':len(code)+len(c1),'c1_bytes':len(c1),'control_bytes':len(control),'native_backend_policy':native_policy,'reflection':reflection,'texture_bindings':tb,'native_available':any(p['passed'] and key in (p['vs'],p['fs']) for p in c['pairs'])})
 pair_init=[];pair_rejections=[];interfaces=[];native_ids=set()
 for p in c['pairs']:
  if not p['passed']:continue
  prefix=Path(p['prefix']);program=json.loads(Path(str(prefix)+'.link.json').read_text())
  if program!=p['program'] or program['chip_id']!=c['chip_id']:raise ValueError('linked compiler metadata changed')
  try:interfaces.append(interface_check(program))
  except ValueError as e:pair_rejections.append({'vs':p['vs'],'fs':p['fs'],'reason':str(e)});continue
  name='nn_ogp_'+str(len(pair_init));array(name+'_vs',Path(str(prefix)+'.vs.bin').read_bytes());array(name+'_fs',Path(str(prefix)+'.fs.bin').read_bytes());string(name+'_metadata',program)
  for stage in ('vs','fs'):
   if program[stage]['binary_bytes']!=Path(str(prefix)+'.'+stage+'.bin').stat().st_size:raise ValueError('native binary extent mismatch')
  native_ids.update((ids[p['vs']],ids[p['fs']]))
  pair_init.append('{'+f'.chip={c["chip_id"]},.vs_index={ids[p["vs"]]},.fs_index={ids[p["fs"]]},.vs_code={name}_vs,.fs_code={name}_fs,.vs_bytes={program["vs"]["binary_bytes"]},.fs_bytes={program["fs"]["binary_bytes"]},.compiler_json={name}_metadata'+'}')
 for entry in summary:entry['native_available']=entry['index'] in native_ids
 if not stage_init:raise ValueError('no frontend-ready stages')
 lines+=['static const struct nn_offline_graphics_stage nn_offline_stages[]={'+','.join(stage_init)+'};']
 if pair_init:lines+=['static const struct nn_offline_graphics_pair nn_offline_pairs[]={'+','.join(pair_init)+'};']
 lines+=['static const struct nn_offline_graphics_archive nn_offline_graphics={nn_offline_stages,'+str(len(stage_init))+','+('nn_offline_pairs' if pair_init else 'NULL')+','+str(len(pair_init))+'};']
 data=('\n'.join(lines)+'\n').encode();h=out/'offline_graphics_stages.h'
 if verify:
  if not h.is_file() or h.read_bytes()!=data:raise ValueError('cached stage header changed')
 else:out.mkdir(parents=True);h.write_bytes(data)
 for p,v in inputs.items():checked(p,v)
 result={'schema':1,'policy':'finite-offline-graphics-stages-v1','runtime_compiler':False,'draw_template':False,'gpu_execution':False,'stages':summary,'pair_count':len(pair_init),'interface_checks':interfaces,'linked_pair_rejections':pair_rejections,'inputs':inputs,'outputs':{str(h):sha(h)}}
 manifest=(json.dumps(result,indent=2)+'\n').encode();mp=out/'archive-manifest.json'
 if verify:
  if not mp.is_file() or mp.read_bytes()!=manifest:raise ValueError('cached stage manifest changed')
 else:mp.write_bytes(manifest)
 return h,result,{str(h):sha(h),str(mp):sha(mp)}
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--check',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--verify',action='store_true');a=p.parse_args();print(build(a.check,a.out,root=a.root,verify=a.verify)[0])

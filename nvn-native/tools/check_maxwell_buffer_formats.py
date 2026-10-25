#!/usr/bin/env python3
"""HOST ONLY: explicit shader-enum BUFFER specialization of original bounded C1 input.
No actual resource capture, GPU execution, native SNORM alias or BUFFER admission.
"""
import argparse,copy,hashlib,importlib.util,json,os,subprocess
from pathlib import Path

def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def load(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for arg in ('root','frontend','ir3','spirv-tools','ndk','fragment-fixture','vertex-fixture','out'):p.add_argument('--'+arg,type=Path,required=True)
 p.add_argument('--baseline-stages',type=Path,help='fresh normal old-frontend two-format check; every recorded input/artifact remains verified')
 a=p.parse_args();root=a.root.resolve();native=root/'nvn-native';out=a.out.resolve()
 if out.exists() or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 out.mkdir(parents=True);inputs={};runs=[];proof={'schema':1,'scope':__doc__,'passed':False,'gpu_execution':False,'public_buffer_gpu_admission':False,'host_snorm':False}
 def pin(q):q=q.resolve();inputs[str(q)]=sha(q)
 def run(cmd,good=True):
  r=subprocess.run(list(map(str,cmd)),text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  runs.append({'argv':list(map(str,cmd)),'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
  if (r.returncode==0)!=good:raise ValueError('unexpected command status: '+str(cmd)+'\n'+r.stderr)
  return r
 try:
  pin(Path(__file__));fixture=native/'tests/d2r_bridge/maxwell_buffer_format_inventory.c';pin(fixture)
  for folder in (native/'include',native/'third_party/nvn'):
   for q in folder.rglob('*.h'):pin(q)
  common=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(native/'include'),'-I'+str(native/'third_party/nvn')]
  inventories=[]
  for cc,san in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all'])]:
   binary=out/('inventory-'+cc);run([cc,*common,*san,fixture,'-o',binary]);inventories.append(json.loads(run([binary]).stdout))
  if inventories[0]!=inventories[1] or len(inventories[0])!=39:raise ValueError('inventory disagreement')
  rows=inventories[0];inventory_file=out/'formats.json';inventory_file.write_text(json.dumps(rows,indent=2)+'\n')
  ndk=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang';pin(ndk)
  run([ndk,*common,fixture,'-Wl,--no-undefined','-o',out/'inventory-api29'])
  front_record=a.frontend.resolve()/'build-record.json';pin(front_record);front=json.loads(front_record.read_text());dotnet=Path(front['commands'][0][0]);pin(dotnet)
  project=native/'tests/maxwell_texture_formats/TextureFormats.csproj';pin(project);pin(project.parent/'Program.cs')
  run([dotnet,'build',project,'-c','Release','--artifacts-path',out/'unit-artifacts','-p:NuGetAudit=false','--verbosity','quiet'])
  unit=json.loads(run([dotnet,out/'unit-artifacts/bin/TextureFormats/release/TextureFormats.dll',front['entrypoint'],inventory_file]).stdout)
  if unit['accepted']!=39 or unit['rejected']<24 or unit['host_snorm'] is not False:raise ValueError('unit contract')
  proof['unit']=unit
  fv=load(native/'tools/shader_fixture.py','format_fixture');pin(native/'tools/shader_fixture.py')
  docs={};original={}
  for stage,path in [('vs',a.vertex_fixture.resolve()),('fs',a.fragment_fixture.resolve())]:
   pin(path);doc=json.loads(path.read_text());envelope=fv.validate(doc,path.parent)
   if doc['stage']!=stage:raise ValueError('stage mismatch')
   if stage=='fs' and envelope['inputs']['c1']['size']!=256:raise ValueError('original C1 required')
   for blob in doc['blobs'].values():
    source=(path.parent/blob['path']).resolve();pin(source);blob['path']=str(source)
   docs[stage]=doc;original[stage]=envelope['inputs']
  generated=out/'fixtures';generated.mkdir();entries=[]
  def emit(doc,name):
   path=generated/(name+'.fixture.json');path.write_text(json.dumps(doc,indent=2)+'\n');envelope=fv.validate(doc,path.parent)
   if envelope['inputs']!=original[doc['stage']]:raise ValueError('source identity changed')
   entries.append({'fixture':str(path),'fixture_sha256':sha(path),'stage':doc['stage'],'container_offset':doc['provenance']['container_offset'] if 'container_offset' in doc['provenance'] else 0,'code_bytes':envelope['inputs']['code']['size'],'control_bytes':envelope['inputs']['control']['size'],'c1_bytes':envelope['inputs']['c1']['size']})
   return path
  emit(docs['vs'],'unchanged-vs')
  paths={}
  for row in rows:
   doc=copy.deepcopy(docs['fs']);doc['state']['host']['QueryHostSupportsSnormBufferTextureFormat']=False
   target=[t for t in doc['state']['resources']['textures'] if t['handle']==112 and t['cbuf_slot']==-1]
   if len(target)!=1 or target[0]['sampler_type']!='TextureBuffer':raise ValueError('explicit original BUFFER specialization required')
   target[0]['texture_format']=row['shader_name'];paths[row['shader_name']]=emit(doc,row['shader_name'])
  inv=out/'inventory.json';inv.write_text(json.dumps({'schema':1,'kind':'declared-graphics-container-inventory-v1','source_inputs':inputs.copy(),'entries':entries,'scope':'explicit finite BUFFER query specialization, not actual game resource bindings'},indent=2)+'\n')
  normal=load(native/'tools/offline_graphics_stages.py','format_stages');pin(native/'tools/offline_graphics_stages.py')
  check=normal.compile_all(root,inv,a.frontend.resolve(),a.ir3.resolve(),a.spirv_tools.resolve(),out/'stages',True)
  stages=json.loads(check.read_text());front_results=[]
  for row in rows:
   stage=next(v for v in stages['stages'].values() if v['fixture']==str(paths[row['shader_name']]))
   if not stage['frontend_passed']:raise ValueError('frontend/SPIRV failure '+row['shader_name']+': '+stage['failure'])
   queries=stage['resources']['queries'];fmt=[q for q in queries if q['name']=='QueryTextureFormat' and q.get('handle')==112 and q.get('cbufSlot')==-1]
   cap=[q for q in queries if q['name']=='QueryHostSupportsSnormBufferTextureFormat']
   if not fmt or any(q['value']!=row['shader'] for q in fmt) or not cap or any(q['value'] is not False for q in cap):raise ValueError('query identity/capability changed')
   if not any(c['Slot']==1 for c in stage['resources']['cbufs']) or any(q['name']=='ConstantBuffer1Read' for q in queries):raise ValueError('immutable original C1 must remain a runtime binding, never folded')
   pair=next(q for q in stages['pairs'] if stages['stages'][q['fs']] is stage)
   dis=run([a.spirv_tools.resolve()/'spirv-dis',stage['prefix']+'.spv']).stdout
   image_types=[line.strip() for line in dis.splitlines() if 'OpTypeImage' in line]
   sampled_type={'sampled_type_float':any(' Buffer ' in line and '%float ' in line for line in image_types),'image_types':image_types}
   native_artifacts={q.name.removeprefix(Path(pair['prefix']).name):sha(q) for q in Path(pair['prefix']).parent.glob(Path(pair['prefix']).name+'*') if q.is_file()}
   front_results.append({**row,**sampled_type,'native_artifacts':native_artifacts,'cbufs':stage['resources']['cbufs'],'request_sha256':stage['envelope']['request_sha256'],'inputs':stage['envelope']['inputs'],'frontend_passed':True,'query_capture':queries,'native_passed':pair['passed'],'native_failure':pair['failure'],'spv_sha256':sha(stage['prefix']+'.spv'),'native_bin_sha256':sha(pair['prefix']+'.fs.bin') if pair['passed'] else None})
  reference=next(row for row in front_results if row['shader_name']=='R32Float')
  non_snorm=[row for row in front_results if 'Snorm' not in row['shader_name']]
  if len(non_snorm)!=33 or any(row['spv_sha256']!=reference['spv_sha256'] for row in non_snorm):raise ValueError('non-SNORM SPIRV regression')
  for row in non_snorm:
   if row['native_passed']!=reference['native_passed']:raise ValueError('non-SNORM native result changed')
   if row['native_passed'] and row['native_artifacts']!=reference['native_artifacts']:raise ValueError('non-SNORM native metadata/code regression')
  if any(not row['sampled_type_float'] for row in front_results):raise ValueError('record actual sampled image type; do not infer from format query')
  proof['non_snorm_equal_outputs']=len(non_snorm)
  if a.baseline_stages:
   pin(a.baseline_stages);baseline=json.loads(a.baseline_stages.read_text())
   if baseline['native_backend_policy']!={'unit_render_scale':True} or len(baseline['pairs'])!=2 or not all(q['passed'] for q in baseline['pairs']):raise ValueError('baseline must be fresh old-frontend 2-pair normal compilation')
   for mapping in [baseline['inputs'],baseline['artifacts']]:
    for path,h in mapping.items():
     if sha(path)!=h:raise ValueError('baseline changed: '+path)
     inputs[path]=h
   compared=[]
   for pair in baseline['pairs']:
    stage=baseline['stages'][pair['fs']];fmt=next(q['value'] for q in stage['resources']['queries'] if q['name']=='QueryTextureFormat')
    if fmt not in (10,25) or stage['envelope']['inputs']!=original['fs']:raise ValueError('baseline identity/format mismatch')
    row=next(q for q in front_results if q['shader']==fmt)
    prefix=Path(pair['prefix']);artifacts={q.name.removeprefix(prefix.name):sha(q) for q in prefix.parent.glob(prefix.name+'*') if q.is_file()}
    if sha(stage['prefix']+'.spv')!=row['spv_sha256'] or artifacts!=row['native_artifacts']:raise ValueError('old-format compilation regression')
    compared.append(fmt)
   if sorted(compared)!=[10,25]:raise ValueError('missing old format regression')
   proof['baseline_format_equal']=compared
  # Run complete original fixture through normal frontend for namespace failures too.
  negatives=[]
  for name,value in [('public-name','NVN_FORMAT_RGBA32F'),('public-number',46),('hardware','FMT6_32_32_32_32_FLOAT'),('numeric-string','34'),('unknown','Unknown'),('missing',None),('missing-capability',None)]:
   doc=copy.deepcopy(json.loads(paths['R32G32B32A32Float'].read_text()));target=next(t for t in doc['state']['resources']['textures'] if t['handle']==112 and t['cbuf_slot']==-1)
   if name=='missing-capability':del doc['state']['host']['QueryHostSupportsSnormBufferTextureFormat']
   elif value is None:del target['texture_format']
   else:target['texture_format']=value
   path=out/(name+'.fixture.json');path.write_text(json.dumps(doc,indent=2)+'\n');prefix=out/name;r=run([dotnet,front['entrypoint'],path,prefix],False)
   if Path(str(prefix)+'.spv').exists():raise ValueError('negative emitted SPIRV')
   negatives.append({'name':name,'stderr':r.stderr})
  if any(sha(q)!=h for q,h in inputs.items()):raise ValueError('input changed during check')
  proof.update(passed=True,unit=unit,formats=front_results,frontend_passed=39,native_passed=sum(x['native_passed'] for x in front_results),original_inputs=original,negative_frontend=negatives,stage_check=str(check),source_policy='explicit query test specialization; no actual NVN descriptor inference; SNORM SINT alias not admitted')
 except Exception as e:proof['error']=str(e)
 proof.update(inputs=inputs,runs=runs)
 proof['artifacts']={str(q):sha(q) for q in out.rglob('*') if q.is_file()}
 (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps({'passed':proof['passed'],'native_passed':proof.get('native_passed'),'check':str(out/'check.json')}));return 0 if proof['passed'] else 1
if __name__=='__main__':raise SystemExit(main())

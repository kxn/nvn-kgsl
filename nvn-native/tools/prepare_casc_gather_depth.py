#!/usr/bin/env python3
"""Normal source+closed-math inputs for original CASC Gather/FragDepth qualifier.
This does not implement a public Draw, GPU run, shader VM or alternate compiler.
"""
import argparse,hashlib,importlib.util,json,struct,sys,subprocess,shutil
from pathlib import Path
from casc_gather_depth_oracle import cases
# This is one explicit original-source QUALIFICATION oracle, never runtime
# shader selection. Different source bytes require a separately reviewed math
# provider; compatible resource masks alone are not semantic equivalence.
ORIGINAL_SOURCE={
 'vs':{'code':(512,'db9ec713cbd0073bc5220cb48d2db41088e73c03f6e2b2de0ad2f8500f6f7d4c'),'control':(2176,'69cfdc6545a6691505428cdf1cce1bed1fe17912e02103331217cdc66ae73e4d'),'c1':(0,'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855')},
 'fs':{'code':(512,'18367b9bfb66118af30d3303413b993c84342b0f6a5ea0a9e84a6fd5e6094b9b'),'control':(2176,'5ddee022e68e6a2deba52be68125e0caa422b26f37097a4c4188b1f78fa11b77'),'c1':(256,'c129b1ecc96a9117d2915573e88dbaa60cf19ca6110263c99c8e18d4c2f2c462')}}

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(path,name):
 sys.path.insert(0,str(path.parent))
 try:
  spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
 finally:sys.path.pop(0)

def produce(root,pipeline,vs_key,fs_key,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh worktree/.local output required')
 n=root/'nvn-native';p=json.loads(pipeline.read_text())
 if p.get('kind')!='native-graphics-pipeline-recipes-v1'or p.get('chip')!=0x06050002 or p.get('gpu_execution')is not False:raise ValueError('actual normal A650 package required')
 pkg=module(n/'tools/package_graphics_pipelines.py','casc_depth_actual_package')
 kw={}
 if p.get('defer_depth_recipes'):kw['defer_depth_recipes']=True
 if p.get('publish_depth_recipes'):kw['publish_depth_recipes']=True
 header=pkg.build(root,Path(p['stage_check']),Path(p['compiler_record']).parent,p['gmem_bytes'],pipeline.parent,verify=True,**kw)
 stages=json.loads(Path(p['stage_check']).read_text());ordered=list(stages['stages']);vi=ordered.index(vs_key);fi=ordered.index(fs_key)
 pairs=[x for x in p['pairs']+p.get('deferred_pairs',[])if x['vs_index']==vi and x['fs_index']==fi]
 if len(pairs)!=1:raise ValueError('explicit source pair absent or ambiguous')
 pair=pairs[0];m=pair['metadata'];t=pair['native_texture_types']
 depth=m.get('depth_export',{});expected={'schema':1,'recipe_version':2,'writes_depth':True,'color_outputs':0,'late_z_required':True,'lrz_disabled_required':True}
 if any(depth.get(k)!=v for k,v in expected.items())or type(depth.get('regid'))is not int or not 0<=depth['regid']<252 or m['texture_mask']!=2 or m['semantic_sampler_mask']!=2 or m['fragcoord_mask']!=3 or m['has_kill']is not False:raise ValueError('different original depth/resource contract')
 if t['dimensions']!=[0,2]+[0]*14 or t['operations']!=[0,16]+[0]*14:raise ValueError('actual NIR Gather2D facts differ')
 inputs={**p['inputs'],**p['outputs'],str(pipeline):sha(pipeline)};identities={}
 sf=module(n/'tools/shader_fixture.py','casc_depth_actual_fixture')
 for stage,key in [('vs',vs_key),('fs',fs_key)]:
  row=stages['stages'][key];f=Path(row['fixture']);fixture=json.loads(f.read_text());envelope=sf.validate(fixture,f.parent)
  if envelope!=row['envelope']or fixture['stage']!=stage:raise ValueError('actual full source fixture differs')
  if {role:(entry['size'],entry['sha256'])for role,entry in envelope['inputs'].items()}!=ORIGINAL_SOURCE[stage]:raise ValueError('closed oracle is not defined for different original source bytes')
  blobs={}
  for role,entry in fixture['blobs'].items():
   path=(f.parent/entry['path']).resolve();raw=path.read_bytes()[entry['offset']:entry['offset']+entry['size']]
   if len(raw)!=entry['size']or hashlib.sha256(raw).hexdigest()!=entry['sha256']:raise ValueError('source slice identity '+role)
   inputs[str(path)]=sha(path);blobs[role]=dict(entry,path=str(path))
   if stage=='fs'and role=='c1':
    if len(raw)!=256 or struct.unpack_from('<f',raw)[0]!=-1.:raise ValueError('this closed oracle requires original full C1 with -1 at offset0')
  identities[stage]=dict(request_key=key,stage_index=vi if stage=='vs'else fi,fixture=str(f),fixture_sha256=sha(f),envelope=envelope,blobs=blobs,resources=row['resources']);inputs[str(f)]=sha(f)
 if identities['vs']['resources']['cbufs']!=[{'Slot':6,'Binding':1}]or identities['fs']['resources']['cbufs']!=[{'Slot':1,'Binding':2}]:raise ValueError('actual original CB mapping differs')
 maps=identities['fs']['resources']['textures']
 wanted={'kind':'SeparateSamplerHandle','resolved':True,'texture':{'cbuf_slot':2,'word_offset':110,'source':'texture handle word'},'sampler':{'cbuf_slot':2,'word_offset':352,'source':'packed sampler handle word'}}
 if len(maps)!=1 or maps[0]['Type']!=3 or maps[0]['Binding']!=1 or maps[0]['handle_sources']!=wanted:raise ValueError('actual original packed sampler source differs')
 # These dumped NIRs belong to this exact pair's recorded native-code artifact.
 # Preserve them as evidence; the closed oracle is manually derived algebra,
 # not an instruction interpreter or guessed source-SPIR-V dimensional tag.
 matching=[]
 paths=[str(stages['stages'][key]['prefix'])+'.spv'for key in(vs_key,fs_key)]
 for command in p['commands']:
  argv=command['argv']
  if command['returncode']==0 and paths[0]in argv and paths[1]in argv:
   i=argv.index(paths[0])
   if argv[i+1]!=paths[1]:raise ValueError('unexpected normal paired compiler command')
   prefix=argv[i+2]
   if all(prefix+suffix in p['outputs']for suffix in('.vs.nir','.fs.nir','.link.json','.vs.bin','.fs.bin')):matching.append(prefix)
 if len(matching)!=1:raise ValueError('exact paired NIR compiler command absent or ambiguous')
 stem=matching[0];vsnir=Path(stem+'.vs.nir').read_text();fsnir=Path(stem+'.fs.nir').read_text()
 for token in ('tg4','gather_component','FRAG_RESULT_DEPTH','@load_frag_coord','fmin','fmul','range_base=0, range=4'):
  if token not in fsnir:raise ValueError('actual fragment algebra evidence missing '+token)
 for value in (40,44,48,52):
  if 'range_base='+str(value)+', range=4'not in vsnir:raise ValueError('actual vertex CB6 offset absent')
 here=Path(__file__).resolve();oracle=here.with_name('casc_gather_depth_oracle.py');inputs[str(here)]=sha(here);inputs[str(oracle)]=sha(oracle)
 out.mkdir(parents=True)
 case_header=out/'casc_gather_depth_cases.h'
 case_header.write_text('/* Authored finite actual-resource fixture catalogue. */\n#pragma once\nstruct nn_casc_gather_depth_case {unsigned format,width,height,origin,wrap,mip,compare;};\nstatic const struct nn_casc_gather_depth_case NN_CASC_GATHER_DEPTH_CASES[]={\n'+',\n'.join('{'+','.join(str(c[k])for k in('format','width','height','origin','wrap_index','selected_mip','compare'))+'}'for c in cases())+'\n};\n#define NN_CASC_GATHER_DEPTH_CASE_COUNT 36u\n')
 own=Path(__file__).resolve().parents[1];utility=out/'canonical-layout';cc=Path(shutil.which('cc')).resolve();layout=own/'tests/casc_gather_depth_layout.c';src=n/'src/texture_source.c'
 argv=[str(cc),'-std=c11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(n/'include'),'-DNN_CASC_GATHER_CASES_HEADER='+json.dumps(str(case_header)),str(layout),str(src),'-Wl,--gc-sections','-o',str(utility)]
 run=subprocess.run(argv,capture_output=True,text=True)
 (out/'commands.json').write_text(json.dumps([dict(argv=argv,returncode=run.returncode,stdout=run.stdout,stderr=run.stderr)],indent=2)+'\n')
 if run.returncode:raise ValueError(run.stderr)
 result=subprocess.run([utility],capture_output=True,text=True)
 if result.returncode:raise ValueError('normal canonical metadata tool failed')
 canonical=json.loads(result.stdout)
 if len(canonical)!=36:raise ValueError('canonical target catalogue incomplete')
 for c,want in zip(canonical,cases()):
  if any(c[k]!=want[k]for k in('case','format','width','height','origin','wrap_index','selected_mip','compare','clear_depth_bits')):raise ValueError('normal C/independent oracle catalogue mismatch')
 (out/'canonical.json').write_text(json.dumps(canonical,indent=2)+'\n')
 for path in(cc,layout,src,n/'src/texture_volume.inc',n/'include/nvn_native/texture_source.h',n/'include/nvn_native/texture_volume.h',n/'include/nvn_native/texture_buffer_format.h'):inputs[str(path)]=sha(path)
 record=dict(schema=1,kind='original-casc-gather-fragdepth-oracle-inputs-v1',passed=True,gpu_execution=False,public_NVN_executed=False,producer_sha256=sha(here),pipeline_check=str(pipeline),pipeline_header=str(header),identities=identities,selected_pair=pair,actual_paired_NIR_prefix=stem,cases=cases(),canonical=canonical,case_header=str(case_header),planned_draws=72,source_format=10,source_shape=[17,11],source_value='(1+(x+2*y+5*repeat)%7)/8',source_initializer='authored canonical input bytes only, never CPU converted GPU output',original_C1_bytes=256,original_C1_first_float=-1.,VS_public_uniform_index=3,VS_source_CB=6,VS_native_UBO=1,FS_C1_native_UBO=2,texture_selector=10,sampler_selector=3,math='k=1 when fract((FragCoord.x+FragCoord.y)/2)>=.5, else original C1[0]=-1; z=min(gatherR_i*k)*k; hence parity-even MIN and parity-odd MAX',qualifier_scope='Explicit original CASC source with authored actual public resource/state fixture; not captured D2R Draw state, not game execution, GPU still pending',public_depth_consumer_pending=True,inputs=inputs,artifacts={str(path):sha(path)for path in out.iterdir()if path.is_file()})
 if any(sha(k)!=v for k,v in inputs.items()):raise ValueError('normal/source artifact changed during preparation')
 (out/'check.json').write_text(json.dumps(record,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for key in('root','pipeline-check','out'):p.add_argument('--'+key,type=Path,required=True)
 for key in('vs-key','fs-key'):p.add_argument('--'+key,required=True)
 a=p.parse_args();print(produce(a.root.resolve(),a.pipeline_check.resolve(),a.vs_key,a.fs_key,a.out.resolve()))

#!/usr/bin/env python3
"""Normal original-source PUBLIC FLOAT2D sampled22 input/one-texel oracle producer.

Mesa packing/unpacking runs only in HOST tools. Android gets immutable authored
texel/expected tables, never a CPU image codec or fake Maxwell source envelope.
"""
import argparse,hashlib,importlib.util,json,math,os,shlex,shutil,subprocess,sys
from pathlib import Path
KIND='public-nvn-float2d-sampled-qualification-v1'
FORMATS=(1,2,5,6,7,10,13,14,17,18,19,22,37,38,41,42,43,46,56,63,154,156)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p,name):
 sys.path.insert(0,str(p.parent))
 try:
  s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
 finally:sys.path.pop(0)
def dependencies(root,pipeline,fdl,kernel,pair_index):
 n=root/'nvn-native';p=json.loads(pipeline.read_text())
 ph=load(n/'tools/package_graphics_pipelines.py','sampled_public_graphics').build(root,Path(p['stage_check']),Path(p['compiler_record']).parent,p['gmem_bytes'],pipeline.parent,True)
 def verified_origin(path,anchor,rels,tool,name):
  record=json.loads(path.read_text());origins=[Path(x).parents[2]for x in record.get('inputs',{})if x.endswith('/nvn-native/'+anchor)]
  if len(origins)!=1:raise ValueError('ambiguous normal dependency origin '+anchor)
  # Verify at the declared original normal producer root, then independently
  # require every adapter source to equal this build's actual source bytes.
  result=load(n/tool,name).verify(origins[0],path)
  for rel in rels:
   original=origins[0]/'nvn-native'/rel
   if record['inputs'].get(str(original))!=sha(n/rel):raise ValueError('normal dependency differs from current source '+rel)
  return result
 library,f=verified_origin(fdl,'src/runtime_fdl.cc',('tools/build_runtime_fdl.py','src/runtime_fdl.cc','include/nvn_native/runtime_fdl.h','include/nvn_native/texture_buffer_format.h','tests/runtime_fdl.cc'),'tools/build_runtime_fdl.py','sampled_public_fdl')
 k=verified_origin(kernel,'shaders/block_copy.comp',('tools/compile_block_copy.py','shaders/block_copy.comp'),'tools/compile_block_copy.py','sampled_public_block')
 if p.get('chip')!=0x06050002 or p.get('gmem_bytes')!=1179648 or type(pair_index)is not int or not 0<=pair_index<len(p['pairs']):raise ValueError('explicit normal A650 original pair required')
 pair=p['pairs'][pair_index];stages=list(json.loads(Path(p['stage_check']).read_text())['stages'].values());selected=[stages[pair[x+'_index']]for x in ('vs','fs')]
 identity=[('c1906ac598c15e11a3029bff6db444f45b4343ec56ffb15624eff0be5f7d7369','96757bb36a024b91e6f161b424af3cc3db2f035f059842e5151ba7fb1ff89cc4'),('a98bd42200fa86b7c94c19ecb528449043c6fb71c235dafbf46ce20f7c8fdbda','a7672f1d3bfeb7d0312812c8e98eea92565d5934bad06bbb5f345e5a684f92e5')]
 for stage,(code,control)in zip(selected,identity):
  i=stage['envelope']['inputs']
  if i['code']['sha256']!=code or i['control']['sha256']!=control or i['control']['size']!=2176 or i['c1']['size']!=0:raise ValueError('changed complete original source/control/C1 oracle identity')
 t=pair['native_texture_types'];m=pair['metadata']
 if t.get('schema')!=1 or t.get('basis')!='actual common NIR texture instructions' or t.get('dimensions')!=[0,2]+[0]*14 or t.get('operations')!=[0,1]+[0]*14 or t.get('texture_mask')!=2 or t.get('sampler_mask')!=2 or not m.get('has_kill') or m.get('vertex_inputs')!=3:raise ValueError('changed actual original SAMPLE shader facts')
 nir=pipeline.parent/('pair-'+str(pair_index)+'.fs.nir');text=nir.read_text()
 if text.count(' = fabs ')<3 or ' = flog2 'not in text or '@terminate'not in text:raise ValueError('original shader oracle fabs/gamma/KIL evidence missing')
 inputs={str(x):sha(x)for x in(pipeline,fdl,kernel,nir)}
 for record in(p,f,k):inputs.update(record['inputs']);inputs.update(record.get('artifacts',record.get('outputs',{})))
 return ph,library,p,k,pair,selected,inputs

def write_patterns(path,rows):
 lines=['/* Normal HOST Mesa ONE-texel authored fixture table. No runtime codec. */','#pragma once','#include <stdint.h>','#include <string.h>',
 'struct nn_public_sampled_fixture {unsigned format,cpp;unsigned char packed[8][16],expected[8][4];};','static const struct nn_public_sampled_fixture nn_public_sampled_fixtures[]={']
 for r in rows:
  lines.append('{'+str(r['format'])+','+str(r['banks'][0]['cpp'])+',{'+','.join('{'+','.join(map(str,b['packed']))+'}'for b in r['banks'])+'},{'+','.join('{'+','.join(map(str,b['expected']))+'}'for b in r['banks'])+'}},')
 lines+=['};','#define NN_PUBLIC_SAMPLED_FORMAT_COUNT 22u',
 'static inline unsigned nn_public_sampled_bank(unsigned x,unsigned y,unsigned mip,unsigned repeat){return(x+2*y+3*mip+5*repeat)&7u;}',
 'static inline unsigned nn_public_sampled_base(unsigned view,unsigned repeat){const unsigned v[3][2]={{0,1},{1,2},{3,0}};return v[view][repeat];}',
 'static inline unsigned nn_public_sampled_count(unsigned view,unsigned repeat){const unsigned v[3][2]={{4,3},{2,2},{1,1}};return v[view][repeat];}']
 path.write_text('\n'.join(lines)+'\n')

def produce(root,pipeline,fdl,kernel,pair_index,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 own=Path(__file__).resolve().parents[1];ph,library,p,k,pair,selected,inputs=dependencies(root,pipeline,fdl,kernel,pair_index)
 for rel in('tools/nvn_pipeline_sampled_qualification.py','tools/nvn_pipeline_sampled_validation.py','tools/nvn_pipeline_sampled_runner.py','tests/nvn_pipeline_probe.c','tests/nvn_pipeline_sampled_probe.inc','tests/sampled_fixture_pack.c','tests/nvn_pipeline_sampled_pattern.c','android/nvn_pipeline_probe_activity.c'):
  inputs[str(own/rel)]=sha(own/rel)
 for rel in('src/clear_color_pattern.c','src/texture_source.c','tools/check_clear_color.py'):
  inputs[str(root/'nvn-native'/rel)]=sha(root/'nvn-native'/rel)
 compiler=Path(p['compiler_record']).parent;host=compiler/'build';normal_entries=host/'compile_commands.json';inputs[str(normal_entries)]=sha(normal_entries)
 he=next(e for e in json.loads(normal_entries.read_text())if e['file'].endswith('/format/u_format_other.c'));_,hf=load(root/'nvn-native/tools/check_clear_color.py','sampled_actual_pack_flags').flags(he)
 lib=host/'src/util/libmesa_util.a';inputs[str(lib)]=sha(lib)
 out.mkdir(parents=True);commands=[];rows=None;oracle=[]
 def run(argv,cwd=None,binary=False):
  argv=list(map(str,argv));r=subprocess.run(argv,cwd=cwd,capture_output=True,text=not binary,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  commands.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout_bytes=len(r.stdout)if binary else None,stdout_sha256=hashlib.sha256(r.stdout).hexdigest()if binary else None,stdout=None if binary else r.stdout,stderr=r.stderr.decode(errors='replace')if binary else r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(str(r.stderr)[-5000:])
  return r.stdout
 for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
  tool=Path(shutil.which(cc)).resolve();inputs[str(tool)]=sha(tool);flags=hf if cc=='gcc'else[x for x in hf if x not in('-Wno-error=maybe-uninitialized','-Wno-nonnull-compare')]
  exe=out/('pack-'+cc);run([cc,*flags,'-I'+str(own/'include'),'-I'+str(own/'third_party/nvn'),'-DNN_CLEAR_PATTERN_SOURCE='+json.dumps(str(root/'nvn-native/src/clear_color_pattern.c')),*san,'-fno-pie','-no-pie',own/'tests/sampled_fixture_pack.c',lib,'-lm','-ldl','-lpthread','-o',exe],host)
  r=json.loads(run([exe],host))
  if tuple(v['format']for v in r)!=FORMATS or any(len(v['banks'])!=8 for v in r):raise ValueError('full22 actual Mesa fixture set required')
  for f in r:
   for b in f['banks']:
    u=b['unpacked'];expected=[17,29,43,59]if u[3]==0 else[math.floor(min(1,max(0,abs(v)if i<3 else v))*255+.5)for i,v in enumerate(u)]
    if b['expected']!=expected or any(not math.isfinite(v)for v in u):raise ValueError('independent original shader math/Mesa oracle mismatch')
  if rows is not None and rows!=r:raise ValueError('sanitized normal Mesa pack/unpack outputs differ')
  rows=r;oracle.append(dict(compiler=cc,formats=22,banks=176,negative_float_snorm=True,host_only=True))
 patterns=out/'sampled-patterns.h';write_patterns(patterns,rows);(out/'patterns.json').write_text(json.dumps(rows,indent=2)+'\n')
 cases=None;expected=None
 for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
  exe=out/('pattern-'+cc);run([cc,'-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-fno-pie','-no-pie',*san,'-I'+str(own/'include'),'-DNN_SAMPLED_PATTERNS='+json.dumps(str(patterns)),own/'tests/nvn_pipeline_sampled_pattern.c',root/'nvn-native/src/texture_source.c','-lm','-o',exe])
  c=json.loads(run([exe,'--cases']));raw=run([exe],binary=True)
  if cases is not None and(cases!=c or expected!=raw):raise ValueError('C sanitized fixture case/pixel outputs differ')
  cases,expected=c,raw
 if len(cases)!=132 or len(expected)!=264*16384:raise ValueError('incomplete22x2shape3view2repeat matrix')
 (out/'cases.json').write_text(json.dumps(cases,indent=2)+'\n');(out/'oracle.rgba8').write_bytes(expected)
 validation=load(own/'tools/nvn_pipeline_sampled_validation.py','sampled_independent_pixels')
 if expected!=b''.join(validation.pixels(c,r,rows)for c in cases for r in range(2)):raise ValueError('C/Python immutable-table index/UV oracle mismatch')
 header=out/'nvn_pipeline_sampled_qualification.h';header.write_text('/* Complete original declared880 source/C1; fixture selection only. */\n#include '+json.dumps(str(ph))+'\n#include '+json.dumps(k['header'])+'\n#include '+json.dumps(str(patterns))+'\n#define NN_PUBLIC_SAMPLED_PAIR '+str(pair_index)+'u\n#define NN_PUBLIC_SAMPLED_BLOCK_IMAGE NVN_COMPILED_IMAGE\n')
 for path,h in inputs.items():
  if sha(path)!=h:raise ValueError('normal sampled qualifier input changed '+path)
 record=dict(schema=1,kind=KIND,producer_sha256=sha(__file__),pipeline_check=str(pipeline),runtime_fdl_check=str(fdl),kernel_check=str(kernel),pair_index=pair_index,pair=pair,header=str(header),runtime_fdl_library=str(library),original_stage_inputs=[s['envelope']['inputs']for s in selected],graphics_control_bytes=2176,source_levels=4,formats=list(FORMATS),cases=cases,patterns=rows,handles=132,repeats=2,submissions=264,channel_checks=4325376,guard_byte_checks=2162688,readonly_byte_checks=sum(2*(8192+2*c['source_pool_bytes']+32768+65536+8192)for c in cases),oracle_checks=oracle,inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()},gpu_execution=False,android_execution=False,public_nvn=True,scope='Normal original source-backed SAMPLE22 full/selected-mip + late-registration/replay qualifier. Host Mesa ONE-texel fixture/oracle only. GPU pending. mixed FETCH/SIZE+BUFFER followup; no public Gather/FragDepth or game claim.')
 (out/'check.json').write_text(json.dumps(record,indent=2)+'\n');return out/'check.json'

def build(check,out,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();c=json.loads(check.read_text())
 if c.get('schema')!=1 or c.get('kind')!=KIND or c.get('producer_sha256')!=sha(__file__)or c.get('formats')!=list(FORMATS)or c.get('submissions')!=264:raise ValueError('unknown/stale sampled22 qualifier')
 for p,h in {**c['inputs'],**c['artifacts']}.items():
  if sha(p)!=h:raise ValueError('sampled22 source/product changed '+p)
 ph,lib,p,k,pair,stages,required=dependencies(root,Path(c['pipeline_check']),Path(c['runtime_fdl_check']),Path(c['kernel_check']),c['pair_index'])
 if c['runtime_fdl_library']!=str(lib)or c['pair']!=pair or c['original_stage_inputs']!=[s['envelope']['inputs']for s in stages]or any(c['inputs'].get(p)!=h for p,h in required.items()):raise ValueError('sampled22 provenance/source closure differs')
 if c['cases']!=json.loads((check.parent/'cases.json').read_text())or c['patterns']!=json.loads((check.parent/'patterns.json').read_text()):raise ValueError('sampled22 immutable fixture changed')
 return Path(c['header']),c,{**c['inputs'],**c['artifacts'],str(check):sha(check)}
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__)
 for n in('root','pipeline-check','runtime-fdl-check','kernel-check','out'):a.add_argument('--'+n,type=Path,required=True)
 a.add_argument('--pair-index',type=int,required=True);x=a.parse_args();print(produce(x.root.resolve(),x.pipeline_check.resolve(),x.runtime_fdl_check.resolve(),x.kernel_check.resolve(),x.pair_index,x.out.resolve()))

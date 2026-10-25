#!/usr/bin/env python3
"""Normal complete-original public depth-source sampling inputs; never GPU pixels."""
import argparse,json,shutil,subprocess
from pathlib import Path
from casc_gather_depth_qualification import produce as casc, checked
from prepare_casc_gather_depth import sha,module
from public_depth_sampled_oracle import cases
KIND='public-nvn-depth-sampled-qualification-v1'
OWN=('src/nvn_pipeline_draw_record.inc','tools/public_depth_sampled_qualification.py','tools/public_depth_sampled_oracle.py','tools/public_depth_sampled_validation.py','tools/public_depth_sampled_runner.py','tools/check_public_depth_sampled_probe.py','tests/public_depth_sampled_probe.c','tests/public_depth_sampled_layout.c','tests/d2r_bridge/public_depth_sampled_failure.c','tests/test_public_depth_sampled_validation.py')
def produce(root,pipeline,fdl,depth,a2d,runtime,block,out,sample_pair):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 out.mkdir(parents=True);own=Path(__file__).resolve().parents[1]
 cascpath=casc(root,pipeline,fdl,depth,a2d,runtime,block,out/'casc-original-prerequisite');inputs={};base=checked(cascpath,inputs);p=json.loads(pipeline.read_text());stage=json.loads(Path(p['stage_check']).read_text());stages=list(stage['stages'].values())
 identities=[('c1906ac598c15e11a3029bff6db444f45b4343ec56ffb15624eff0be5f7d7369','96757bb36a024b91e6f161b424af3cc3db2f035f059842e5151ba7fb1ff89cc4'),('a98bd42200fa86b7c94c19ecb528449043c6fb71c235dafbf46ce20f7c8fdbda','a7672f1d3bfeb7d0312812c8e98eea92565d5934bad06bbb5f345e5a684f92e5')]
 if type(sample_pair)is not int or not 0<=sample_pair<len(p['pairs']):raise ValueError('explicit normal SAMPLE pair index required')
 sample_index=sample_pair;sample=p['pairs'][sample_pair]
 for stage_index,(code,control)in zip((sample['vs_index'],sample['fs_index']),identities):
  entry=stages[stage_index]['envelope']['inputs']
  if entry['code']['sha256']!=code or entry['control']['sha256']!=control or entry['control']['size']!=2176 or entry['c1']['size']!=0:raise ValueError('explicit original SAMPLE identity differs')
 types=sample['native_texture_types']
 if types['dimensions']!=[0,2]+[0]*14 or types['operations']!=[0,1]+[0]*14:raise ValueError('actual normal SAMPLE2D facts required')
 nir=pipeline.parent/('pair-'+str(sample_index)+'.fs.nir');text=nir.read_text()
 if text.count(' = fabs ')<3 or ' = flog2 'not in text or '@terminate'not in text:raise ValueError('actual SAMPLE algebra evidence required')
 inputs[str(nir)]=sha(nir)
 case_header=out/'public-depth-sampled-cases.h';case_header.write_text('#pragma once\nstruct nn_public_depth_sampled_case {unsigned role,format,width,height,origin,wrap,mip,compare;};\nstatic const struct nn_public_depth_sampled_case NN_PUBLIC_DEPTH_SAMPLED_CASES[]={\n'+',\n'.join('{'+','.join(str(c[k])for k in('role','format','width','height','origin','wrap_index','selected_mip','compare'))+'}'for c in cases())+'\n};\n#define NN_PUBLIC_DEPTH_SAMPLED_CASE_COUNT 72u\n')
 header=out/'public-depth-sampled-qualification.h';header.write_text('#include '+json.dumps(base['header'])+'\n#include '+json.dumps(str(case_header))+'\n#define NN_PUBLIC_DEPTH_SAMPLE_PAIR '+str(sample_index)+'u\n#define NN_PUBLIC_DEPTH_GATHER_PAIR '+str(base['pair_index'])+'u\n#define NN_PUBLIC_DEPTH_SAMPLED_BLOCK_IMAGE NN_CASC_GATHER_DEPTH_BLOCK_IMAGE\n')
 exe=out/'canonical-layout';argv=['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(own/'include'),'-DNN_PUBLIC_DEPTH_SAMPLED_CASES_HEADER='+json.dumps(str(case_header)),str(own/'tests/public_depth_sampled_layout.c'),str(own/'src/texture_source.c'),'-Wl,--gc-sections','-o',str(exe)];r=subprocess.run(argv,capture_output=True,text=True)
 if r.returncode:raise ValueError(r.stderr)
 run=subprocess.run([exe],capture_output=True,text=True)
 if run.returncode:raise ValueError('canonical tool failed')
 canonical=json.loads(run.stdout)
 if len(canonical)!=72:raise ValueError('whole72 canonical layout required')
 (out/'canonical.json').write_text(json.dumps(canonical,indent=2)+'\n');(out/'commands.json').write_text(json.dumps([dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr)],indent=2)+'\n')
 for rel in OWN:inputs[str(own/rel)]=sha(own/rel)
 for rel in('src/texture_source.c','include/nvn_native/texture_source.h','tests/nvn_pipeline_probe.c','android/nvn_pipeline_probe_activity.c'):inputs[str(own/rel)]=sha(own/rel)
 c=dict(schema=1,kind=KIND,passed=True,gpu_execution=False,android_execution=False,producer_sha256=sha(__file__),normal_root=str(root),header=str(header),pipeline_check=str(pipeline),runtime_fdl_check=str(fdl),runtime_fdl_library=base['runtime_fdl_library'],native_libraries=base['native_libraries'],casc_source_check=base['source_check'],sample_pair_index=sample_index,gather_pair_index=base['pair_index'],cases=cases(),canonical=canonical,submissions=144,files=144,finishes=144,source_formats=[50,51,52],original_control_bytes=2176,original_C1_bytes=256,compare_sampler=False,source_swizzle='R001',scope='original public SAMPLE and GatherR+FragDepth with authored actual depth-source bindings; not D2R execution or original NVIDIA oracle',inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()})
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('normal source changed '+p)
 (out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'
def build(path,out,root=None):
 c=json.loads(path.read_text());locked={};checked(path,locked)
 if c.get('kind')!=KIND or c.get('producer_sha256')!=sha(__file__)or c.get('cases')!=cases()or c.get('submissions')!=144:raise ValueError('unknown/stale full144 public depth qualifier')
 for rel in OWN:
  p=Path(__file__).resolve().parents[1]/rel
  if c['inputs'].get(str(p))!=sha(p):raise ValueError('actual qualifier source changed '+rel)
 # Exact base producer verifies all normal compilation and original source/control/C1.
 base=path.parent/'casc-original-prerequisite/check.json'
 module(Path(__file__).with_name('casc_gather_depth_qualification.py'),'public_depth_base').build(base,out,root)
 return Path(c['header']),c,locked
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in('root','pipeline','fdl','depth','a2d','runtime','block','out'):p.add_argument('--'+k,type=Path,required=True)
 p.add_argument("--sample-pair",type=int,required=True)
 a=p.parse_args();print(produce(*[getattr(a,k).resolve()for k in('root','pipeline','fdl','depth','a2d','runtime','block','out')],a.sample_pair))

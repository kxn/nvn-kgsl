#!/usr/bin/env python3
"""Normal authored block-copy qualifier bundle from real compiler/source math."""
import argparse,hashlib,importlib.util,json,shutil,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p):
 s=importlib.util.spec_from_file_location('nn_block_closure',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def build(kernel,out,*,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();kernel=Path(kernel).resolve();out=Path(out).resolve()
 if out.exists() or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored block bundle required')
 c=json.loads(kernel.read_text());inputs={str(kernel):sha(kernel),str(Path(__file__).resolve()):sha(__file__)}
 if c.get('schema')!=1 or c.get('kind')!='native-general-block-copy-v1' or c.get('gpu_execution')is not False or c.get('runtime_compiler')is not False or c.get('environment')!={'IR3_SHADER_DEBUG':'asmroundtrip,nopreamble'}:raise ValueError('unknown normal block kernel')
 for section in ['inputs','artifacts']:
  for p,h in c[section].items():
   if sha(p)!=h:raise ValueError('block kernel source/artifact changed '+p)
   inputs[p]=h
 for rel in ['shaders/block_copy.comp','tools/compile_block_copy.py','tools/build_ir3.py','shader/compute_artifact.cc']:
  p=root/'nvn-native'/rel
  if not any(old.endswith('/nvn-native/'+rel) and h==sha(p) for old,h in c['inputs'].items()):
   # The overlay is recorded under the isolated compiler source name.
   if rel!='shader/compute_artifact.cc' or c['compiler_build']['overlay'].get('src/freedreno/ir3/nvn_compute_artifact.cc')!=sha(p):raise ValueError('current normal compiler/kernel source differs '+rel)
 records=[Path(p) for p in c['inputs']if Path(p).name=='build-record.json' and json.loads(Path(p).read_text())==c['compiler_build']]
 origins=[Path(p).parents[2] for p in c['inputs']if p.endswith('/nvn-native/tools/build_ir3.py')and sha(p)==c['compiler_build']['builder_sha256']]
 if len(records)!=1 or len(origins)!=1:raise ValueError('ambiguous normal compiler')
 actual,closure,compiler,count=module(root/'nvn-native/tools/compute_driverparams.py').compiler_inputs(origins[0],records[0].parent)
 if actual!=c['compiler_build']or count!=c['compiler_source_files']or any(c['inputs'].get(p)!=v for p,v in closure.items()):raise ValueError('normal block compiler closure changed')
 header=Path(c['header']);prefix=header.with_suffix('').with_suffix('')
 if str(header)not in c['artifacts']or json.loads(prefix.with_suffix('.json').read_text())!=c['program']or json.loads(prefix.with_suffix('.native.json').read_text())!=c['native']:raise ValueError('block kernel header/metadata outside artifact closure')
 expected={'chip_id':0x06050002,'local_size':[8,8,1],'ssbo_table_slots':2,'ubo_table_slots':3,'num_uavs':2,'num_samplers':0,'private_bytes':0,'shared_bytes':0,'early_preamble':False,'constant_data_bytes':0}
 if any(c['program'].get(k)!=v for k,v in expected.items())or c['native'].get('descriptor_fdl_checks')!=20:raise ValueError('block kernel resource contract')
 ranges=c['program'].get('ubo_push_ranges',[])
 if len(ranges)!=1 or any(ranges[0].get(k)!=v for k,v in {'block':2,'start_bytes':0,'end_bytes':64,'bindless':False,'global':False}.items()):raise ValueError('block kernel UBO2 range')
 out.mkdir(parents=True);native=root/'nvn-native';runs=[]
 def run(argv):
  argv=list(map(str,argv));p=subprocess.run(argv,capture_output=True,text=True);runs.append(dict(argv=argv,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if p.returncode:raise ValueError('metadata command failed '+p.stderr)
  return p.stdout
 sources=[native/'tests/block_copy_probe_layout.c',native/'src/block_copy.c',native/'src/texture_source.c'];utility=out/'block-layout';cc=Path(shutil.which('cc')).resolve()
 run([cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(native/'include'),*sources,'-Wl,--gc-sections','-o',utility]);cases=json.loads(run([utility]))
 from block_copy_validation import validate_cases
 validate_cases(cases)
 for p in [cc,*sources,native/'tests/block_copy_cases.h',native/'tests/block_copy_probe_cases.h',native/'include/nvn_native/block_copy.h',native/'include/nvn_native/texture_source.h',native/'include/nvn_native/compute.h',native/'include/nvn_native/compute_image.h',native/'include/nvn_native/kgsl.h',native/'tools/compute_driverparams.py',native/'tools/block_copy_validation.py']:inputs[str(p)]=sha(p)
 target=out/'block_copy.native.h';target.write_bytes(header.read_bytes());inputs[str(target)]=sha(target)
 layouts=out/'cases.json';layouts.write_text(json.dumps(cases,indent=2)+'\n');inputs[str(layouts)]=sha(layouts)
 result=dict(schema=1,kind='native-block-copy-qualification-v1',gpu_execution=False,android_execution=False,cases=cases,kernel_check=str(kernel),kernel_manifest=c,code_bo_bytes=(c['program']['binary_bytes']+4095)//4096*4096,inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()})
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('block bundle input changed '+p)
 check=out/'check.json';check.write_text(json.dumps(result,indent=2)+'\n');inputs[str(check)]=sha(check)
 return target,result,inputs
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);p.add_argument('--kernel-check',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();print(build(a.kernel_check,a.out,root=a.root)[0])

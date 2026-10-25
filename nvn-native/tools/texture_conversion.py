#!/usr/bin/env python3
"""Package authored native layout kernels and actual FDL metadata; no GPU run.

Reuses the frozen compiler/check and public FDL build, never a new compiler or
CPU image converter. Source payload extent is not a public NVN storage getter.
"""
import argparse,hashlib,importlib.util,json,shlex,shutil,subprocess,sys
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(path,name):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def kernel_check(root,path):
 c=json.loads(path.read_text());inputs={str(path):sha(path)}
 if c.get('schema')!=1 or c.get('kind')!='native-texture-layout-copy-v1' or c.get('gpu_execution') is not False or c.get('runtime_compiler') is not False or c.get('environment')!={'IR3_SHADER_DEBUG':'asmroundtrip,nopreamble'}:raise ValueError('unknown authored kernel check')
 for mapping in (c['inputs'],c['artifacts']):
  for p,h in mapping.items():
   if sha(p)!=h:raise ValueError('kernel source/product changed: '+p)
   inputs[p]=h
 for name in ('gob_copy','linear_copy'):
  canonical=root/'nvn-native/shaders'/(name+'.comp')
  if not any(p.endswith('/nvn-native/shaders/'+name+'.comp') and h==sha(canonical) for p,h in c['inputs'].items()):raise ValueError('kernel was not built from current normal source bytes: '+name)
 producer=root/'nvn-native/tools/compile_texture_copy.py'
 if not any(p.endswith('/nvn-native/tools/compile_texture_copy.py') and h==sha(producer) for p,h in c['inputs'].items()):raise ValueError('kernel was not built by current normal producer bytes')
 records=[Path(p) for p in c['inputs'] if Path(p).name=='build-record.json' and json.loads(Path(p).read_text())==c['compiler_build']]
 if len(records)!=1:raise ValueError('ambiguous real compiler build')
 common=module(root/'nvn-native/tools/compute_driverparams.py','nn_conversion_compiler_closure')
 origins=[Path(p).parents[2] for p in c['inputs'] if p.endswith('/nvn-native/tools/build_ir3.py') and sha(p)==c['compiler_build']['builder_sha256']]
 if len(origins)!=1:raise ValueError('ambiguous original compiler source root')
 if sha(root/'nvn-native/tools/build_ir3.py')!=sha(origins[0]/'nvn-native/tools/build_ir3.py'):raise ValueError('current compiler source differs')
 record,actual,compiler,count=common.compiler_inputs(origins[0],records[0].parent)
 if record!=c['compiler_build'] or count!=c['compiler_source_files'] or any(c['inputs'].get(p)!=v for p,v in actual.items()):raise ValueError('kernel compiler closure changed')
 expected={'chip_id':0x06050002,'local_size':[8,8,1],'ssbo_table_slots':2,'ubo_table_slots':3,'num_uavs':2,'num_samplers':0,'private_bytes':0,'shared_bytes':0,'early_preamble':False,'constant_data_bytes':0,'immediate_dwords':0}
 if len(c.get('cases',[]))!=2 or {v['name'] for v in c['cases']}!={'gob_copy','linear_copy'}:raise ValueError('unknown kernel pair')
 for entry in c['cases']:
  p=entry['program'];n=entry['native'];header=Path(entry['header']);prefix=header.with_suffix('').with_suffix('')
  if any(p.get(k)!=v for k,v in expected.items()) or n.get('schema')!=1 or n.get('descriptor_fdl_checks')!=20:raise ValueError('native kernel reflection mismatch')
  ranges=p.get('ubo_push_ranges',[])
  if len(ranges)!=1 or ranges[0].get('block')!=2 or ranges[0].get('start_bytes')!=0 or ranges[0].get('end_bytes')!=64 or ranges[0].get('bindless') is not False or ranges[0].get('global') is not False:raise ValueError('kernel needs bounded64B UBO2')
  if json.loads(prefix.with_suffix('.json').read_text())!=p or json.loads(prefix.with_suffix('.native.json').read_text())!=n or str(header) not in c['artifacts']:raise ValueError('kernel header/metadata not in exact artifact closure')
 return c,inputs,records[0].parent

def specifications():
 formats=((1,1,1),(6,2,1),(0x25,4,1),(0x2a,8,1),(0x2e,16,1),(0x41,8,4),(0x44,16,4));out=[]
 for fmt,cpp,block in formats:
  width={1:31,2:17,4:19,8:9,16:5}[cpp] if block==1 else 67
  for rows in (8,11,17,128):out.append(dict(flags=0,target=1,format=fmt,width=width,height=rows*block,levels=1,layers=1,stride=0,block_height_log2=-1))
 for fmt,cpp,block in formats[:5]:
  width={1:31,2:17,4:19,8:9,16:5}[cpp]
  for i,rows in enumerate((8,11,17,128)):
   align=32 if i%2==0 else 128;out.append(dict(flags=0x10 if align==32 else 0x40,target=1,format=fmt,width=width,height=rows,levels=1,layers=1,stride=(width*cpp+align-1)//align*align,block_height_log2=-1))
 for fmt,cpp,block in formats:out.append(dict(flags=0,target=4,format=fmt,width=65 if block==1 else 133,height=17*block,levels=5,layers=3,stride=0,block_height_log2=-1))
 return out

def produce(root,kernels,out):
 if out.exists() or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output below ROOT/.local required')
 c,inputs,ir3=kernel_check(root,kernels);out.mkdir(parents=True);native=root/'nvn-native';build=ir3/'build';runs=[]
 for p in (Path(__file__).resolve(),native/'tests/texture_conversion_layout.c',native/'tests/texture_conversion_cases.h',native/'include/nvn_native/texture_source.h',native/'src/texture_source.c',build/'compile_commands.json'):inputs[str(p)]=sha(p)
 def run(cmd,cwd=None):
  q=subprocess.run(list(map(str,cmd)),cwd=cwd,capture_output=True,text=True);runs.append({'argv':list(map(str,cmd)),'cwd':str(cwd) if cwd else None,'returncode':q.returncode,'stdout':q.stdout,'stderr':q.stderr})
  (out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if q.returncode:raise RuntimeError(q.stdout+q.stderr)
  return q.stdout
 # Use exact normal Mesa compilation flags and link libraries for its own FDL
 # test, replacing only test objects with this metadata-only caller/calculator.
 entry=next(e for e in json.loads((build/'compile_commands.json').read_text()) if e['file'].endswith('/fd6_layout.c'))
 args=shlex.split(entry['command']);flags=[];skip=False
 for i,v in enumerate(args[1:]):
  if skip:skip=False;continue
  if v in ('-o','-MQ','-MF'):skip=True;continue
  if v in ('-MD','-c') or v.endswith('/fd6_layout.c'):continue
  flags.append(v)
 objects=[]
 for source in (native/'tests/texture_conversion_layout.c',native/'src/texture_source.c'):
  obj=out/(source.stem+'.o');run([args[0],*flags,'-I'+str(native/'include'),'-c',source,'-o',obj],build);objects.append(obj)
 ninja=next(Path(p) for p in c['compiler_build']['tools'] if Path(p).name=='ninja');link=shlex.split(run([ninja,'-C',build,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
 for v in link[1:]:
  if skip:skip=False;continue
  if v=='-o':skip=True;continue
  if v.endswith('.o'):continue
  libs.append(v)
 utility=out/'fdl-layout';run([link[0],'-o',utility,*objects,*libs],build)
 for executable in (args[0],link[0]):
  p=Path(shutil.which(executable)).resolve();inputs[str(p)]=sha(p)
 for v in libs:
  p=(build/v).resolve()
  if p.is_file():inputs[str(p)]=sha(p)
 cases=[]
 for index,desc in enumerate(specifications()):
  actual=json.loads(run([utility,*[desc[k] for k in ('flags','target','format','width','height','levels','layers','stride','block_height_log2')]]));actual['id']=index;actual['desc']=desc;cases.append(actual)
 (out/'layouts.json').write_text(json.dumps(cases,indent=2)+'\n')
 artifacts={str(p):sha(p) for p in (out/'layouts.json',utility,out/'commands.json')}
 check={'schema':1,'kind':'native-texture-conversion-batch-v1','scope':'HOST FDL/source geometry and compiled authored kernels only; no GPU/NVN/game qualification','gpu_execution':False,'kernel_check':str(kernels),'kernel_check_sha256':sha(kernels),'kernel_manifest':c,'cases':cases,'compiler_build_path':str(ir3/'build-record.json'),'inputs':inputs,'producer_sha256':sha(__file__),'artifacts':artifacts,'layouts_path':str(out/'layouts.json'),'fdl_utility_sha256':sha(utility),'command_record_sha256':sha(out/'commands.json')}
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('source/tool changed during metadata production: '+p)
 (out/'check.json').write_text(json.dumps(check,indent=2)+'\n');return out/'check.json'

def build(check_path,out,*,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();check_path=Path(check_path).resolve();c=json.loads(check_path.read_text())
 if c.get('schema')!=1 or c.get('kind')!='native-texture-conversion-batch-v1' or c.get('gpu_execution') is not False or c.get('producer_sha256')!=sha(__file__):raise ValueError('unknown native conversion batch')
 inputs={str(check_path):sha(check_path),str(Path(__file__).resolve()):sha(__file__)}
 for p,h in {**c['inputs'],**c['artifacts']}.items():
  if sha(p)!=h:raise ValueError('conversion source/tool changed: '+p)
  inputs[p]=h
 k,closure,_=kernel_check(root,Path(c['kernel_check']))
 if k!=c['kernel_manifest'] or sha(c['kernel_check'])!=c['kernel_check_sha256']:raise ValueError('conversion kernel check changed')
 inputs.update(closure)
 if json.loads(Path(c['layouts_path']).read_text())!=c['cases']:raise ValueError('layout output differs from checked artifact')
 for name in ('tests/texture_conversion_layout.c','tests/texture_conversion_cases.h','include/nvn_native/texture_source.h','src/texture_source.c'):
  if not any(p.endswith('/nvn-native/'+name) and h==sha(root/'nvn-native'/name) for p,h in c['inputs'].items()):raise ValueError('current conversion source differs: '+name)
 if [v['desc'] for v in c['cases']]!=specifications():raise ValueError('unknown conversion qualification matrix')
 out.mkdir(parents=True,exist_ok=True);lines=['/* Generated checked authored kernels/FDL metadata; qualification only. */','#include "texture_conversion_cases.h"']
 for entry in k['cases']:
  name=entry['name'];dest=out/(name+'.native.h');dest.write_bytes(Path(entry['header']).read_bytes());inputs[str(dest)]=sha(dest)
  lines += ['#include "'+dest.name+'"','static const struct nn_cs_image *const nn_tc_'+name+'=&NVN_COMPILED_IMAGE;','#undef NVN_COMPILED_IMAGE']
 lines.append('static const struct nn_tc_case nn_tc_cases[]={')
 fields=('source_offset','source_bytes','row_bytes','rows','source_pitch','block_height_log2','native_offset','native_pitch','native_layer_stride','native_bytes')
 for item in c['cases']:
  d=item['desc'];lines.append('{.desc={'+','.join('.'+k+'='+str(v) for k,v in d.items())+'},'+','.join('.'+k+'='+str(item[k]) for k in ('source_bytes','source_layer_stride','native_bytes','linear','levels','layers'))+',.level={'+','.join('{'+','.join('.'+k+'='+str(level[k]) for k in fields)+'}' for level in item['level'])+'}},')
 lines+=['};','#define NN_TC_CASE_COUNT (sizeof(nn_tc_cases)/sizeof(nn_tc_cases[0]))'];header=out/'texture_conversion_bundle.h';header.write_text('\n'.join(lines)+'\n');inputs[str(header)]=sha(header)
 return header,c,inputs

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--kernels-check',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();print(produce(a.root.resolve(),a.kernels_check.resolve(),a.out.resolve()))
if __name__=='__main__':main()

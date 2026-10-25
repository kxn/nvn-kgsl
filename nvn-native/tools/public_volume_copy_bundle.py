#!/usr/bin/env python3
"""Normal public true3D copy catalogue and checked native volume kernel bundle."""
import argparse,hashlib,importlib.util,json,shutil,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def build(kernel,out,*,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();kernel=Path(kernel).resolve();out=Path(out).resolve();n=root/'nvn-native'
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored public volume bundle required')
 c=json.loads(kernel.read_text());inputs={str(kernel):sha(kernel),str(Path(__file__).resolve()):sha(__file__)}
 origins=[Path(p).parents[2]for p in c.get('inputs',{})if p.endswith('/nvn-native/tools/compile_volume_copy.py')]
 if len(origins)!=1:raise ValueError('ambiguous normal volume producer')
 module(n/'tools/compile_volume_copy.py','normal_volume_producer').verify(origins[0],kernel)
 for rel in ['tools/compile_volume_copy.py','shaders/volume_copy.comp']:
  if not any(p.endswith('/nvn-native/'+rel)and h==sha(n/rel)for p,h in c['inputs'].items()):raise ValueError('current volume source differs '+rel)
 for name,h in {**c['inputs'],**c['artifacts']}.items():
  if sha(name)!=h:raise ValueError('normal volume input/artifact changed '+name)
  inputs[name]=h
 records=[Path(p)for p in c['inputs']if Path(p).name=='build-record.json'and json.loads(Path(p).read_text())==c['compiler_build']]
 compiler_roots=[Path(p).parents[2]for p in c['inputs']if p.endswith('/nvn-native/tools/build_ir3.py')and sha(p)==c['compiler_build']['builder_sha256']]
 if len(records)!=1 or len(compiler_roots)!=1:raise ValueError('ambiguous normal volume compiler')
 actual,closure,compiler,count=module(n/'tools/compute_driverparams.py','volume_compiler_closure').compiler_inputs(compiler_roots[0],records[0].parent)
 if actual!=c['compiler_build']or count!=c['compiler_source_files']or any(c['inputs'].get(p)!=h for p,h in closure.items()):raise ValueError('normal volume compiler closure changed')
 program=c['program'];native=c['native'];expected=dict(chip_id=0x06050002,local_size=[8,8,1],ssbo_table_slots=2,ubo_table_slots=3,num_uavs=2,num_samplers=0,private_bytes=0,shared_bytes=0,early_preamble=False,constant_data_bytes=0)
 ranges=program.get('ubo_push_ranges',[])
 if any(program.get(k)!=v for k,v in expected.items())or len(ranges)!=1 or any(ranges[0].get(k)!=v for k,v in {'block':2,'start_bytes':0,'end_bytes':64,'bindless':False,'global':False}.items())or native.get('schema')!=1 or native.get('descriptor_fdl_checks')!=20:raise ValueError('actual volume kernel resource contract')
 source=Path(c['header']);prefix=source.with_suffix('').with_suffix('')
 if str(source)not in c['artifacts']or json.loads(prefix.with_suffix('.json').read_text())!=program or json.loads(prefix.with_suffix('.native.json').read_text())!=native:raise ValueError('volume header/metadata identity')
 out.mkdir(parents=True);runs=[]
 def run(argv):
  argv=list(map(str,argv));p=subprocess.run(argv,text=True,capture_output=True);runs.append(dict(argv=argv,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if p.returncode:raise ValueError('volume metadata '+p.stderr)
  return p.stdout
 cc=Path(shutil.which('cc')).resolve();sources=[n/'tests/public_volume_copy_layout.c',n/'src/texture_source.c'];utility=out/'public-volume-layout'
 run([cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),*sources,'-Wl,--gc-sections','-o',utility]);cases=json.loads(run([utility]))
 from public_volume_copy_validation import validate_cases
 validate_cases(cases);target=out/'volume_copy.native.h';target.write_bytes(source.read_bytes());lines=['/* Authored true3D catalogue; original normal kernel, no shader edits. */','#define NN_PUBLIC_COPY_CASES 228u','#define NN_PUBLIC_COPY_OPERATION "public_volume_copy"','#define NN_PUBLIC_COPY_PREFIX "public-volume-copy"','#define NN_PUBLIC_COPY_REGISTER(d) nn_nvn_register_volume_copy((d),&NVN_COMPILED_IMAGE)','#include "public_copy_cases.h"','#include "nvn_native/compute_image.h"','#include "volume_copy.native.h"','static const struct nn_public_copy_case nn_public_copy_cases[]={']
 for a in cases:
  fields=['id','family','variant','target','format','width','height','layers','levels','flags','shared','level','base_layer','layer_count','x_bytes','y_rows','row_bytes','rows','row_stride','image_stride','canonical_offset','destination_offset','linear_offset','linear_bytes']
  row='{'+','.join('.'+k+'='+str(a[k])for k in fields)+',.pool_bytes={'+','.join(map(str,a['pool_bytes']))+'},.region={'+','.join('{'+','.join(map(str,z))+'}'for z in a['region'])+'},.source={.schema=1,.flags='+str(a['flags'])+',.target=2,.format='+str(a['format'])+',.linear=0,.block_w='+str(4 if 0x41<=a['format']<=0x50 else 1)+',.block_h='+str(4 if 0x41<=a['format']<=0x50 else 1)+',.bytes_per_block='+str(a['mips'][0][6]//a['mips'][0][4])+',.levels='+str(a['levels'])+',.layers=1,.base_block_height_log2=0,.gob_bytes=512,.public_extent_known=0,.alignment=0,.layer_stride='+str(a['layer_stride'])+',.payload_bytes='+str(a['payload_bytes'])+',.volume_depth='+str(a['depth'])+',.base_block_depth_log2='+str(a['base_bd'])+',.level_depths={'+','.join(map(str,a['mip_depths']))+'},.level_block_depths={'+','.join(map(str,a['mip_bds']))+'},.level={'
  names=['offset','bytes','width','height','blocks_x','blocks_y','row_bytes','pitch','block_height_log2'];row+=','.join('{'+','.join('.'+k+'='+str(v)for k,v in zip(names,m))+'}'for m in a['mips'])+'}}},';lines.append(row)
 lines+=['};'];header=out/'public_volume_copy_bundle.h';header.write_text('\n'.join(lines)+'\n');layouts=out/'cases.json';layouts.write_text(json.dumps(cases,indent=2)+'\n')
 for q in [cc,*sources,n/'src/texture_volume.inc',n/'include/nvn_native/texture_source.h',n/'include/nvn_native/texture_volume.h',n/'tests/public_copy_cases.h',n/'tests/block_copy_cases.h',n/'tools/compute_driverparams.py',n/'tools/public_volume_copy_validation.py',n/'tools/block_copy_validation.py',header,target,layouts]:inputs[str(q)]=sha(q)
 for q,h in inputs.items():
  if sha(q)!=h:raise ValueError('public volume input changed '+q)
 result=dict(schema=1,kind='public-nvn-volume-copy-qualification-v1',gpu_execution=False,android_execution=False,true3D=True,cases=cases,kernel_check=str(kernel),kernel_manifest=c,inputs=inputs,artifacts={str(q):sha(q)for q in out.iterdir()if q.is_file()});check=out/'check.json';check.write_text(json.dumps(result,indent=2)+'\n');inputs[str(check)]=sha(check);return header,result,inputs
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);p.add_argument('--kernel-check',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();print(build(a.kernel_check,a.out,root=a.root)[0])

#!/usr/bin/env python3
"""Normal public NVN authored copy qualifier metadata/kernel bundle, no GPU."""
import argparse,hashlib,importlib.util,json,shutil,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p,name):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def build(kernel,out,*,root=None,packaged=False):
 root=(root or Path(__file__).resolve().parents[2]).resolve();out=Path(out).resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored public-copy bundle required')
 out.mkdir(parents=True);n=root/'nvn-native';native,baseline,inputs=module(n/'tools/block_copy_bundle.py','native_block_bundle').build(kernel,out/'native',root=root);runs=[]
 def run(argv):
  argv=list(map(str,argv));p=subprocess.run(argv,capture_output=True,text=True);runs.append(dict(argv=argv,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if p.returncode:raise ValueError('public metadata command '+p.stderr)
  return p.stdout
 sources=[n/('tests/public_packaged_copy_layout.c'if packaged else'tests/public_copy_layout.c'),n/'src/block_copy.c',n/'src/texture_source.c'];cc=Path(shutil.which('cc')).resolve();utility=out/'public-copy-layout';run([cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),*sources,'-Wl,--gc-sections','-o',utility]);cases=json.loads(run([utility]));from public_copy_validation import validate_cases
 if packaged:
  from public_packaged_copy_validation import validate_cases
 validate_cases(cases);lines=['/* Normal authored public-copy inputs/independent expected metadata. */','#include "public_copy_cases.h"','#include "native/block_copy.native.h"','static const struct nn_public_copy_case nn_public_copy_cases[]={']
 if packaged:lines[1:1]=['#define NN_PUBLIC_COPY_CASES 72u','#define NN_PUBLIC_COPY_FORMATS_JSON "2"','#define NN_PUBLIC_COPY_PACKAGED_BNTX 1','#define NN_PUBLIC_COPY_OPERATION "public_packaged_bntx_copy"','#define NN_PUBLIC_COPY_PREFIX "public-copy"','#define NN_PUBLIC_COPY_REGISTER(d) nn_nvn_register_block_copy((d),&NVN_COMPILED_IMAGE)','#define NN_PUBLIC_COPY_EXPERIMENT_JSON '+json.dumps('"packaged_experiment":true,"NVN_encoding_normative_proof":false,')]
 for c in cases:
  fields=['id','family','variant','target','format','width','height','layers','levels','flags','shared','level','base_layer','layer_count','x_bytes','y_rows','row_bytes','rows','row_stride','image_stride','canonical_offset','destination_offset','linear_offset','linear_bytes']
  row='{'+','.join('.'+k+'='+str(c[k])for k in fields)+',.pool_bytes={'+','.join(map(str,c['pool_bytes']))+'},.region={'+','.join('{'+','.join(map(str,x))+'}'for x in c['region'])+'},.source={.schema=1,.flags=0,.target='+str(c['target'])+',.format='+str(c['format'])+',.linear=0,.block_w='+str(4 if 0x41<=c['format']<=0x50 else 1)+',.block_h='+str(4 if 0x41<=c['format']<=0x50 else 1)+',.bytes_per_block='+str(c['mips'][0][6]//c['mips'][0][4])+',.levels='+str(c['levels'])+',.layers='+str(c['layers'])+',.base_block_height_log2='+str(c['base_bh'])+',.gob_bytes=512,.public_extent_known=0,.alignment=0,.layer_stride='+str(c['layer_stride'])+',.payload_bytes='+str(c['payload_bytes'])+',.level={'
  names=['offset','bytes','width','height','blocks_x','blocks_y','row_bytes','pitch','block_height_log2'];row+=','.join('{'+','.join('.'+k+'='+str(v)for k,v in zip(names,m))+'}'for m in c['mips'])+'}}},';lines.append(row)
 lines+=['};'];header=out/'public_copy_bundle.h';header.write_text('\n'.join(lines)+'\n');layouts=out/'cases.json';layouts.write_text(json.dumps(cases,indent=2)+'\n')
 for p in [Path(__file__).resolve(),*([n/'tools/public_packaged_copy_bundle.py',n/'tools/public_packaged_copy_validation.py',n/'src/packaged_bntx.c',n/'include/nvn_native/packaged_bntx.h']if packaged else[]),n/'tools/block_copy_bundle.py',n/'tools/public_copy_validation.py',n/'tools/block_copy_validation.py',*sources,n/'tests/public_copy_cases.h',n/'tests/block_copy_probe_cases.h',n/'tests/block_copy_cases.h',n/'third_party/nvn/nvn.h',cc,header,layouts]:inputs[str(p)]=sha(p)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('public copy bundle input changed '+p)
 result=dict(schema=1,kind='public-nvn-packaged-bntx-copy-experiment-v1'if packaged else'public-nvn-canonical-copy-qualification-v1',gpu_execution=False,android_execution=False,cases=cases,kernel_check=str(Path(kernel).resolve()),native_baseline=baseline,inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});check=out/'check.json';check.write_text(json.dumps(result,indent=2)+'\n');inputs[str(check)]=sha(check);return header,result,inputs
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',required=True,type=Path);p.add_argument('--kernel-check',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();print(build(a.kernel_check,a.out,root=a.root)[0])

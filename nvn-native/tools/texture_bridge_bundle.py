#!/usr/bin/env python3
"""Package checked native GOB kernel and actual FDL clear extent; no GPU run.

Uses the normal texture-copy compiler closure and existing FDL metadata caller.
No game/compiler identity branch, runtime compiler, CPU converter or SDK layout
substitution. The bundle lives through Device and consumers keep private BOs.
"""
import argparse,hashlib,importlib.util,json,subprocess,shlex,shutil
from pathlib import Path

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(path,name):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

def native_views(kernels,ir3,out,shapes):
 build=ir3/'build';entry=next(x for x in json.loads((build/'compile_commands.json').read_text()) if x['file'].endswith('/fd6_view.cc'))
 args=shlex.split(entry['command']);flags=[];skip=False
 for v in args[1:]:
  if skip:skip=False;continue
  if v in ('-o','-MQ','-MF'):skip=True;continue
  if v in ('-MD','-c') or v.endswith('/fd6_view.cc'):continue
  flags.append(v)
 source=Path(__file__).resolve().parents[1]/'tests/texture_native_view.cc';runs=[]
 initial={str(p):sha(p) for p in (source,build/'compile_commands.json')}
 def run(cmd):
  p=subprocess.run(list(map(str,cmd)),cwd=build,text=True,capture_output=True);runs.append(dict(argv=list(map(str,cmd)),cwd=str(build),returncode=p.returncode,stdout=p.stdout,stderr=p.stderr))
  (out/'native-view-commands.json').write_text(json.dumps(runs,indent=2)+'\n')
  if p.returncode:raise ValueError(p.stderr)
  return p.stdout
 obj=out/'native-view.o';run([args[0],*flags,'-Werror','-c',source,'-o',obj])
 ninja=next(x for x in kernels['compiler_build']['tools'] if Path(x).name=='ninja')
 link=shlex.split(run([ninja,'-C',build,'-t','commands','src/freedreno/fdl/fd6_layout']).strip().splitlines()[-1]);libs=[];skip=False
 for v in link[1:]:
  if skip:skip=False;continue
  if v=='-o':skip=True;continue
  if v.endswith('.o'):continue
  libs.append(v)
 exe=out/'fdl-native-view';run([link[0],'-o',exe,obj,*libs]);rows=[]
 for shape in shapes:
  row=json.loads(run([exe,*shape]))
  if row['schema']!=1 or row['chip']!=0x06050002 or row['format']!=37 or row['target']!=1 or row['levels']!=1 or row['layers']!=1 or row['samples']!=1 or row['tile_mode'] or row['ubwc'] or row['descriptor_offset'] or row['descriptor_address_word']!=4 or len(row['texture_descriptor'])!=16:raise ValueError('native view metadata unsupported')
  field=row['texture_descriptor'][4]|row['texture_descriptor'][5]<<32
  if field&row['descriptor_address_mask']:raise ValueError('FDL view must have zero GPUVA')
  rows.append(row)
 inputs=dict(initial)
 for v in libs:
  p=(build/v).resolve()
  if p.is_file():inputs[str(p)]=sha(p)
 for cc in (args[0],link[0]):
  p=Path(shutil.which(cc)).resolve();inputs[str(p)]=sha(p)
 if any(sha(p)!=h for p,h in inputs.items()):raise ValueError('view source/tool changed during generation')
 return rows,inputs,exe

def produce(root,kernel_check,out,extra_views=()):
 if out.exists() or not out.is_relative_to(root/'.local'):raise ValueError('fresh directory below ROOT/.local required')
 tool_hash=sha(__file__)
 common=load(root/'nvn-native/tools/texture_conversion.py','nn_bridge_normal_conversion')
 # Existing normal producer compiles the actual FDL utility with original Mesa
 # build flags/libraries. Its fixed qualification cases are metadata only.
 metadata=common.produce(root,kernel_check,out/'fdl');record=json.loads(metadata.read_text())
 kernels,closure,ir3=common.kernel_check(root,kernel_check)
 runs=[];layouts=[]
 for dim in (64,2):
  args=[str(out/'fdl/fdl-layout'),'0','1','37',str(dim),str(dim),'1','1','0','-1']
  p=subprocess.run(args,capture_output=True,text=True);runs.append(dict(argv=args,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr))
  if p.returncode:raise ValueError('FDL bridge geometry rejected')
  row=json.loads(p.stdout);level=row['level'][0]
  if row['linear'] or row['levels']!=1 or row['layers']!=1 or row['cpp']!=4 or row['native_alignment']!=64 or level['native_offset']:
   raise ValueError('unexpected source/native FDL geometry')
  layouts.append(dict(width=dim,height=dim,pitch=level['native_pitch'],layer_bytes=level['native_layer_stride'],bytes=row['native_bytes'],alignment=row['native_alignment']))
 views,view_inputs,view_exe=native_views(kernels,ir3,out,[(64,64),(2,2),*extra_views])
 entry=next(c for c in kernels['cases'] if c['name']=='gob_copy');image=out/'gob_copy.native.h';image.write_bytes(Path(entry['header']).read_bytes())
 header=out/'texture_bridge_bundle.h';header.write_text('/* Generated actual normal kernel + FDL metadata. */\n#include "nvn_native/texture_bridge.h"\n#include "gob_copy.native.h"\nstatic const struct nn_tex_bridge_bundle NN_TEXTURE_BRIDGE_BUNDLE={.schema=1,.chip=0x06050002,.gob=&NVN_COMPILED_IMAGE,.clear_layout={'+','.join('.'+k+'='+str(v) for k,v in layouts[0].items())+'}};\n#undef NVN_COMPILED_IMAGE\n')
 view_header=out/'texture_native_views.h';lines=['/* Generated actual zero-GPUVA FDL views; see check.json. */','#include "nvn_native/texture_native_view.h"','static const struct nn_tex_native_view NN_TEXTURE_NATIVE_VIEWS[]={']
 for view in views:
  lines.append('{'+','.join('.'+key+'='+('{' + ','.join(map(str,value)) + '}' if isinstance(value,list) else str(int(value))) for key,value in view.items())+'},')
 lines+=['};','#define NN_TEXTURE_NATIVE_VIEW_COUNT (sizeof(NN_TEXTURE_NATIVE_VIEWS)/sizeof(NN_TEXTURE_NATIVE_VIEWS[0]))'];view_header.write_text('\n'.join(lines)+'\n')
 (out/'fdl-shapes.json').write_text(json.dumps(dict(layouts=layouts,views=views,runs=runs),indent=2)+'\n')
 inputs={**record['inputs'],**record['artifacts'],**closure,**view_inputs,str(metadata):sha(metadata),str(Path(__file__).resolve()):tool_hash}
 check=dict(schema=1,kind='native-texture-consumer-bridge-v1',scope='HOST actual FDL/kernel bundle only; no new GPU/NVN/game qualification',producer_sha256=tool_hash,metadata_source_sha256=sha(Path(__file__).resolve().parents[1]/'tests/texture_native_view.cc'),kernel_check=str(kernel_check),inputs=inputs,layouts=layouts,views=views,artifacts={str(p):sha(p) for p in (image,header,view_header,view_exe,out/'fdl-shapes.json',out/'native-view-commands.json')},header=str(header))
 if any(sha(p)!=v for p,v in inputs.items()):raise ValueError('input changed during production')
 (out/'check.json').write_text(json.dumps(check,indent=2)+'\n');return out/'check.json'

def build(root,check_path):
 c=json.loads(check_path.read_text())
 if c.get('schema')!=1 or c.get('kind')!='native-texture-consumer-bridge-v1' or c.get('producer_sha256')!=sha(__file__) or c.get('metadata_source_sha256')!=sha(Path(__file__).resolve().parents[1]/'tests/texture_native_view.cc'):raise ValueError('unknown/stale bridge producer or FDL caller')
 for p,h in {**c['inputs'],**c['artifacts']}.items():
  if sha(p)!=h:raise ValueError('bridge source/product changed: '+p)
 common=load(root/'nvn-native/tools/texture_conversion.py','nn_bridge_bundle_closure')
 common.kernel_check(root,Path(c['kernel_check']))
 return Path(c['header']),c

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--kernels-check',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--view',action='append',default=[],help='Explicit actual FDL WxH or WxH:pitch metadata');a=p.parse_args()
 shapes=[]
 for shape in a.view:
  wh,*pitch=shape.split(':');w,h=map(int,wh.lower().split('x'));shapes.append((w,h,*[int(p) for p in pitch]))
 print(produce(a.root.resolve(),a.kernels_check.resolve(),a.out.resolve(),shapes))
if __name__=='__main__':main()

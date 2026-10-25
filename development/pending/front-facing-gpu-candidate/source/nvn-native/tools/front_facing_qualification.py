#!/usr/bin/env python3
"""Normal actual NIR/IR3 FACE three-role GPU qualification inputs; no GPU."""
import argparse,importlib.util,json,os,re,struct,subprocess
from pathlib import Path
from package_graphics_pipelines import checked,compiler_record,sha
from texture_bridge_bundle import native_views
KIND='raw-native-front-facing-qualification-v1'
def module(p):
 s=importlib.util.spec_from_file_location('face_fdl',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def produce(root,compiler,tools,fdl_check,depth_check,a2d_check,runtime_check,out):
 n=Path(__file__).resolve().parents[1]
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 inputs={str(Path(__file__).resolve()):sha(__file__)};cc,platform,record=compiler_record(compiler,n,inputs)
 def dependency(path,kind):
  d=json.loads(path.read_text());inputs[str(path)]=sha(path)
  if d.get('kind')!=kind or d.get('passed')is not True or d.get('gpu_execution')is not False:raise ValueError('wrong normal helper '+str(path))
  checked(d['inputs'],{});checked(d['artifacts'],{});inputs.update(d['inputs']);inputs.update(d['artifacts']);return d
 depth=dependency(depth_check,'native-depth-fdl-foundation-v1');a2d=dependency(a2d_check,'native-depth-a2d-host-v1');runtime=dependency(runtime_check,'native-depth-zero-color-host-v1')
 for d in(depth,a2d,runtime):
  for rel in('src/depth_native.cc','src/depth_fdl.cc','src/depth_a2d.cc','src/depth_transfer.c'):
   old=[h for f,h in d['inputs'].items()if f.endswith('/nvn-native/'+rel)]
   if old and any(h!=sha(n/rel)for h in old):raise ValueError('native helper differs from checked source '+rel)
 f=json.loads(fdl_check.read_text());origins=[Path(v).parents[2]for v in f['inputs']if v.endswith('/nvn-native/src/runtime_fdl.cc')]
 if len(origins)!=1:raise ValueError('unique original FDL source root required')
 fdlroot=origins[0];fdltool=fdlroot/'nvn-native/tools/build_runtime_fdl.py';fdllib,fdl=module(fdltool).verify(fdlroot,fdl_check);inputs.update(fdl['inputs']);inputs.update(fdl['artifacts']);inputs[str(fdl_check)]=sha(fdl_check);inputs[str(fdltool)]=sha(fdltool)
 for rel in('tests/front_facing_probe.c','tests/pipeline_probe.c','tests/shaders/front_facing.vert','tests/shaders/front_facing.frag','tests/depth_probe_layout.cc','tools/front_facing_validation.py','tools/front_facing_runner.py','src/pipeline.c','src/render_state.c','src/pipeline_frontend.c','src/depth_transfer.c','include/nvn_native/pipeline_native_program.h','include/nvn_native/pipeline_texture_types.h'):
  inputs[str(n/rel)]=sha(n/rel)
 for t in('glslc','spirv-val','spirv-dis'):inputs[str(tools/t)]=sha(tools/t)
 out.mkdir(parents=True);commands=[]
 def run(a,cwd=None):
  a=list(map(str,a));q=subprocess.run(a,cwd=cwd,text=True,capture_output=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip'));commands.append(dict(argv=a,cwd=str(cwd)if cwd else None,returncode=q.returncode,stdout=q.stdout,stderr=q.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if q.returncode:raise ValueError(q.stdout+q.stderr)
  return q.stdout
 recipes=[];headers=['/* Authored FACE, FACE+FragCoord, FACE+FragDepth; never Maxwell/game. */','#pragma once','#include "nvn_native/pipeline_native_program.h"']
 for case in(0,1,3):
  vs=out/f'face{case}.vert.spv';fs=out/f'face{case}.frag.spv'
  for stage,dest in(('vert',vs),('frag',fs)):
   run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O','-DCASE='+str(case),n/'tests/shaders'/('front_facing.'+stage),'-o',dest]);run([tools/'spirv-val','--target-env','spv1.3',dest]);run([tools/'spirv-dis',dest,'-o',dest.with_suffix('.spvasm')])
  prefix=out/f'face{case}';run([cc,vs,fs,prefix,0,'0x06050002']);meta=json.loads(Path(str(prefix)+'.pipeline.json').read_text());types=json.loads(Path(str(prefix)+'.pipeline.types.json').read_text());face=meta.get('front_facing')
  if not face or face.get('half_scalar')is not True or face.get('merged_registers')is not True or face.get('need_size')is not True or face['max_separate_half_register']!=-1 or not 0<=face['regid']<192 or meta['texture_mask']or meta['semantic_sampler_mask']or types['texture_mask']or types['sampler_mask']or any(types['dimensions']+types['operations']):raise ValueError('actual native half FACE and no resources required')
  if bool(meta.get('depth_export'))!=(case==3)or meta['fragcoord_mask']!=(3 if case==1 else 0):raise ValueError('actual expected builtin roles required')
  text=Path(str(prefix)+'.pipeline.h').read_text();symbols=re.findall(r'static const (?:uint32_t|struct \w+)\s+(\w+)',text)
  for symbol in sorted(set(symbols),key=len,reverse=True):text=re.sub(r'\b'+symbol+r'\b',symbol+'_face'+str(case),text)
  headers.append(text)
  names=[]
  for stage,spv in(('vs',vs),('fs',fs)):
   raw=spv.read_bytes();name=f'face{case}_{stage}_spv';names.append(name);headers.append('static const uint32_t '+name+'[]={'+','.join(str(w)+'u'for w in struct.unpack('<'+'I'*(len(raw)//4),raw))+'};')
  recipe='&nn_pipeline_recipe_face'+str(case);v,g=names
  headers+=['static const struct nn_pipeline_texture_types face'+str(case)+'_types={.version=1,.recipe='+recipe+',.spirv={'+v+','+g+'},.spirv_bytes={sizeof('+v+'),sizeof('+g+')}};', 'static const struct nn_pipeline_native_artifact face'+str(case)+'_artifact={.version=1,.recipe='+recipe+',.spirv={'+v+','+g+'},.spirv_bytes={sizeof('+v+'),sizeof('+g+')},.textures=&face'+str(case)+'_types};']
  recipes.append(dict(case=case,prefix=str(prefix),metadata=meta,types=types,source='authored GLSL; no original code/control/C1 identity'))
 run([platform,'0x06050002',1179648,out/'device']);headers.append((out/'device.platform.h').read_text());headers.append('static const struct nn_pipeline_native_artifact *const NN_FACE_ARTIFACTS[]={&face0_artifact,&face1_artifact,&face3_artifact};')
 views,extra,_=native_views({'compiler_build':record},compiler,out,[(64,64),(73,41)]);inputs.update(extra);headers+=['#include "nvn_native/texture_native_view.h"','static const struct nn_tex_native_view NN_FACE_VIEWS[]={']
 for v in views:headers.append('{'+','.join('.'+key+'='+('{' + ','.join(map(str,value)) + '}'if isinstance(value,list)else str(int(value)))for key,value in v.items())+'},')
 headers.append('};');header=out/'front_facing_qualification.h';header.write_text('\n'.join(headers)+'\n')
 original=json.loads((fdl_check.parent/'commands.json').read_text());template=next(c for c in original if c['argv'][0]=='g++');a=template['argv'];start=a.index('-I'+str(fdlroot/'nvn-native/include'));end=a.index('-Wl,--as-needed');flags=a[1:start];libs=a[end:-2];exe=out/'depth-layout';run(['g++',*flags,'-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),'-Werror',n/'tests/depth_probe_layout.cc',n/'src/depth_a2d.cc',n/'src/depth_fdl.cc',*libs,'-o',exe],template['cwd']);dv=[v for v in json.loads(run([exe]))if v['format']==52 and v['width']==73]
 if len(dv)!=1:raise ValueError('actual D32F readback shape')
 obj=runtime_check.parent/'depth-native-api29.o'
 if runtime['artifacts'].get(str(obj))!=sha(obj):raise ValueError('unrecorded actual native object')
 libraries=[str(fdllib),depth['static_library'],a2d['static_library'],str(obj)];checked(inputs,{})
 c=dict(schema=1,kind=KIND,passed=True,gpu_execution=False,producer_sha256=sha(__file__),compiler_record=str(compiler/'build-record.json'),fdl_root=str(fdlroot),fdl_check=str(fdl_check),header=str(header),native_libraries=libraries,recipes=recipes,targets=views,depth_view=dv[0],jobs=320,draws=384,submissions=640,finishes=640,files=512,cases=192,replays=192,formats=['RGBA8_COLOR','D32F_DEPTH'],sequence_offsets={'draw':45056,'transfer':40960},max_fragcoord_channel_error=1,exact_face_channels=True,exact_D32F_depth=True,original_FS12_texture106_still_unknown=True,nvn_api=False,game_modules_executed=False,inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});(out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'
def build(check,out,root=None):
 c=json.loads(check.read_text());n=Path(__file__).resolve().parents[1]
 if c.get('kind')!=KIND or c.get('passed')is not True or c.get('gpu_execution')is not False or c.get('producer_sha256')!=sha(__file__)or c.get('draws')!=384 or c.get('submissions')!=640:raise ValueError('wrong/stale FF qualifier')
 checked(c['inputs'],{});checked(c['artifacts'],{});compiler_record(Path(c['compiler_record']).parent,n,{})
 return Path(c['header']),c,{str(check):sha(check),**c['inputs'],**c['artifacts']}
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);keys=('root','compiler','spirv-tools','fdl-check','depth-check','a2d-check','runtime-check','out')
 for k in keys:p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(produce(*[getattr(a,k.replace('-','_')).resolve()for k in keys]))

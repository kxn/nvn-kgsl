#!/usr/bin/env python3
"""Normal authored D16/D24X8/D32F depth write/compare/CP_BLIT qualifier inputs."""
import argparse,importlib.util,json,subprocess
from pathlib import Path
from package_graphics_pipelines import checked,compiler_record,sha
from texture_bridge_bundle import native_views

def module(p):
 s=importlib.util.spec_from_file_location('depth_qual_fdl',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def produce(root,compiler,shader_check,fdl_root,fdl_check,depth_check,a2d_check,runtime_check,out):
 n=Path(__file__).resolve().parents[1]
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 inputs={str(Path(__file__).resolve()):sha(__file__)};_,_,record=compiler_record(compiler,n,inputs)
 def check(p,kind):
  c=json.loads(p.read_text());inputs[str(p)]=sha(p)
  if c.get('kind')!=kind or not c.get('passed')or c.get('gpu_execution')is not False:raise ValueError('wrong normal input '+str(p))
  checked(c['inputs'],{});checked(c['artifacts'],{});inputs.update(c['inputs']);inputs.update(c['artifacts']);return c
 shader=check(shader_check,'authored-native-depth-recipes-v1');depth=check(depth_check,'native-depth-fdl-foundation-v1');a2d=check(a2d_check,'native-depth-a2d-host-v1');runtime=check(runtime_check,'native-depth-zero-color-host-v1')
 if shader['compiler_build']!=record or len(shader['recipes'])!=2:raise ValueError('authored shader compiler mismatch')
 fdltool=fdl_root/'nvn-native/tools/build_runtime_fdl.py';_,fdl=module(fdltool).verify(fdl_root,fdl_check);inputs.update(fdl['inputs']);inputs.update(fdl['artifacts']);inputs[str(fdl_check)]=sha(fdl_check);inputs[str(fdltool)]=sha(fdltool)
 for name in ['tools/check_depth_probe.py','tests/d2r_bridge/depth_probe_host.c','tests/test_depth_probe_validation.py','tests/depth_probe.c','tests/depth_probe_layout.cc','tests/pipeline_probe.c','tools/depth_probe_validation.py','tools/depth_probe_runner.py','include/nvn_native/depth_transfer.h','src/depth_transfer.c','src/pipeline.c','src/pipeline_views.c','src/pipeline_program.c','src/depth_native.cc','src/depth_a2d.cc','src/depth_fdl.cc']:
  p=n/name;inputs[str(p)]=sha(p)
 for check_record in (depth,a2d,runtime):
  for name in ['src/depth_transfer.c','src/depth_native.cc','src/depth_a2d.cc','src/depth_fdl.cc']:
   p=n/name;matches=[h for key,h in check_record['inputs'].items()if key.endswith('/nvn-native/'+name)]
   if matches and any(h!=sha(p)for h in matches):raise ValueError('runtime helper source differs '+name)
 out.mkdir(parents=True);commands=[]
 def run(a,cwd=None):
  a=list(map(str,a));r=subprocess.run(a,cwd=cwd,text=True,capture_output=True);commands.append(dict(argv=a,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if r.returncode:raise ValueError(r.stdout+r.stderr)
  return r.stdout
 original=json.loads((fdl_check.parent/'commands.json').read_text());t=next(c for c in original if c['argv'][0]=='g++');a=t['argv'];start=a.index('-I'+str(fdl_root/'nvn-native/include'));end=a.index('-Wl,--as-needed');flags=a[1:start];libs=a[end:-2];binary=out/'depth-layout'
 run(['g++',*flags,'-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),'-Werror',n/'tests/depth_probe_layout.cc',n/'src/depth_a2d.cc',n/'src/depth_fdl.cc',*libs,'-o',binary],t['cwd']);depth_views=json.loads(run([binary]));
 if len(depth_views)!=6:raise ValueError('depth FDL views')
 views,extra,_=native_views({'compiler_build':record},compiler,out,[(64,64),(73,41)]);inputs.update(extra)
 # The exact native_depth object is compiled/hashed by the normal runtime
 # checker using original generated A6 headers, not an unrecorded CFLAG build.
 obj=runtime_check.parent/'depth-native-api29.o'
 if runtime['artifacts'].get(str(obj))!=sha(obj):raise ValueError('unrecorded native depth helper object')
 libraries=[fdl['static_library'],depth['static_library'],a2d['static_library'],str(obj)]
 header=Path(shader['header']);checked(inputs,{})
 c=dict(schema=2,kind='raw-native-depth-qualification-v2',producer_sha256=sha(__file__),passed=True,gpu_execution=False,nvn_api=False,game_modules_executed=False,header=str(header),fdl_root=str(fdl_root),fdl_check=str(fdl_check),depth_check=str(depth_check),runtime_check=str(runtime_check),native_libraries=libraries,compiler_record=str(compiler/'build-record.json'),targets=views,depth_views=depth_views,formats=[0x32,0x33,0x34],origins=2,modes=12,repeats=2,jobs=216,submissions=432,draws=144,files=276,clear_submissions=144,readback_submissions=144,true_zero_color_draws=12,dummy_color_draws=12,draw_sequence_offset=45056,transfer_sequence_offset=40960,sequence_bytes_exempt=4,max_channel_error=1,max_unorm_depth_error=1,inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()})
 (out/'check.json').write_text(json.dumps(c,indent=2)+'\n');return out/'check.json'

def build(check,out,root=None):
 c=json.loads(check.read_text());n=Path(__file__).resolve().parents[1]
 if c.get('kind')!='raw-native-depth-qualification-v2'or c.get('producer_sha256')!=sha(__file__)or c.get('submissions')!=432 or c.get('gpu_execution')is not False:raise ValueError('wrong native depth qualifier')
 checked(c['inputs'],{});checked(c['artifacts'],{});compiler_record(Path(c['compiler_record']).parent,n,{})
 return Path(c['header']),c,{str(check):sha(check),**c['inputs'],**c['artifacts']}
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ['root','compiler','shader-check','fdl-root','fdl-check','depth-check','a2d-check','runtime-check','out']:p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(produce(*[getattr(a,k).resolve()for k in ['root','compiler','shader_check','fdl_root','fdl_check','depth_check','a2d_check','runtime_check','out']]))

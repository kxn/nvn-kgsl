#!/usr/bin/env python3
"""Normal authored BC TEXELFETCH VS/FS compilation, no game shader identity/GPU."""
import argparse,hashlib,importlib.util,json,os,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def build(root,ir3,tools,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 p=root/'nvn-native/tools/compute_driverparams.py';s=importlib.util.spec_from_file_location('bc_fetch_compiler',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
 record,inputs,_,count=m.compiler_inputs(root,ir3)
 compilers=[Path(p)for p in record['artifacts']if Path(p).name=='nvn-pipeline-ir3']
 if len(compilers)!=1:raise ValueError('normal pipeline compiler missing')
 compiler=compilers[0];sources=Path(__file__).resolve().parents[1]/'tests/shaders'
 for p in [Path(__file__).resolve(),root/'nvn-native/tools/compute_driverparams.py',sources/'bc_fetch.vert',sources/'bc_fetch.frag',*[tools/n for n in ('glslc','spirv-val','spirv-dis')]]:inputs[str(p)]=sha(p)
 out.mkdir(parents=True);commands=[]
 def run(argv):
  argv=list(map(str,argv));p=subprocess.run(argv,text=True,capture_output=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip,nopreamble'))
  commands.append(dict(argv=argv,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if p.returncode:raise ValueError(p.stdout+p.stderr)
 for stage in ('vert','frag'):
  stem=out/stage
  run([tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O',sources/('bc_fetch.'+stage),'-o',stem.with_suffix('.spv')])
  run([tools/'spirv-val','--target-env','spv1.3',stem.with_suffix('.spv')]);run([tools/'spirv-dis',stem.with_suffix('.spv'),'-o',stem.with_suffix('.spvasm')])
 run([compiler,out/'vert.spv',out/'frag.spv',out/'bc_fetch','0','0x06050002'])
 platform=[Path(p)for p in record['artifacts']if Path(p).name=='nvn-pipeline-platform']
 if len(platform)!=1:raise ValueError('normal platform compiler missing')
 run([platform[0],'0x06050002','1179648',out/'device'])
 recipe=json.loads((out/'bc_fetch.pipeline.json').read_text());usage=json.loads((out/'bc_fetch.fs.io.json').read_text())
 if any(recipe.get(k)!=v for k,v in {'texture_mask':2,'semantic_sampler_mask':0,'vertex_inputs':2,'has_kill':False,'fragcoord_mask':0}.items()):raise ValueError('unexpected authored fetch recipe')
 if usage['resource_semantics']['bindings']!=[dict(binding=1,fetches=1,size_queries=0,samples=0,offsets=0,other=0)]:raise ValueError('not pure TXF resource')
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('compiler input changed '+p)
 result=dict(schema=1,kind='authored-bc-fetch-pipeline-v1',passed=True,gpu_execution=False,original_game_shader=False,compiler_build=record,compiler_source_files=count,inputs=inputs,commands=commands,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()})
 (out/'check.json').write_text(json.dumps(result,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','ir3','spirv-tools','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(build(a.root.resolve(),a.ir3.resolve(),a.spirv_tools.resolve(),a.out.resolve()))

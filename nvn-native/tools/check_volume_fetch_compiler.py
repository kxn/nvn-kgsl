#!/usr/bin/env python3
"""Authored actual common-NIR negative-domain checks; no original shader edits/GPU."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','shader-check','spirv-tools','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();root=a.root.resolve();out=a.out.resolve();record=json.loads(a.shader_check.read_text())
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ROOT/.local output required')
 inputs={str(a.shader_check.resolve()):sha(a.shader_check),str(Path(__file__).resolve()):sha(__file__),**record['inputs'],**record['artifacts']}
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('stale normal inputs '+p)
 compiler=next(Path(p)for p in record['compiler_build']['artifacts']if Path(p).name=='nvn-pipeline-ir3');vs=Path(record['header']).parent/'volume_fetch.vert.spv';out.mkdir(parents=True);commands=[]
 cases=[('filtered3d','sampler3D','texture(t,v)','unqualified SPIR-V profile:'),('integer3d','isampler3D','vec4(texelFetch(t,ivec3(v),0))','unqualified SPIR-V profile:'),('offset3d','sampler3D','texelFetchOffset(t,ivec3(v),0,ivec3(1))','only constant two-component 32-bit texel-fetch offsets'),('cube','samplerCube','vec4(textureSize(t,0),0,1)','unqualified SPIR-V profile:')]
 results=[]
 for name,typename,expression,reason in cases:
  src=out/(name+'.frag');src.write_text('#version 450\nlayout(set=0,binding=1) uniform '+typename+' t;\nlayout(location=0)in vec3 v;layout(location=0)out vec4 c;void main(){c='+expression+';}\n');spv=src.with_suffix('.spv')
  for argv in ([a.spirv_tools/'glslc','--target-env=vulkan1.1','--target-spv=spv1.3','-O',src,'-o',spv],[a.spirv_tools/'spirv-val','--target-env','spv1.3',spv]):
   argv=list(map(str,argv));r=subprocess.run(argv,text=True,capture_output=True);commands.append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
   if r.returncode:raise ValueError('negative was invalid SPIR-V: '+r.stderr)
  argv=list(map(str,[compiler,vs,spv,out/name,'0','0x06050002']));r=subprocess.run(argv,text=True,capture_output=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip,nopreamble'));commands.append(dict(argv=argv,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
  if r.returncode!=4 or reason not in r.stderr or (out/(name+'.pipeline.h')).exists():raise ValueError('unsupported actual NIR accepted '+name)
  results.append(dict(case=name,spirv_valid=True,normal_compiler_rejected=True,returncode=r.returncode,stderr=r.stderr))
 for p in (a.spirv_tools/'glslc',a.spirv_tools/'spirv-val'):inputs[str(p.resolve())]=sha(p)
 for p,h in inputs.items():
  if sha(p)!=h:raise ValueError('input changed '+p)
 result=dict(schema=1,passed=True,gpu_execution=False,android_execution=False,results=results,inputs=inputs,commands=commands,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});(out/'check.json').write_text(json.dumps(result,indent=2)+'\n');print(out/'check.json')
if __name__=='__main__':main()

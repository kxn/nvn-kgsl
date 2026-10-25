#!/usr/bin/env python3
"""Normal authored negative storage shaders. Actual SPIR-V/NIR/IR3, no GPU."""
import argparse,json,subprocess,os
from pathlib import Path
from package_graphics_pipelines import compiler_record,sha,checked

def check(root,compiler,tools,positive,out):
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored output required')
 native=root/'nvn-native';inputs={str(Path(__file__).resolve()):sha(__file__)};cc,_,_=compiler_record(compiler,native,inputs)
 p=json.loads(positive.read_text());inputs[str(positive)]=sha(positive)
 for group in ('inputs','artifacts'):
  for f,h in p[group].items():
   if sha(f)!=h:raise ValueError('changed positive source/product '+f)
   inputs[f]=h
 out.mkdir(parents=True);commands=[];results=[]
 cases={
 'atomic':('#version 450\n', 'layout(set=0,binding=1,std430) buffer Data { uint data[]; } sb;', 'uint x=atomicAdd(sb.data[0],1u);'),
 'dynamic_binding':('#version 450\n', 'layout(set=0,binding=1,std430) readonly buffer Data { uint data[]; } sb[2];', 'uint x=sb[IDX&1u].data[0];'),
 'binding16':('#version 450\n', 'layout(set=0,binding=16,std430) readonly buffer Data { uint data[]; } sb;', 'uint x=sb.data[IDX];'),
 'width64':('#version 450\n#extension GL_ARB_gpu_shader_int64 : require\n', 'layout(set=0,binding=1,std430) readonly buffer Data { uint64_t data[]; } sb;', 'uint x=uint(sb.data[IDX]);'),
 'width16':('#version 450\n#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require\n#extension GL_EXT_shader_16bit_storage : require\n', 'layout(set=0,binding=1,std430) readonly buffer Data { uint16_t data[]; } sb;', 'uint x=uint(sb.data[IDX]);'),
 }
 for stage in ('vert','frag'):
  for name,(version,decl,body)in cases.items():
   if stage=='vert':io='layout(location=0)in vec2 position;layout(location=0)out vec4 value;';expression='uint(gl_VertexIndex)';tail='gl_Position=vec4(position,0.,1.);value=vec4(float(x)/255.0);'
   else:io='layout(location=0)in vec4 value;layout(location=0)out vec4 color;';expression='uint(gl_FragCoord.x)';tail='color=value+vec4(float(x)/255.0);'
   source=out/(name+'.'+stage);source.write_text(version+io+decl+'void main(){'+body.replace('IDX',expression)+tail+'}\n');dest=source.with_suffix(source.suffix+'.spv')
   argv=[str(tools/'glslc'),'--target-env=vulkan1.1','--target-spv=spv1.3','-O',str(source),'-o',str(dest)];q=subprocess.run(argv,text=True,capture_output=True);commands.append(dict(argv=argv,returncode=q.returncode,stdout=q.stdout,stderr=q.stderr))
   if q.returncode:raise ValueError('negative must reach actual native compiler '+q.stderr)
   argv=[str(tools/'spirv-val'),'--target-env','spv1.3',str(dest)];q=subprocess.run(argv,text=True,capture_output=True);commands.append(dict(argv=argv,returncode=q.returncode,stdout=q.stdout,stderr=q.stderr))
   if q.returncode:raise ValueError('negative SPIR-V invalid '+q.stderr)
   vs=dest if stage=='vert'else positive.parent/'case0.vert.spv';fs=dest if stage=='frag'else positive.parent/'case0.frag.spv';prefix=out/(name+'-'+stage)
   argv=[str(cc),str(vs),str(fs),str(prefix),'0','0x06050002'];q=subprocess.run(argv,text=True,capture_output=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip'));commands.append(dict(argv=argv,returncode=q.returncode,stdout=q.stdout,stderr=q.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
   if not q.returncode:raise ValueError('unsupported storage shader accepted '+name+' '+stage)
   if 'graphics storage'not in q.stderr and 'unqualified SPIR-V profile:'not in q.stderr:raise ValueError('negative failed outside semantic/profile gates '+name+' '+q.stderr)
   results.append(dict(stage=stage,case=name,rejection=q.stderr))
 for t in ('glslc','spirv-val'):inputs[str((tools/t).resolve())]=sha((tools/t).resolve())
 checked(inputs,{})
 r=dict(schema=1,kind='graphics-storage-compiler-limits-v1',passed=True,gpu_execution=False,results=results,inputs=inputs,artifacts={str(f):sha(f)for f in out.iterdir()if f.is_file()},scope='Ten valid authored SPIR-V inputs rejected by actual storage or strict source profile gates; not game/GPU qualification');(out/'check.json').write_text(json.dumps(r,indent=2)+'\n');return out/'check.json'
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('root','compiler','spirv-tools','positive','out'):p.add_argument('--'+k,type=Path,required=True)
 a=p.parse_args();print(check(*[getattr(a,k).resolve()for k in ('root','compiler','spirv_tools','positive','out')]))

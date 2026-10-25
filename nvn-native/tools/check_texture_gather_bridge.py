#!/usr/bin/env python3
"""HOST ONLY standard scalar floating2D gather bridge; real original C1/FragDepth retained.
Existing spirv-dis/as/val produce qualification-only variations, not game shaders.
"""
import argparse,hashlib,importlib.util,json,os,re,subprocess
from pathlib import Path

def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def load(p,n):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def main():
 a=argparse.ArgumentParser(description=__doc__)
 for n in('root','compiled','compiler','spirv-tools','out'):a.add_argument('--'+n,type=Path,required=True)
 for n in('baseline-root','baseline-compiler','regression-check'):a.add_argument('--'+n,type=Path)
 x=a.parse_args();
 if bool(x.baseline_root)!=bool(x.baseline_compiler):a.error('baseline root/compiler must be supplied together')
 root=x.root.resolve();out=x.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):a.error('fresh ignored output required')
 out.mkdir(parents=True);inputs={};commands=[];proof={'passed':False,'gpu_execution':False,'scope':__doc__}
 def pin(p,h=None):
  p=Path(p).resolve();v=sha(p)
  if h is not None and v!=h:raise ValueError('source/artifact changed '+str(p))
  inputs[str(p)]=v
 def run(args):
  args=list(map(str,args));r=subprocess.run(args,text=True,capture_output=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip'));commands.append({'argv':args,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr});return r
 def yes(v,msg):
  if not v:raise ValueError(msg)
 try:
  pin(__file__);own=load(root/'nvn-native/tools/compute_driverparams.py','common_compiler');pin(own.__file__)
  new,extra,_,_=own.compiler_inputs(root,x.compiler.resolve());inputs.update(extra)
  if x.baseline_compiler:
   old,extra,_,_=own.compiler_inputs(x.baseline_root.resolve(),x.baseline_compiler.resolve());inputs.update(extra)
  pin(x.compiled);actual=json.loads(x.compiled.read_text())
  for p,h in{**actual['inputs'],**actual['artifacts']}.items():pin(p,h)
  yes(len(actual['pairs'])==1 and actual['pairs'][0]['passed'],'one original actual pair must compile')
  p=actual['pairs'][0];vs=actual['stages'][p['vs']];fs=actual['stages'][p['fs']];program=p['program'];g=program['fs']['nir_usage'].get('gather_usage')
  yes(g and g['count']==1 and len(g['bindings'])==1,'actual NIR gather proof absent')
  binding=g['bindings'][0]['binding'];samplers=g['bindings'][0]['semantic_sampler_mask'];yes(g['bindings'][0]['component_mask']==1 and samplers>0,'actual gather component/sampler')
  use=program['fs']['nir_usage']['resource_semantics']['bindings'];yes(len(use)==1 and use[0]['other']==1 and use[0]['samples']==0,'gather must not masquerade as ordinary sample')
  depth=program['fs'].get('depth_export');yes(depth and depth['writes_depth']and depth['requires_depth_attachment_contract'],'actual original FragDepth retained')
  yes(fs['envelope']['inputs']['c1']['size']==256,'complete original C1 retained')
  newcc=x.compiler.resolve()/'build/src/freedreno/fdl/nvn-graphics-ir3';nativecc=x.compiler.resolve()/'build/src/freedreno/fdl/nvn-pipeline-ir3'
  tools={n:x.spirv_tools.resolve()/n for n in('spirv-as','spirv-dis','spirv-val')}
  for q in tools.values():pin(q)
  vsp=Path(vs['prefix']+'.spv');fsp=Path(fs['prefix']+'.spv')
  if x.baseline_compiler:
   oldcc=x.baseline_compiler.resolve()/'build/src/freedreno/fdl/nvn-graphics-ir3'
   r=run([oldcc,'--unit-render-scale',vsp,fsp,out/'old-rejected',0,'0x06050002']);yes(r.returncode!=0 and 'unqualified SPIR-V profile'in r.stderr,'old bridge unexpectedly accepted gather')
  r=run([nativecc,'--unit-render-scale',vsp,fsp,out/'depth-recipe-rejected',0,'0x06050002']);yes(r.returncode==4 and 'one float32 vec4 color output'in r.stderr,'existing color-only recipe must not accept depth output')
  dis=out/'original.spvasm';yes(run([tools['spirv-dis'],fsp,'-o',dis]).returncode==0,'disassembly failed');text=dis.read_text()
  line=re.search(r'^\s*(%\w+) = OpImageGather (%\w+) (%\w+) (%\w+) (%\w+) None\s*$',text,re.M);yes(line is not None and text.count(' = OpImageGather ')==1,'bounded existing gather instruction shape')
  integer=re.search(r'^\s*(%\w+) = OpTypeInt 32 1\s*$',text,re.M);floating=re.search(r'^\s*(%\w+) = OpTypeFloat 32\s*$',text,re.M);yes(integer and floating,'existing scalar types absent');it=integer[1];ft=floating[1]
  def mutate(name,global_lines,body=None,source=None):
   t=source or text
   yes('%qual_'not in t,'qualification ID collision')
   first=re.search(r'^.* = OpFunction .*$',t,re.M);yes(first is not None,'function boundary')
   t=t[:first.start()]+global_lines+'\n'+t[first.start():]
   if body is not None:t=t.replace(line[0],body,1)
   asm=out/(name+'.spvasm');asm.write_text(t);spv=out/(name+'.spv');r=run([tools['spirv-as'],'--target-env','spv1.3',asm,'-o',spv]);yes(r.returncode==0,'assembler failed '+name)
   r=run([tools['spirv-val'],spv]);return spv,r
  results=[]
  for component in(1,2,3):
   body=line[0].replace(line[5]+' None','%qual_component None');spv,r=mutate('component'+str(component),'%qual_component = OpConstant '+it+' '+str(component),body);yes(r.returncode==0,'valid component input')
   prefix=out/('component'+str(component));r=run([newcc,'--unit-render-scale',vsp,spv,prefix,0,'0x06050002']);yes(r.returncode==0,'Mesa native gather component '+str(component))
   meta=json.loads(Path(str(prefix)+'.link.json').read_text());fact=meta['fs']['nir_usage']['gather_usage']['bindings'][0];yes(fact['component_mask']==1<<component and fact['semantic_sampler_mask']==samplers and meta['fs']['nir_usage']['resource_semantics']['bindings'][0]['samples']==0,'distinct actual gather component metadata')
   yes(('gather4'+'rgba'[component])in Path(str(prefix)+'.fs.asm').read_text().lower(),'native component opcode')
   results.append({'name':'component'+str(component),'passed':True,'qualification_only':True})
  negatives=[]
  global_offset='%qual_vec2i = OpTypeVector '+it+' 2\n%qual_i1 = OpConstant '+it+' 1\n%qual_im1 = OpConstant '+it+' -1\n%qual_offset = OpConstantComposite %qual_vec2i %qual_i1 %qual_im1'
  ext=text.replace('OpCapability Shader','OpCapability Shader\n               OpCapability ImageGatherExtended',1)
  spv,r=mutate('constant-offset',global_offset,line[0].replace(' None',' ConstOffset %qual_offset'),ext);yes(r.returncode==0,'valid gather constant-offset input')
  negatives.append(('constant-offset',spv,'gather offset not qualified'))
  decorates=re.findall(r'OpDecorate (%\w+) Binding 1\s*$',text,re.M);yes(len(decorates)==1,'actual scalar texture binding')
  changed=re.sub(r'(OpDecorate '+re.escape(decorates[0])+r' DescriptorSet )0\b',r'\g<1>1',text);spv,r=mutate('set-one','',source=changed);yes(r.returncode==0,'valid set-one input');negatives.append(('set-one',spv,'sampled resource declaration layout'))
  image=re.search(r'OpTypeImage '+re.escape(ft)+r' 2D 0 0 0 1 Unknown',text);yes(image is not None,'actual image type')
  changed=text[:image.start()]+image[0].replace('2D 0 0 0','2D 1 0 0')+text[image.end():]
  body=line[0].replace('OpImageGather','OpImageDrefGather').replace(line[5]+' None','%qual_reference None');spv,r=mutate('shadow','%qual_reference = OpConstant '+ft+' 0',body,changed);yes(r.returncode==0,'valid shadow gather input');negatives.append(('shadow',spv,'only floating scalar 2D sample/gather-level-zero'))
  for name,spv,message in negatives:
   r=run([newcc,'--unit-render-scale',vsp,spv,out/(name+'-rejected'),0,'0x06050002']);yes(r.returncode==4 and message in r.stderr,'strict NIR negative '+name);results.append({'name':name,'nir_rejected':True,'qualification_only':True})
  # The current official SPIR-V OpImageGather specification defines values
  # outside0..3 as undefined behavior, not module-invalid. Record the actual
  # validator result; do not manufacture a rejection claim or execute it.
  spv,r=mutate('component-undefined','%qual_component = OpConstant '+it+' 4',line[0].replace(line[5]+' None','%qual_component None'))
  results.append({'name':'component4','spirv_val_returncode':r.returncode,'undefined_behavior':True,'native_compilation_attempted':False,'source':'https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html#OpImageGather'})
  comparisons=[]
  if x.regression_check:
   pin(x.regression_check);reg=json.loads(x.regression_check.read_text())
   for p,h in{**reg['inputs'],**reg['artifacts']}.items():pin(p,h)
   chosen={}
   for pair in reg['pairs']:
    if not pair['passed']:continue
    f=reg['stages'][pair['fs']];n=len(f.get('resources',{}).get('textures',[]))
    if n in(0,1,2)and n not in chosen:chosen[n]=pair
   yes(set(chosen)=={0,1,2},'representative original color/sample/two-resource groups absent')
   for n,pair in sorted(chosen.items()):
    v=reg['stages'][pair['vs']];f=reg['stages'][pair['fs']];prefix=out/('unchanged-'+str(n));r=run([newcc,'--unit-render-scale',v['prefix']+'.spv',f['prefix']+'.spv',prefix,0,'0x06050002']);yes(r.returncode==0,'old normal shader pair no longer compiles')
    compared=[]
    for suffix in('.vs.bin','.fs.bin','.vs.asm','.fs.asm','.link.json'):
     prior=Path(pair['prefix']+suffix);now=Path(str(prefix)+suffix);yes(now.read_bytes()==prior.read_bytes(),'old native/metadata changed '+suffix);compared.append(suffix)
    comparisons.append({'texture_count_group':n,'vs_request':pair['vs'],'fs_request':pair['fs'],'byte_identical':compared})
  for p,h in inputs.items():yes(sha(p)==h,'input changed during check '+p)
  proof.update(passed=True,original_pair={'vs_bytes':program['vs']['binary_bytes'],'fs_bytes':program['fs']['binary_bytes'],'gather':g,'depth_export':depth,'c1_bytes':256,'native_gpu_execution':False},old_bridge_rejected=bool(x.baseline_compiler),historical_regressions_checked=bool(x.regression_check),color_only_recipe_rejected=True,variations=results,regressions=comparisons,compiler_source_files_changed=['shader/nir_bridge.h','shader/graphics_ir3.c'])
 except Exception as e:proof['error']=str(e)
 proof.update(inputs=inputs,commands=commands);proof['artifacts']={str(p):sha(p)for p in out.rglob('*')if p.is_file()};(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps({'passed':proof['passed'],'check':str(out/'check.json'),'error':proof.get('error')}));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())

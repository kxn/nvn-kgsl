#!/usr/bin/env python3
"""HOST ONLY option-forwarding/provider integrity test; no host build or GPU."""
import argparse,hashlib,importlib.util,json,sys
from pathlib import Path

def module(path,name):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--native',type=Path,required=True);p.add_argument('--check',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();root=a.root.resolve();native=a.native.resolve();out=a.out.resolve();sys.path[:0]=[str(root/'nvn-native/tools'),str(native/'tools')]
 # Verify packages with their declared original producer. Candidate build
 # scripts are still loaded explicitly by path below; records are not moved.
 # These are mocked OPTIONS dictionaries for pure argv reconstruction only;
 # they are not accepted as an actual host/module build record or deployment.
 activity=module(native/'tools/build_d2r_activity_apk.py','pipeline_option_activity');cli=module(native/'tools/run_d2r_host.py','pipeline_option_cli')
 inventory={str(Path(__file__).resolve()):hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
 cases=0
 for enabled in (False,True):
  for abi,helper in ((1,cli.host_verify_argv),(2,activity.host_verify)):
   options={'host_abi':abi,'rewrite_abi':'elf'if abi==1 else'elfdesc','rules':'native','private_gates':1,'abi':'arm64-v8a','library_kind':'shared','execute':False,'deploy':False,'python':sys.executable,'exefs':'/mock/original','ndk':'/mock/ndk','jobs':2}
   if enabled:options['graphics_pipeline_check']=str(a.check.resolve())
   argv=helper({'options':options,'modules':str(out/'mock-modules'),'artifacts':inventory,'sources':inventory,'tools':inventory},root)
   assert ('--graphics-pipeline-check'in argv)==enabled
   if enabled:assert argv[argv.index('--graphics-pipeline-check')+1]==str(a.check.resolve())
   cases+=1
 # Preserve independently selected color features when reconstructing the
 # canonical verification command. Dropping them prevents a correctly built
 # original host from reaching APK packaging or CLI deployment verification.
 for mask in range(4):
  for abi,helper in ((1,cli.host_verify_argv),(2,activity.host_verify)):
   options={'host_abi':abi,'rewrite_abi':'elf'if abi==1 else'elfdesc','rules':'native','private_gates':1,'abi':'arm64-v8a','library_kind':'shared','execute':False,'deploy':False,'python':sys.executable,'exefs':'/mock/original','ndk':'/mock/ndk','jobs':2}
   if mask&1:options.update(clear_color_kernel_check='/mock/clear-kernel.json',clear_color_constant_check='/mock/clear-constants.json')
   if mask&2:options['color_targets']=True
   argv=helper({'options':options,'modules':str(out/'mock-modules'),'artifacts':inventory,'sources':inventory,'tools':inventory},root)
   for key in ('clear_color_kernel_check','clear_color_constant_check'):
    flag='--'+key.replace('_','-');assert(flag in argv)==bool(mask&1)
    if mask&1:assert argv[argv.index(flag)+1]==options[key]
   assert('--color-targets'in argv)==bool(mask&2);cases+=1
 from graphics_pipeline_cache import build
 header,manifest=build(root,a.check);assert header.is_file()and manifest['source_only']and not manifest['draw_supported']and not manifest['runtime_compiler'];cases+=1
 # Relocating a check cannot authorize an unrecorded generated header. No
 # production input/output is mutated; this directory contains only negatives.
 out.mkdir(parents=True);record=json.loads(a.check.read_text());(out/'check.json').write_text(json.dumps(record));(out/'offline_graphics_pipelines.h').write_text('unrecorded header\n')
 try:build(root,out/'check.json')
 except(ValueError,OSError):cases+=1
 else:raise AssertionError('moved/unrecorded header accepted')
 record=json.loads(a.check.read_text());record['outputs'][str(header)]='0'*64;(out/'check.json').write_text(json.dumps(record))
 try:build(root,out/'check.json')
 except(ValueError,OSError):cases+=1
 else:raise AssertionError('wrong header hash accepted')
 result={'scope':'HOST ONLY mocked option forwarding and actual provider closure','cases':cases,'gpu_submits':0,'original_modules_executed':False}
 inputs=dict(manifest['inputs'])
 for path in (Path(__file__).resolve(),native/'tools/build_d2r_activity_apk.py',native/'tools/run_d2r_host.py'):
  inputs[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
 (out/'option-proof.json').write_text(json.dumps(dict(result,passed=True,inputs=inputs),indent=2)+'\n')
 print(json.dumps(result))
if __name__=='__main__':main()

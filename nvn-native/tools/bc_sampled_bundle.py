#!/usr/bin/env python3
"""Verify normal BC qualifier/FDL closure for the standard APK builder."""
import importlib.util,json,shutil
from pathlib import Path
from validate_bc_sampled import verify,sha
def build(check,out,*,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();out=Path(out).resolve();check=Path(check).resolve()
 c=verify(check)
 producer=root/'nvn-native/tools/build_runtime_fdl.py';s=importlib.util.spec_from_file_location('bc_fdl_bundle',producer);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
 library,fdl=m.verify(root,Path(c['runtime_fdl_check']))
 if str(library)!=c['runtime_fdl_library']or c['inputs'].get(str(library))!=sha(library):raise ValueError('FDL actual library differs')
 native=Path(__file__).resolve().parents[1]
 for rel in ('src/texture_bc_shadow.c','src/pipeline.c','tests/bc_sampled_probe.c','tests/bc_sampled_cases.h','include/nvn_native/texture_bc_shadow.h','include/nvn_native/pipeline.h'):
  p=native/rel
  if c['inputs'].get(str(p))!=sha(p):raise ValueError('qualifier from a different current source '+str(p))
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored bundle required')
 out.mkdir(parents=True);header=out/'bc_sampled_qualification.h';shutil.copyfile(c['qualification_header'],header)
 inputs={str(check):sha(check),**c['inputs'],**c['artifacts'],str(producer):sha(producer),str(Path(__file__).resolve()):sha(__file__),str(Path(__file__).with_name('validate_bc_sampled.py')):sha(Path(__file__).with_name('validate_bc_sampled.py')),str(header):sha(header)}
 result=dict(c,normal_check=str(check),header=str(header),runtime_fdl_library=str(library))
 return header,result,inputs

#!/usr/bin/env python3
"""Normal texel GPU qualification bundle, existing checked kernel unchanged."""
import argparse,hashlib,json
from pathlib import Path
from public_copy_bundle import build as public_build
from texture_texels_validation import IDS,validate_cases

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def build(kernel,out,*,root=None):
 root=(root or Path(__file__).resolve().parents[2]).resolve();out=Path(out).resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):raise ValueError('fresh ignored texel bundle required')
 out.mkdir(parents=True);header,baseline,inputs=public_build(kernel,out/'native',root=root);cases=[baseline['cases'][n]for n in IDS];validate_cases(cases)
 h=out/'texture_texels_bundle.h';h.write_text('/* Normal authored texel matrix, checked original raw-block image. */\n#include "native/public_copy_bundle.h"\n#define NN_TEXTURE_TEXELS_CASES 156\nstatic const struct nn_public_copy_case *const nn_texture_texel_cases[]={'+','.join('nn_public_copy_cases+'+str(n)for n in IDS)+'};\n')
 for p in [Path(__file__).resolve(),root/'nvn-native/tools/texture_texels_validation.py',h]:inputs[str(p)]=sha(p)
 result=dict(schema=1,kind='public-nvn-texture-texels-qualification-v1',gpu_execution=False,android_execution=False,cases=cases,original_case_ids=list(IDS),native_baseline=baseline,kernel_check=str(Path(kernel).resolve()),inputs=inputs,artifacts={str(p):sha(p)for p in out.iterdir()if p.is_file()});check=out/'check.json';check.write_text(json.dumps(result,indent=2)+'\n');inputs[str(check)]=sha(check);return h,result,inputs
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2]);p.add_argument('--kernel-check',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();print(build(a.kernel_check.resolve(),a.out.resolve(),root=a.root.resolve())[0])

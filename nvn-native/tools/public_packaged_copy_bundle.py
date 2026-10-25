#!/usr/bin/env python3
"""Experimental BNTX writer8B profile; normal actual raw-block kernel, no GPU."""
import argparse
from pathlib import Path
from public_copy_bundle import build as common_build
def build(kernel,out,*,root=None):return common_build(kernel,out,root=root,packaged=True)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 for n in ('root','kernel-check','out'):p.add_argument('--'+n,type=Path,required=True)
 a=p.parse_args();print(build(a.kernel_check.resolve(),a.out.resolve(),root=a.root.resolve())[0])

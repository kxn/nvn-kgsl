"""Package existing checked authored linear-copy kernel; no compiler/GPU run."""
from pathlib import Path
import hashlib,importlib.util,json

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def build(check,out,*,root=None):
 root=Path(root or Path(__file__).resolve().parents[2]).resolve();check=Path(check).resolve();out=Path(out).resolve()
 spec=importlib.util.spec_from_file_location('fullsize_kernel_closure',root/'nvn-native/tools/texture_conversion.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 manifest,inputs,unused_ir3=m.kernel_check(root,check)
 entry=next(c for c in manifest['cases'] if c['name']=='linear_copy');source=Path(entry['header']);out.mkdir(parents=True,exist_ok=True)
 header=out/'fullsize-linear.native.h';header.write_bytes(source.read_bytes())
 inputs[str(Path(__file__).resolve())]=sha(__file__);inputs[str(root/'nvn-native/tools/texture_conversion.py')]=sha(root/'nvn-native/tools/texture_conversion.py');inputs[str(header)]=sha(header)
 record={'schema':1,'kind':'native-fullsize-ahb-linear-copy-v1','scope':'Authored native compute plus explicit AHB display qualification; not NVN Draw/game frame','width':2880,'height':1620,'kernel_check':str(check),'kernel_check_sha256':sha(check),'kernel_header_sha256':sha(header),'kernel_manifest':manifest,'cpu_image_copy':False,'game_modules_executed':False}
 return header,record,inputs

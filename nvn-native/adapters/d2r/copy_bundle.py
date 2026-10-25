"""Checked color bundle and original Mesa PM4 generator for the copy qualifier."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from graphics_api_bundle import build as graphics_build

MESA_SHA = '794b3d1729075dae7ebd47c7283b7ca0cdb5dec668f7f11cc0a5d82f6138a0a5'
PM4_SHA = '8253409f517a2eb03edd5c889b9a654705e69c1d4b17b74d9b0b4edce5804e85'
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def build(check_path,out):
    check_path,out=Path(check_path).resolve(),Path(out).resolve()
    header,data,hashes=graphics_build(check_path,out)
    if data.get('modes') is not True or data['program']['chip_id']!=0x06050002 or data['image']['schema']!=3:
        raise ValueError('copy requires the checked A650 schema3 color image')
    check=json.loads(check_path.read_text());ir3=check['ir3_build']
    if ir3['mesa_archive_sha256']!=MESA_SHA: raise ValueError('wrong pinned Mesa source')
    compilers=[Path(p) for p in ir3['artifacts'] if Path(p).name=='nvn-graphics-ir3']
    if len(compilers)!=1: raise ValueError('missing original IR3 build identity')
    ir3out=compilers[0].parents[4];manifest=ir3out/'source-hashes.json'
    if sha(manifest)!=ir3['source_manifest_sha256']: raise ValueError('Mesa source manifest changed')
    recorded=json.loads(manifest.read_text());source=ir3out/'source/mesa-26.2.5'
    prefix='src/freedreno/registers/'
    entries={name:value for name,value in recorded.items() if name.startswith(prefix)}
    registry=source/prefix
    paths=list(registry.rglob('*'))
    if any(p.is_symlink() for p in paths): raise ValueError('PM4 source symlink is outside the fixed profile')
    actual={str(p.relative_to(source)) for p in paths if p.is_file()}
    if not entries or actual!=set(entries): raise ValueError('PM4 generator source inventory changed')
    for name,value in entries.items():
        p=source/name
        if sha(p)!=value: raise ValueError('PM4 generator source changed: '+name)
        hashes[str(p)]=value
    hashes[str(manifest)]=sha(manifest);hashes[str(Path(sys.executable).resolve())]=sha(Path(sys.executable).resolve())
    generated=out/'pm4'
    command=[sys.executable,str(registry/'gen_header.py'),'--rnn',str(registry),'--xml',str(registry/'adreno/adreno_pm4.xml'),'c-defines']
    result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'},check=True)
    if hashlib.sha256(result.stdout).hexdigest()!=PM4_SHA: raise ValueError('regenerated PM4 packers differ from the qualified input')
    pm4=generated/'adreno_pm4.xml.h';audit=generated/'generation.json'
    audit_bytes=(json.dumps({'argv':command,'stderr':result.stderr.decode(),'source_manifest_sha256':sha(manifest),'header_sha256':PM4_SHA,'scope':'Original Mesa XML generator only; no driver/API/runtime'},indent=2)+'\n').encode()
    if generated.exists() or generated.is_symlink():
        if generated.is_symlink() or not generated.is_dir() or {p.name for p in generated.iterdir()}!={'adreno_pm4.xml.h','generation.json'}:
            raise ValueError('cached PM4 directory has unexpected entries')
        if any(p.is_symlink() or not p.is_file() for p in generated.iterdir()) or pm4.read_bytes()!=result.stdout or audit.read_bytes()!=audit_bytes:
            raise ValueError('cached PM4 packers/audit changed; use a fresh output')
    else:
        generated.mkdir(exist_ok=False);pm4.write_bytes(result.stdout);audit.write_bytes(audit_bytes)
    hashes[str(pm4)]=sha(pm4);hashes[str(audit)]=sha(audit)
    return header,data,hashes,generated

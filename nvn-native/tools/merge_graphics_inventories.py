# SPDX-License-Identifier: MIT
#!/usr/bin/env python3
"""Compose normal bounded container inventories; no shader-byte transformation."""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--inventory',type=Path,action='append',required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
if a.out.exists():p.error('fresh output required')
inputs={str(Path(__file__).resolve()):sha(Path(__file__))};entries=[]
for path in a.inventory:
 path=path.resolve();j=json.loads(path.read_text())
 if j.get('kind')!='declared-graphics-container-inventory-v1':raise ValueError('inventory kind')
 for name,value in {str(path):sha(path),**j['source_inputs']}.items():
  if sha(Path(name))!=value or name in inputs and inputs[name]!=value:raise ValueError('changed/conflicting source')
  inputs[name]=value
 for e in j['entries']:
  if sha(Path(e['fixture']))!=e['fixture_sha256']:raise ValueError('changed fixture')
  entries.append(e)
a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(dict(schema=1,kind='declared-graphics-container-inventory-v1',source_inputs=inputs,entries=entries,scope='Normal original source identities with explicit multiple specialization inputs; not runtime/frame capture'),indent=2)+'\n');print(a.out)

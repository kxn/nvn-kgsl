#!/usr/bin/env python3
"""Synthetic offline oracle negatives; never Android/GPU evidence."""
import copy,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import nvn_pipeline_color_validation as v

def main():
 import numpy as np
 check=json.loads(Path(sys.argv[1]).read_text());nonce='abcdef0123456789abcdef0123456789';uid=10234;cases=[];captured={};formats=check['formats']
 for i in range(66):
  f=formats[i//3];state=i%3;c=dict(case=i,format=f['format'],state=state,storage_bytes=f['storage_bytes'],pool_bytes=f['pool_bytes'],cpp=f['cpp'],gob_pitch=f['gob_pitch'],block_height_log2=f['block_height_log2'],handle_kind='call'if state==1 else'copy');cases.append(c)
  index,padding=v.addresses(c['storage_bytes'],c['cpp'],c['gob_pitch'],c['block_height_log2'])
  arena=bytes((j*7+i)%256 for j in range(131072))
  for repeat in range(2):
   n=i*2+repeat;raw=np.full(c['pool_bytes'],0xa5,dtype=np.uint8);canonical=raw[4096:4096+c['storage_bytes']];canonical[:]=0
   source=(Path(check['expected'])/('expected-%03u.raw'%n)).read_bytes();canonical[index]=np.frombuffer(source,dtype=np.uint8).reshape(41,73*c['cpp'])
   captured[v.NAMES[n]]=raw.tobytes()+v.readonly_inputs(state,repeat)+arena+arena
 outer=sum((f['pool_bytes']-f['storage_bytes'])*6 for f in formats);active=sum(73*41*f['cpp']*6 for f in formats)
 data=dict(schema=1,operation='nvn_gob_color_targets',uid=uid,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,formats=22,submissions=132,finishes=132,completed_cases=132,files=132,recorded_handles=198,released_handles=198,copy_commands=66,call_commands=66,replays=66,source_address_checks=132,shader_source_finalizes=1,inflight_finalize_rejections=264,guard_byte_checks=outer,readonly_byte_checks=132*139264,active_bytes_saved=active,pixel_checks=0,pixel_oracle_on_host=True,same_handle_replay=True,source_flags=9,pool_flags=164,canonical_prefix=4096,source_kind=254,public_nvn=True,cpu_image_conversion=False,game_modules_executed=False,passed=True,bad_case=0xffffffff,run_nonce=nonce,run_directory='nvn-pipeline-'+nonce,cases=cases)
 result=v.validate(data,captured,check,nonce,uid);assert result['passed'];negatives=0
 def deny(d=None,raw=None,n=nonce,u=uid):
  nonlocal negatives
  try:v.validate(d or data,raw if raw is not None else captured,check,n,u)
  except (RuntimeError,ValueError,TypeError,KeyError):negatives+=1;return
  raise AssertionError('invalid synthetic evidence accepted')
 for k,w in (('submissions',131),('finishes',False),('uid',0),('source_flags',0),('public_nvn',False),('game_modules_executed',True),('resources_retained',True),('replays',0),('copy_commands',0),('call_commands',0),('readonly_byte_checks',0),('source_kind',0),('run_nonce','0'*32),('stage','finish')):
  d=copy.deepcopy(data);d[k]=w;deny(d)
 deny(u=0);deny(n='stale');d=copy.deepcopy(data);d['cases'][0]['cpp']=True;deny(d)
 raw=dict(captured);raw.pop(v.NAMES[0]);deny(raw=raw);raw=dict(captured);raw['extra.raw']=b'';deny(raw=raw)
 f=formats[0];index,padding=v.addresses(f['storage_bytes'],f['cpp'],f['gob_pitch'],f['block_height_log2'])
 for off in (0,4096+f['storage_bytes'],4096+int(np.flatnonzero(padding)[0]),4096+int(index[0,0]),f['pool_bytes']+256,f['pool_bytes']+4096+256,f['pool_bytes']+139264+7):
  raw=dict(captured);b=bytearray(raw[v.NAMES[0]]);b[off]^=0xff;raw[v.NAMES[0]]=bytes(b);deny(raw=raw)
 raw=dict(captured);raw[v.NAMES[0]]=raw[v.NAMES[0]][:-1];deny(raw=raw)
 raw=dict(captured);b=bytearray(raw[v.NAMES[0]]);b[4096:4096+f['storage_bytes']]=b'\0'*f['storage_bytes'];raw[v.NAMES[0]]=bytes(b);deny(raw=raw)
 # Synthetic data only: this deterministic manifest is not a GPU submission.
 print(json.dumps(dict(passed=True,negative_cases=negatives,synthetic_files=132,synthetic_raw_bytes=sum(map(len,captured.values())),oracle=result,gpu_execution=False)))
if __name__=='__main__':main()

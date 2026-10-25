from pathlib import Path
import sys,copy,json,importlib.util
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
import nvn_pipeline_validation as v
nonce='a'*32;check=dict(source_layout='linear',submissions=24,channel_checks=393216)
data=dict(schema=1,operation='nvn_dynamic_pipeline',uid=10001,source_layout='linear',stage='complete',errno=0,cleanup_errno=0,resources_retained=False,submissions=24,finishes=24,completed_cases=24,recorded_handles=12,released_handles=12,files=24,replays=12,channel_checks=393216,target_offset=4096,nvn_ubo_index=0,original_maxwell_cb=3,passed=True,same_handle_replay=True,cpu_image_conversion=False,legacy_image_template=False,game_modules_executed=False,offline_pixel_oracle_required=False,run_nonce=nonce,run_directory='nvn-pipeline-'+nonce,guard_byte_checks=196608,readonly_byte_checks=491520,maximum_channel_error=0,cases=[])
blobs={}
for i in range(12):
 data['cases'].append(dict(case=i,width=64,height=64,layout=i//4,mode=i%4,payload_bytes=16384,pool_bytes=24576))
 for r in range(2):blobs[v.NAMES[i*2+r]]=b'\xa5'*4096+v.expected(64,64,i%4,r).tobytes()+b'\xa5'*4096
assert v.validate(data,blobs,check,nonce,10001)['passed'];checks=1
for key,val in [('submissions',23),('recorded_handles',11),('released_handles',11),('same_handle_replay',False),('resources_retained',True),('run_nonce','b'*32),('guard_byte_checks',196607),('readonly_byte_checks',1),('maximum_channel_error',2)]:
 d=copy.deepcopy(data);d[key]=val
 try:v.validate(d,blobs,check,nonce,10001)
 except RuntimeError:checks+=1
 else:raise AssertionError(key)
for where in (0,4096,4099,20480):
 b=dict(blobs);x=bytearray(b[v.NAMES[0]]);x[where]^=127;b[v.NAMES[0]]=bytes(x)
 try:v.validate(data,b,check,nonce,10001)
 except RuntimeError:checks+=1
 else:raise AssertionError(where)
b=dict(blobs);del b[v.NAMES[-1]]
try:v.validate(data,b,check,nonce,10001)
except RuntimeError:checks+=1
else:raise AssertionError('missing file')
b=dict(blobs);b[v.NAMES[0]]=b'cat: missing file\n'
try:v.validate(data,b,check,nonce,10001)
except RuntimeError:checks+=1
else:raise AssertionError('error text accepted')
print(json.dumps(dict(passed=True,checks=checks,scope='HOST synthetic oracle/negative metadata only; no GPU execution')))

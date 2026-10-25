"""Independent authored BC endpoint oracle for PUBLIC NVN calls, not a codec."""
import re
from build_bc_sampled_qualification import expected
NAMES=tuple('nvn-pipeline-%02d.raw'%i for i in range(24))
def pixels(case,repeat):
 from bc_srgb_oracle import expected as srgb_expected
 oracle=srgb_expected if case['format']>=0x45 else expected
 return b''.join(oracle(case['format'],((2*x+1)*case['width'])//128,((2*y+1)*case['height'])//128,repeat)for y in range(64)for x in range(64))
def metadata(data,check,nonce,uid):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or re.fullmatch('[0-9a-f]{32}',nonce)is None:raise RuntimeError('ordinary UID/fresh nonce required')
 want=dict(schema=1,operation='nvn_bc_sampled_pipeline',uid=uid,stage='complete',errno=0,cleanup_errno=0,submissions=24,finishes=24,completed_cases=24,recorded_handles=12,released_handles=12,replays=12,files=24,channel_checks=393216,guard_byte_checks=196608,texture_registrations=24,registrations_after_record=24,registered_cpu_object_finalizes=36,binding_array_copies=24,target_bytes=24576,target_offset=4096,target_width=64,target_height=64,target_pitch=256,graphics_control_profile_bytes=2176)
 if type(data)is not dict or any(type(data.get(k))is not type(v)or data[k]!=v for k,v in want.items()):raise RuntimeError('public BC completion/identity/count mismatch')
 for k,v in dict(passed=True,resources_retained=False,same_handle_replay=True,public_nvn=True,cpu_image_conversion=False,game_modules_executed=False).items():
  if type(data.get(k))is not bool or data[k]is not v:raise RuntimeError('public BC lifecycle '+k)
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='nvn-pipeline-'+nonce:raise RuntimeError('public BC nonce/namespace')
 if check.get('kind')!='public-nvn-bc-sampled-qualification-v1' or check.get('submissions')!=24 or check.get('channel_checks')!=393216 or check.get('graphics_control_profile_bytes')!=2176:raise RuntimeError('checked BC qualifier required')
 if data.get('srgb')is not check.get('srgb',False):raise RuntimeError('BC color-space profile mismatch')
 if type(data.get('readonly_byte_checks'))is not int or data['readonly_byte_checks']!=check['readonly_byte_checks']:raise RuntimeError('incomplete readonly pool checks')
 if type(data.get('maximum_channel_error'))is not int or not 0<=data['maximum_channel_error']<=1:raise RuntimeError('pixel tolerance')
 if data.get('cases')!=check['cases'] or any(type(c.get(k))is not int for c in data['cases']for k in c):raise RuntimeError('public BC exact source case inventory')
 return check['cases']
def validate(data,captured,check,nonce,uid):
 cases=metadata(data,check,nonce,uid)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('public BC exact raw inventory')
 channels=guards=maximum=0
 for case in cases:
  for repeat in range(2):
   name=NAMES[2*case['case']+repeat];blob=captured[name]
   if type(blob)is not bytes or len(blob)!=24576:raise RuntimeError('public BC raw size '+name)
   if blob[:4096]!=b'\xa5'*4096 or blob[20480:]!=b'\xa5'*4096:raise RuntimeError('public BC target guard '+name)
   want=pixels(case,repeat);actual=blob[4096:20480]
   for i,(a,b)in enumerate(zip(actual,want)):
    delta=abs(a-b);maximum=max(maximum,delta)
    if delta>(0 if i%4==3 else 1):raise RuntimeError('public BC original shader pixel mismatch '+name+' byte'+str(i))
   channels+=len(actual);guards+=8192
 if channels!=data['channel_checks']or guards!=data['guard_byte_checks']:raise RuntimeError('public BC actual readback counts')
 return dict(passed=True,submissions=24,channel_checks=channels,guard_byte_checks=guards,readonly_byte_checks=data['readonly_byte_checks'],maximum_channel_error=maximum,scope='Actual public NVN BC1 RGB/RGBA, BC2/BC3 selected UNORM or sRGB profile; original bounded a767 profile. No C1 FS, game full-control/frame, GOB RT, 3D or general sampler claim.')

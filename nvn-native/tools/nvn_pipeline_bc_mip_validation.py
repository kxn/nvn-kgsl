"""Independent authored multi-mip endpoint oracle / exact public transport."""
import re
import numpy as np
from bc_mip_oracle import pixels
NAMES=tuple('nvn-pipeline-%02d.raw'%i for i in range(96))
def metadata(data,check,nonce,uid):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or re.fullmatch('[0-9a-f]{32}',nonce)is None:raise RuntimeError('ordinary UID/fresh nonce required')
 want=dict(schema=1,operation='nvn_bc_mip_sampled_pipeline',uid=uid,stage='complete',errno=0,cleanup_errno=0,submissions=96,finishes=96,completed_cases=96,recorded_handles=48,released_handles=48,replays=48,files=96,channel_checks=1572864,guard_byte_checks=786432,texture_registrations=192,registrations_after_record=192,registered_cpu_object_finalizes=240,binding_array_copies=96,target_bytes=24576,target_offset=4096,target_width=64,target_height=64,target_pitch=256,graphics_control_profile_bytes=2176,source_levels=4,inflight_descriptor_updates=96,backing_inflight_busy_checks=96,backing_postfinish_recycles=96,authored_mip_uploads=384,selected_mip_changes=48)
 if type(data)is not dict or any(type(data.get(k))is not type(v)or data[k]!=v for k,v in want.items()):raise RuntimeError('public BC mip completion/identity/count mismatch')
 for k,v in dict(passed=True,resources_retained=False,same_handle_replay=True,public_nvn=True,mixed_unorm_srgb=True,typed_archive=True,all_lods_pixel_qualified=False,cpu_image_conversion=False,game_modules_executed=False).items():
  if type(data.get(k))is not bool or data[k]is not v:raise RuntimeError('public BC mip lifecycle '+k)
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='nvn-pipeline-'+nonce:raise RuntimeError('public BC mip nonce/namespace')
 if check.get('kind')!='public-nvn-bc-mip-sampled-qualification-v1'or check.get('submissions')!=96 or check.get('channel_checks')!=1572864 or check.get('graphics_control_profile_bytes')!=2176 or check.get('source_levels')!=4:raise RuntimeError('checked BC mip qualifier required')
 if type(data.get('readonly_byte_checks'))is not int or data['readonly_byte_checks']!=check['readonly_byte_checks']:raise RuntimeError('incomplete readonly checks')
 if type(data.get('maximum_channel_error'))is not int or not 0<=data['maximum_channel_error']<=1:raise RuntimeError('pixel tolerance')
 if data.get('cases')!=check['cases']or any(type(c.get(k))is not int for c in data['cases']for k in c):raise RuntimeError('exact multi-mip inventory')
 return check['cases']
def validate(data,captured,check,nonce,uid):
 cases=metadata(data,check,nonce,uid)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('public BC mip exact raw inventory')
 channels=guards=maximum=0
 for case in cases:
  for repeat in range(2):
   name=NAMES[2*case['case']+repeat];blob=captured[name]
   if type(blob)is not bytes or len(blob)!=24576:raise RuntimeError('public BC mip raw size '+name)
   if blob[:4096]!=b'\xa5'*4096 or blob[20480:]!=b'\xa5'*4096:raise RuntimeError('public BC mip target guard '+name)
   actual=np.frombuffer(blob[4096:20480],dtype=np.uint8).astype(np.int16).reshape(64,64,4)
   wanted=np.frombuffer(pixels(case,repeat),dtype=np.uint8).astype(np.int16).reshape(64,64,4)
   delta=np.abs(actual-wanted);maximum=max(maximum,int(delta.max()))
   bad=(delta[:,:,:3]>1).any()or(delta[:,:,3]>0).any()
   if bad:raise RuntimeError('public BC mip original shader pixel mismatch '+name)
   channels+=16384;guards+=8192
 if channels!=data['channel_checks']or guards!=data['guard_byte_checks']:raise RuntimeError('public BC mip actual readback counts')
 return dict(passed=True,submissions=96,channel_checks=channels,guard_byte_checks=guards,readonly_byte_checks=data['readonly_byte_checks'],maximum_channel_error=maximum,scope='Actual public NVN ordinary2D four-mip BC1/2/3 UNORM/sRGB nearest/clamp selected base replay and in-flight canonical backing ownership. Bounded a7670x880; no allLOD/mipfilter/array/3D/fullgame claim.')

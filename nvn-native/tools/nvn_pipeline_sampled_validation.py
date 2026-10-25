"""Independent normal-Mesa authored fixture oracle, exact standard transport."""
import re
import numpy as np
NAMES=tuple('nvn-pipeline-%02d.raw'%i for i in range(264))
CASE_KEYS=('case','format','width','height','levels','view','base0','count0','base1','count1')
def compact(cases):return[{k:c[k]for k in CASE_KEYS}for c in cases]
def pixels(case,repeat,patterns):
 row=next(x for x in patterns if x['format']==case['format']);base=case['base'+str(repeat)];w=max(1,case['width']>>base);h=max(1,case['height']>>base)
 x=((2*np.arange(64)+1)*w)//128;y=((2*np.arange(64)+1)*h)//128
 bank=(x[None,:]+2*y[:,None]+3*base+5*repeat)&7
 table=np.asarray([b['expected']for b in row['banks']],dtype=np.uint8)
 return table[bank].tobytes()
def metadata(data,check,nonce,uid):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or re.fullmatch('[0-9a-f]{32}',nonce)is None:raise RuntimeError('ordinary UID/fresh nonce required')
 want=dict(schema=1,operation='nvn_float2d_sampled_pipeline',uid=uid,stage='complete',errno=0,cleanup_errno=0,submissions=264,finishes=264,completed_cases=264,recorded_handles=132,released_handles=132,replays=132,files=264,channel_checks=4325376,guard_byte_checks=2162688,texture_registrations=528,registrations_after_record=528,registered_cpu_object_finalizes=660,binding_array_copies=264,target_bytes=24576,target_offset=4096,target_width=64,target_height=64,target_pitch=256,graphics_control_profile_bytes=2176,source_levels=4,inflight_descriptor_updates=264,backing_inflight_busy_checks=264,backing_postfinish_recycles=264,authored_mip_uploads=1056,selected_mip_changes=132,floating_result_formats=22)
 if type(data)is not dict or any(type(data.get(k))is not type(v)or data[k]!=v for k,v in want.items()):raise RuntimeError('sampled22 completion/identity/count mismatch')
 for k,v in dict(passed=True,resources_retained=False,same_handle_replay=True,public_nvn=True,typed_archive=True,all_lods_pixel_qualified=False,cpu_image_conversion=False,game_modules_executed=False,offline_one_texel_patterns=True,native_mip_filter=False,gather=False).items():
  if type(data.get(k))is not bool or data[k]is not v:raise RuntimeError('sampled22 lifecycle/domain '+k)
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='nvn-pipeline-'+nonce:raise RuntimeError('sampled22 nonce/namespace')
 if check.get('kind')!='public-nvn-float2d-sampled-qualification-v1'or check.get('submissions')!=264 or check.get('channel_checks')!=4325376 or check.get('graphics_control_bytes')!=2176 or check.get('source_levels')!=4:raise RuntimeError('normal sampled22 qualifier required')
 if type(data.get('readonly_byte_checks'))is not int or data['readonly_byte_checks']!=check['readonly_byte_checks']:raise RuntimeError('incomplete sampled22 read-only source/arena/control checks')
 if type(data.get('maximum_channel_error'))is not int or not 0<=data['maximum_channel_error']<=1:raise RuntimeError('sampled22 pixel tolerance')
 if data.get('cases')!=compact(check['cases'])or any(type(c.get(k))is not int for c in data['cases']for k in c):raise RuntimeError('sampled22 exact selected-view matrix')
 return check['cases']
def validate(data,captured,check,nonce,uid):
 cases=metadata(data,check,nonce,uid)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('sampled22 exact raw inventory')
 channels=guards=maximum=0
 for c in cases:
  for repeat in range(2):
   name=NAMES[2*c['case']+repeat];raw=captured[name]
   if type(raw)is not bytes or len(raw)!=24576:raise RuntimeError('sampled22 full raw size '+name)
   if raw[:4096]!=b'\xa5'*4096 or raw[20480:]!=b'\xa5'*4096:raise RuntimeError('sampled22 target guards '+name)
   actual=np.frombuffer(raw[4096:20480],dtype=np.uint8).astype(np.int16);expected=np.frombuffer(pixels(c,repeat,check['patterns']),dtype=np.uint8).astype(np.int16);d=np.abs(actual-expected);maximum=max(maximum,int(d.max()))
   if(d>1).any():raise RuntimeError('sampled22 original shader/format/view pixel mismatch '+name)
   channels+=16384;guards+=8192
 if channels!=data['channel_checks']or guards!=data['guard_byte_checks']:raise RuntimeError('sampled22 full byte/count mismatch')
 return dict(passed=True,submissions=264,full_raw_bytes=6488064,channel_checks=channels,guard_byte_checks=guards,readonly_byte_checks=data['readonly_byte_checks'],maximum_channel_error=maximum,scope='Actual public original-source SAMPLE22 canonical GOB upload/FDL full+selected mip snapshots, late-registration/replay and owner retirement. Float/SNORM signed and sRGB/BGRA authored banks. No allLOD/mipfilter/FETCH/Gather/depth/game claim.')

"""HOST original closed algebra + independent inverse canonical addressing."""
import re,struct
from functools import lru_cache
from public_depth_sampled_oracle import cases,red
from casc_gather_depth_validation import eq,up
NAMES=tuple('nvn-pipeline-%02u.raw'%i for i in range(144))
def layout(fmt,width,height,levels):
 cpp=2 if fmt==50 else 4;bh=0
 while bh<4 and height+height//2>=16<<bh:bh+=1
 offset=0;rows=[]
 for level in range(levels):
  w=max(1,width>>level);h=max(1,height>>level);b=bh
  while b and h<=8<<(b-1):b-=1
  pitch=up(w*cpp,64);size=pitch*up(h,8<<b)
  rows.append(dict(source_offset=offset,source_bytes=size,row_bytes=w*cpp,rows=h,source_pitch=pitch,block_height_log2=b));offset+=size
 return dict(cpp=cpp,payload=offset,pool_bytes=8192+up(offset,4096),level=rows)
def canonical():
 return[dict(case=c['case'],source=layout(c['format'],17<<c['selected_mip'],11<<c['selected_mip'],c['selected_mip']+1),target=layout(52 if c['role']else 37,c['width'],c['height'],1))for c in cases()]
def state_ledger(data,check):
 if check.get('kind')!='public-nvn-depth-sampled-qualification-v1'or check.get('submissions')!=144 or not eq(check.get('cases'),cases())or not eq(check.get('canonical'),canonical()):raise RuntimeError('complete actual canonical72 and normal144 qualification required')
 rows=canonical();want=dict(schema=1,operation='public_depth_sampled',stage='complete',errno=0,cleanup_errno=0,resources_retained=False,submissions=144,finishes=144,completed_cases=144,files=144,recorded_handles=120,released_handles=120,replays=72,texture_registrations=288,registered_cpu_finalizes=360,busy_source_checks=144,copy_wrappers=24,call_wrappers=24,guard_byte_checks=sum(2*(r['target']['pool_bytes']-r['target']['payload'])for r in rows),readonly_byte_checks=sum(2*(122880+2*r['source']['pool_bytes'])for r in rows),bad_case=0xffffffff,public_nvn=True,same_handle_replay=True,source_finalize_before_enqueue=True,late_descriptor_updates=True,cpu_image_conversion=False,game_modules_executed=False,zero_color_draws=72,original_C1_bytes=256,public_depth_mode=0,texture_selector=10,sample_sampler_selector=0,gather_sampler_selector=3,offline_pixel_oracle_required=True,passed=True)
 if type(data)is not dict or any(not eq(data.get(k),v)for k,v in want.items()):
  diff={k:(data.get(k),v)for k,v in want.items()if not eq(data.get(k),v)};raise RuntimeError('actual completion/ownership ledger '+str(diff))
 if type(data.get('cases'))is not list or len(data['cases'])!=72:raise RuntimeError('whole72 leaf cases')
 for c,r,a in zip(cases(),rows,data['cases']):
  expected={**c,'clear_depth_bits':0x3f800000,'target_pool_bytes':r['target']['pool_bytes'],'target_payload':r['target']['payload'],'source_pool_bytes':r['source']['pool_bytes']}
  if not eq(a,expected):raise RuntimeError('actual public state '+str(c['case']))
 return rows
@lru_cache(maxsize=72)
def locations(number):
 import numpy as np
 from texture_conversion_validation import source_addresses
 c=cases()[number];r=canonical()[number]['target'];m=r['level'][0];out=np.empty((c['height'],c['width'],4),dtype=np.int64);seen=set()
 for x,y,at in source_addresses(dict(linear=0,level=[m]),0):
  where=4096+at
  if where in seen or not 4096<=where<4096+r['payload']:raise RuntimeError('inverse canonical bounds/alias')
  seen.add(where);out[y,x//4,x%4]=where
 if len(seen)!=4*c['width']*c['height']:raise RuntimeError('incomplete canonical inverse')
 return out
@lru_cache(maxsize=144)
def expected(number):
 import numpy as np
 c=cases()[number//2];repeat=number%2;r=canonical()[number//2]['target'];at=locations(c['case']);raw=np.full(r['pool_bytes'],0xa5,dtype=np.uint8)
 for y in range(c['height']):
  for x in range(c['width']):
   active=5<=x<c['width']-7 and 3<=y<3+c['height']//2
   if c['role']:word=struct.pack('<f',red(c,x,y,repeat)if active else 1.)
   elif active:word=bytes((int(red(c,x,y,repeat)*255+.5),0,0,255))
   else:continue
   raw[at[y,x]]=list(word)
 return raw

def validate(data,captured,check,nonce,uid):
 import numpy as np
 if type(uid)is not int or uid<10000 or type(nonce)is not str or not re.fullmatch('[0-9a-f]{32}',nonce)or type(data)is not dict or not eq(data.get('uid'),uid)or data.get('run_nonce')!=nonce or data.get('run_directory')!='nvn-pipeline-'+nonce:raise RuntimeError('ordinary UID/fresh nonce required')
 rows=state_ledger(data,check)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('exact144 full raw inventory required')
 channels=pixels=guards=total=0;maximum=0
 for number,name in enumerate(NAMES):
  c=cases()[number//2];r=rows[number//2]['target'];blob=captured[name]
  if type(blob)is not bytes or len(blob)!=r['pool_bytes']:raise RuntimeError('whole target pool '+name)
  actual=np.frombuffer(blob,dtype=np.uint8);want=expected(number);at=locations(c['case']);mask=np.ones(len(blob),dtype=bool)
  if c['role']:mask[at]=False
  else:mask[at[3:3+c['height']//2,5:c['width']-7]]=False
  if not np.array_equal(actual[mask],want[mask]):raise RuntimeError('prefix/tail/GOBpadding/untouched scissor '+name)
  if c['role']:
   if not np.array_equal(actual[at],want[at]):raise RuntimeError('original Gather/C1/quantizeddepth/D32F exact '+name)
   pixels+=c['width']*c['height']
  else:
   delta=np.abs(actual[at].astype(np.int16)-want[at].astype(np.int16));maximum=max(maximum,int(delta.max()))
   if np.any(delta>1):raise RuntimeError('original SAMPLE/R001 channel tolerance '+name)
   channels+=delta.size
  guards+=int(mask.sum());total+=len(blob)
 return dict(passed=True,public_nvn=True,submissions=144,finishes=144,files=144,source_formats=[50,51,52],sample_channel_checks=channels,max_sample_channel_error=maximum,gather_depth_pixels=pixels,Gather_D32F_exact=True,canonical_guard_bytes=guards,readonly_byte_checks=data['readonly_byte_checks'],full_raw_bytes=total,zero_color_draws=72,copy_wrappers=24,call_wrappers=24,same_handle_replays=72,original_C1_bytes=256,source_swizzle='R001',compare_sampler=False,scope='unchanged original SAMPLE/GatherR+FragDepth through public NVN depth-source canonical/selected-mip fixtures; not original NVIDIA oracle, captured D2R bindings, game execution, or Gather component ordering')

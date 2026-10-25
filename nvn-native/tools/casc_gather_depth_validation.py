"""Independent original CASC algebra and public Generic16Bx2 canonical oracle.
HOST reference only. Never supplies converted pixels to the runtime.
"""
import re,struct,math
from functools import lru_cache
from casc_gather_depth_oracle import cases,stored
NAMES=tuple('nvn-pipeline-%02u.raw'%i for i in range(72))
def eq(a,b):
 if type(a)is not type(b):return False
 if type(a)is dict:return set(a)==set(b)and all(eq(a[k],v)for k,v in b.items())
 if type(a)is list:return len(a)==len(b)and all(eq(x,y)for x,y in zip(a,b))
 return a==b
def up(v,a):return (v+a-1)//a*a
def geometry(c):
 cpp=2 if c['format']==50 else 4;w=c['parent_width'];h=c['parent_height'];bh=0
 while bh<4 and h+h//2>=16<<bh:bh+=1
 offset=0;levels=[]
 for l in range(c['levels']):
  lw=max(1,w>>l);lh=max(1,h>>l);b=bh
  while b and lh<=8<<(b-1):b-=1
  pitch=up(lw*cpp,64);size=pitch*up(lh,8<<b)
  levels.append(dict(source_offset=offset,source_bytes=size,row_bytes=lw*cpp,rows=lh,source_pitch=pitch,block_height_log2=b));offset+=size
 return dict(cpp=cpp,target_payload=offset,target_pool_bytes=8192+up(offset,4096),level=levels)
def checked_layout(check):
 rows=check.get('canonical')
 if type(rows)is not list or len(rows)!=36:raise RuntimeError('full36 actual canonical metadata required')
 for c,row in zip(cases(),rows):
  want=geometry(c);want.update({k:c[k]for k in('case','format','width','height','origin','wrap_index','selected_mip','compare','clear_depth_bits')})
  if not eq(row,want):raise RuntimeError('independent actual canonical metadata '+str(c['case']))
 return rows
def state_ledger(data,check):
 """Shared public state/lifetime ledger; HOST RAM completion is not pixels."""
 if check.get('kind')!='public-casc-gather-fragdepth-qualification-v1'or check.get('submissions')!=72:raise RuntimeError('normal original CASC qualification required')
 rows=checked_layout(check)
 want=dict(schema=1,operation='public_casc_gather_depth',stage='complete',errno=0,cleanup_errno=0,resources_retained=False,submissions=72,finishes=72,completed_cases=72,files=72,recorded_handles=60,released_handles=60,replays=36,texture_registrations=144,registered_cpu_finalizes=180,busy_source_checks=72,copy_wrappers=12,call_wrappers=12,guard_byte_checks=sum(2*(r['target_pool_bytes']-r['target_payload'])for r in rows),readonly_byte_checks=72*147456,bad_case=0xffffffff,public_nvn=True,same_handle_replay=True,source_finalize_before_enqueue=True,late_descriptor_updates=True,cpu_image_conversion=False,game_modules_executed=False,zero_color=True,original_C1_bytes=256,public_depth_mode=0,texture_selector=10,sampler_selector=3,offline_pixel_oracle_required=True,passed=True)
 if type(data)is not dict or any(not eq(data.get(k),v)for k,v in want.items()):raise RuntimeError('actual completion/owner/identity ledger')
 ac=data.get('cases')
 if type(ac)is not list or len(ac)!=36:raise RuntimeError('full36 public case states')
 for c,r,a in zip(cases(),rows,ac):
  expected={k:c[k]for k in('case','format','width','height','origin','wrap_index','selected_mip','compare','clear_depth_bits')};expected.update(target_pool_bytes=r['target_pool_bytes'],target_payload=r['target_payload'],source_pool_bytes=12288)
  if not eq(a,expected):raise RuntimeError('actual public state '+str(c['case']))
 return rows
def metadata(data,check,nonce,uid):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or not re.fullmatch('[0-9a-f]{32}',nonce):raise RuntimeError('ordinary UID/fresh nonce')
 if type(data)is not dict or not eq(data.get('uid'),uid)or not eq(data.get('run_nonce'),nonce)or not eq(data.get('run_directory'),'nvn-pipeline-'+nonce):raise RuntimeError('actual completion/owner/identity ledger')
 return state_ledger(data,check)
@lru_cache(maxsize=36)
def locations(n):
 import numpy as np
 from texture_conversion_validation import source_addresses
 c=cases()[n];r=geometry(c);level=c['selected_mip'];m=r['level'][level];out=np.empty((c['height'],c['width'],r['cpp']),dtype=np.int64);seen=set()
 for x,y,at in source_addresses(dict(linear=0,level=[m]),0):
  where=4096+m['source_offset']+at
  if where in seen or not 4096<=where<4096+r['target_payload']:raise RuntimeError('inverse address bounds/alias')
  seen.add(where);out[y,x//r['cpp'],x%r['cpp']]=where
 if len(seen)!=c['width']*c['height']*r['cpp']:raise RuntimeError('inverse address inventory')
 return out
@lru_cache(maxsize=72)
def expected(number):
 import numpy as np
 c=cases()[number//2];repeat=number%2;r=geometry(c);at=locations(c['case']);raw=np.full(r['target_pool_bytes'],0xa5,dtype=np.uint8);z=np.empty((c['height'],c['width']),dtype=np.float64);written=np.zeros(z.shape,dtype=bool)
 for y in range(c['height']):
  for x in range(c['width']):
   z[y,x]=stored(c,x,y,repeat);written[y,x]=z[y,x]!=c['clear_depth']
 if c['format']==52:words=z.astype('<f4').view(np.uint8).reshape(c['height'],c['width'],4)
 else:
  integer=np.floor(z*(65535 if c['format']==50 else 16777215)+.5).astype('<u2'if c['format']==50 else'<u4');words=integer.view(np.uint8).reshape(c['height'],c['width'],r['cpp'])
 raw[at]=words;return raw,z,written

def validate(data,captured,check,nonce,uid):
 import numpy as np
 rows=metadata(data,check,nonce,uid)
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('exact72 full canonical raw inventory')
 pixels=guards=undefined=total=0;maximum=0
 for number,name in enumerate(NAMES):
  c=cases()[number//2];r=rows[number//2];blob=captured[name]
  if type(blob)is not bytes or len(blob)!=r['target_pool_bytes']:raise RuntimeError('complete original caller target pool '+name)
  raw=np.frombuffer(blob,dtype=np.uint8);want,z,written=expected(number);at=locations(c['case']);mask=np.ones(len(raw),dtype=bool);mask[at]=False
  if not np.array_equal(raw[mask],want[mask]):raise RuntimeError('prefix/tail/GOBpadding/nonselectedmip guard '+name)
  active=raw[at].copy();actual=active.reshape(-1).view('<u2'if c['format']==50 else'<u4').reshape(c['height'],c['width'])
  if c['format']==52:
   exp=z.astype('<f4').view('<u4')
   if not np.array_equal(actual,exp):raise RuntimeError('original Gather/C1/D32F exact '+name)
  else:
   if c['format']==51:actual=actual&0xffffff;undefined+=c['width']*c['height']
   exp=np.floor(z*(65535 if c['format']==50 else 16777215)+.5).astype(np.int64);delta=np.abs(actual.astype(np.int64)-exp);maximum=max(maximum,int(delta.max()))
   if np.any(delta[written]>1)or np.any(delta[~written]):raise RuntimeError('original Gather/C1/compare UNORM '+name)
  pixels+=c['width']*c['height'];guards+=int(mask.sum());total+=len(blob)
 return dict(passed=True,public_nvn=True,submissions=72,finishes=72,files=72,depth_pixels=pixels,max_unorm_depth_error=maximum,D32F_exact=True,canonical_guard_bytes=guards,readonly_byte_checks=data['readonly_byte_checks'],undefined_D24X8_active_bytes=undefined,full_raw_bytes=total,zero_color_draws=72,copy_wrappers=12,call_wrappers=12,same_handle_replays=36,clear_depth_endpoints_only=True,original_C1_bytes=256,scope='Unchanged original CASC Gather+FragDepth through actual public NVN with authored R32F/T10/S3/CB6 fixture; not captured D2R Draw or frame; min/max reduction does not prove Gather component ordering.')

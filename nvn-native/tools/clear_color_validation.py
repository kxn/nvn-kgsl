"""Exact full public-pool raw comparison against original Mesa HOST constants."""
import hashlib,re
from pathlib import Path
NAMES=tuple('clear-color-%03u-o%u-p%u-r%u.raw'%(n,o,p,r)for n in range(67)for o in range(2)for p in range(2)for r in range(2))
FORMATS=(1,2,5,6,7,10,13,14,17,18,19,22,37,38,41,42,43,46,154,56,156,63)
def validate_cases(cases):
 if type(cases)is not list or len(cases)!=67:raise ValueError('67 exact clear cases required')
 for n,c in enumerate(cases):
  want=dict(id=n,format=63 if n==66 else FORMATS[n//3],width=2880 if n==66 else 73,height=1620 if n==66 else 41,state=0 if n==66 else n%3)
  if type(c)is not dict or any(type(c.get(k))is not int or c[k]!=v for k,v in want.items()):raise ValueError('clear case identity')
  if any(type(c.get(k))is not int or c[k]<=0 for k in ('cpp','pool_bytes','payload_bytes'))or c['cpp']not in(1,2,4,8,16)or c['pool_bytes']%4096 or c['pool_bytes']!=((65536+c['payload_bytes']+8191)&~4095):raise ValueError('clear storage extent')
  w,h=c['width'],c['height'];rects=[[0,0,w,h],[w//4,h//4,w//2,h//2]]if c['state']==0 else[[3,5,17,9],[10,8,21,17]]if c['state']==1 else[[w-1,h-1,4,4],[w+9,h+3,4,5]]
  if c.get('rects')!=rects:raise ValueError('clear scissor fixture identity')
  if type(c.get('rects'))is not list or len(c['rects'])!=2 or any(type(r)is not list or len(r)!=4 or any(type(v)is not int or v<0 for v in r)for r in c['rects']):raise ValueError('clear rectangles')
  if c.get('masks')!=([15,8]if c['state']==0 else[5,10]if c['state']==1 else[1,15])or any(type(m)is not int for m in c['masks']):raise ValueError('clear masks')
  if c.get('colors')!=[[1,.25,.5,.75],[.5,1,0,.25]]or any(type(v)not in(int,float)for color in c['colors']for v in color):raise ValueError('clear constants')
def validate(data,captured,check,nonce,uid):
 if type(uid)is not int or uid<10000 or re.fullmatch('[0-9a-f]{32}',nonce or'')is None:raise RuntimeError('ordinary UID and nonce required')
 validate_cases(check['cases']);size=sum(c['pool_bytes']*8 for c in check['cases'])
 if type(check.get('expected_bytes'))is not int or check['expected_bytes']!=size:raise RuntimeError('clear expected byte inventory')
 want=dict(schema=1,operation='public_clear_color',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,cases=67,formats=22,origins=2,phases=2,repeats=2,submissions=536,finishes=536,files=536,completed=536,source_public_finalizations=268,CallCommands_calls=268,CopyCommands_calls=0,recorded_texture_finalize_rejections=268,inflight_texture_finalize_rejections=536,data_bytes=size,command_control_readonly_bytes=536*(131072+3*8192),bad_case=0xffffffff,bad_origin=0xffffffff,bad_phase=0xffffffff,bad_repeat=0xffffffff)
 if any(type(data.get(k))is not type(v)or data[k]!=v for k,v in want.items()):raise RuntimeError('clear completion/count mismatch')
 for k,v in dict(passed=True,resources_retained=False,pixel_oracle_on_host=True,nvn_api=True,cpu_image_conversion=False,probe_raw_submit_calls=0,probe_private_encode_calls=0,game_modules_executed=False).items():
  if type(data.get(k))is not type(v)or data[k]!=v:raise RuntimeError('clear lifecycle '+k)
 if data.get('run_nonce')!=nonce or data.get('run_directory')!='clear-color-'+nonce:raise RuntimeError('clear namespace')
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('exact clear full raw inventory')
 for i,name in enumerate(NAMES):
  expected=Path(check['expected'])/name;blob=expected.read_bytes()
  if check['artifacts'].get(str(expected))!=hashlib.sha256(blob).hexdigest():raise RuntimeError('HOST oracle changed')
  actual=captured[name]
  if type(actual)is not bytes or len(actual)!=check['cases'][i//8]['pool_bytes']or len(blob)!=len(actual):raise RuntimeError('clear raw byte length '+name)
  if actual!=blob:
   at=next(i for i,(a,b)in enumerate(zip(actual,blob))if a!=b);raise RuntimeError('%s byte%d actual%02x expected%02x'%(name,at,actual[at],blob[at]))
 return dict(passed=True,exact_byte_checks=size,files=536,formats=22,origin_modes=2,ordinary_and_Call=True,R11_2880x1620=True,CPU_shader_execution=False,game_modules_executed=False)

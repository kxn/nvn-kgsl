"""Independent authored texel-API raw-byte oracle. No runtime conversion."""
from public_copy_validation import spec as public_spec,expected as public_expected
from block_copy_validation import parse_result,typed_equal
import re
IDS=tuple(n for n in range(234)if public_spec(n)['family']<2)
NAMES=tuple(f'texels-{n:03d}-r{k}.bin'for n in range(156)for k in range(2))+('texels-busy.bin',)
def validate_cases(cases):
 if type(cases)is not list or len(cases)!=156:raise RuntimeError('texel case inventory')
 for j,n in enumerate(IDS):
  if not typed_equal(cases[j],public_spec(n)):raise RuntimeError('texel source geometry '+str(j))
 return True
def expected(j,k):return public_expected(IDS[j],k)
def busy_expected():
 b=bytearray([0xa5])*65536;b[512:516]=(0x2468ace0).to_bytes(4,'little');return bytes(b)
def validate(data,captured,nonce,uid,check):
 if type(uid)is not int or uid<10000 or type(nonce)is not str or not re.fullmatch('[0-9a-f]{32}',nonce):raise RuntimeError('texel UID/nonce')
 validate_cases(check.get('cases'))
 exact=dict(schema=1,operation='texture_texels',uid=uid,chip_id=0x06050002,stage='complete',errno=0,cleanup_errno=0,resources_retained=False,cases=156,formats=37,completed=312,public_transfer_calls=312,replays=156,files=313,pool_imports_main=156,pool_releases_main=156,texture_initializations_main=156,texture_finalizations_main=156,buffer_initializations_main=234,noaccess_rejections=2,busy_rejections=2,busy_GPU_word_verified=True,native_submit_attempts=313,native_submit_accepted=313,native_submit_failed=0,native_finish_calls=313,native_finish_completed=313,native_finish_failed=0,native_bad_arguments=0,bad_case=0xffffffff,bad_replay=0xffffffff,bad_pool=0xffffffff,bad_offset=0xffffffffffffffff,nvn_api=True,probe_raw_transfer_calls=0,probe_private_encode_calls=0,read_only_forwarding_observers=True,CPU_visibility_before_caller_invalidate=True,game_shader=False,cpu_image_conversion=False,BC_decode=False,passed=True,run_nonce=nonce,run_directory='texels-'+nonce)
 for key,want in exact.items():
  if not typed_equal(data.get(key),want):raise RuntimeError('texel result '+key)
 stage=data.get('three_d_rejection_stage')
 if type(stage)is not int or stage not in(1,2,3)or not typed_equal(data.get('three_d_rejections'),2 if stage==3 else 1):raise RuntimeError('texel 3D boundary')
 if type(captured)is not dict or set(captured)!=set(NAMES):raise RuntimeError('texel raw inventory')
 total=active=reads=0
 for j in range(156):
  for k in range(2):
   name=NAMES[j*2+k];want,writes,source=expected(j,k);actual=captured[name]
   if type(actual)is not bytes or len(actual)!=len(want):raise RuntimeError('texel raw size '+name)
   if actual!=want:
    offset=next(i for i,(a,b)in enumerate(zip(actual,want))if a!=b);raise RuntimeError(f'{name} byte{offset}: {actual[offset]} != {want[offset]}')
   total+=len(actual);active+=writes;reads+=source
 if type(captured[NAMES[-1]])is not bytes or captured[NAMES[-1]]!=busy_expected():raise RuntimeError('texel actual busy producer/guards')
 counts=dict(data_byte_checks=total,active_byte_checks=active,unchanged_data_byte_checks=total-active,source_readonly_byte_checks=reads,post_cache_stability_byte_checks=total)
 for key,want in counts.items():
  if not typed_equal(data.get(key),want):raise RuntimeError('texel byte count '+key)
 return dict(passed=True,cases=156,formats=37,public_transfer_calls=312,actual_native_submit_accepted=313,actual_native_finish_completed=313,independent_pointer_cases=78,sameBO_disjoint_cases=78,replays=156,raw_files=313,busy_producer_bytes=65536,busy_producer_guard_bytes=65532,byte_tolerance=0,three_d_rejection_stage=stage,**counts,scope='Authored public NVN texel Write/Read/Flush/Invalidate, return-time CPU visibility and guarded busy producer; no sampling/decode/3D execution/game/display claim')

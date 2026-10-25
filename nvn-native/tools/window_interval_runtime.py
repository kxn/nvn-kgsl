"""Private Window-source interval qualification intent; no cadence measurements."""
import hashlib,re,shlex

def requested(record,value):
 if value is None:return None
 if type(record) is not dict:raise RuntimeError('Window interval build record must be an object')
 if type(value) is not int or not 0<=value<=5:raise RuntimeError('Window present interval must be an integer in 0..5')
 # This is a runtime option for exactly the canonical-source Window qualifier.
 # Reject ambiguous profiles before source validation or any ADB invocation.
 if (type(record.get('window_source_check')) is not dict or record['window_source_check'].get('kind')!='native-texture-consumer-bridge-v1' or
     type(record.get('window_source_slots')) is not int or record['window_source_slots'] not in (2,3) or
     record.get('activity')!='org.nvn.nativeprobe.PresentActivity' or
     any(v for k,v in record.items() if k!='window_source_check' and (k.endswith('_check') or k in ('device_inspect','ahb_inspect','iocoherent_inspect','graphics_draw_batch','compute_indirect','nvn_api','texture_modes')))):
  raise RuntimeError('--window-source-present-interval requires only the standard Window source qualifier')
 return value

def payload(nonce,interval):
 if type(nonce) is not str or not re.fullmatch('[0-9a-f]{32}',nonce) or type(interval) is not int or not 0<=interval<=5:raise RuntimeError('invalid Window interval nonce/value')
 return nonce+' '+f'{interval:03d}'

def deploy_config(run,package,nonce,interval):
 target='files/window-source-present-interval'
 if interval is None:
  run('shell','run-as',package,'rm','-f',target);return None
 text=payload(nonce,interval);pending=target+'.'+nonce+'.pending'
 script='set -eu; umask 077; set -C; printf %s '+shlex.quote(text)+' > '+shlex.quote(pending)+'; mv -f '+shlex.quote(pending)+' '+shlex.quote(target)
 run('shell',shlex.join(['run-as',package,'sh','-c',script]))
 expected=hashlib.sha256(text.encode('ascii')).hexdigest()
 response=run('shell','run-as',package,'sha256sum',target).stdout.strip().split()
 if response!=[expected,target]:raise RuntimeError('Window interval private config hash mismatch')
 return dict(requested=interval,sha256=expected,bytes=36,nonce=nonce,path=target)

def validate(probe,interval,slots):
 if interval is None:
  if probe.get('present_interval_configured') is not False or type(probe.get('present_interval')) is not int or probe['present_interval']!=1:raise RuntimeError('default Window profile unexpectedly configured')
  want={'interval_getter_checks':0,'post_enqueue_interval_mutations':0,'platform_snapshot_observations':0}
 else:
  if probe.get('present_interval_configured') is not True or type(probe.get('present_interval')) is not int or probe['present_interval']!=interval:raise RuntimeError('Window interval request/result mismatch')
  want={'interval_getter_checks':3+slots*8,'post_enqueue_interval_mutations':slots*4,'platform_snapshot_observations':slots*4}
 for k,v in want.items():
  if type(probe.get(k)) is not int or probe[k]!=v:raise RuntimeError('Window interval getter/snapshot observation incomplete')
 if probe.get('cadence_measured') is not False:raise RuntimeError('semantic interval test cannot claim measured cadence')
 if interval is not None:
  effective=1 if interval==0 and slots==2 else interval
  stats=probe.get('interval_platform')
  if (type(stats) is not dict or type(stats.get('last')) is not int or stats['last']!=effective or
      type(stats.get('shown')) is not list or len(stats['shown'])!=6 or
      any(type(v) is not int or v!=(slots*4 if k==effective else 0) for k,v in enumerate(stats['shown']))):raise RuntimeError('actual platform show argument counters mismatch')
 return dict(requested=interval,effective=1 if interval is None or interval==0 and slots==2 else interval,
             platform_argument_snapshot_checked=interval is not None,cadence_measured=False,
             forced_worker_interleaving=False,scope='NVN getter and actual platform show arguments; no timing/performance or universal mailbox proof')

#!/usr/bin/env python3
"""One verified Window-source APK, six fresh interval processes. No cadence/FPS claims."""
import argparse,hashlib,json,re,subprocess,sys
from pathlib import Path
from window_interval_runtime import requested

def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--serial',required=True);p.add_argument('--build',type=Path,required=True);p.add_argument('--evidence',type=Path,required=True);p.add_argument('--install',action='store_true');a=p.parse_args()
 record_path=a.build.resolve()/'build-record.json';record=json.loads(record_path.read_text())
 try:requested(record,0)
 except RuntimeError as e:p.error(str(e))
 dest=a.evidence.resolve()
 if a.evidence.exists()or a.evidence.is_symlink():p.error('fresh batch evidence directory required')
 # Each child uses the normal runner: all source/tool/artifact checks run before
 # ADB, and a successful child must prove its own final strict force-stop.
 runner=Path(__file__).with_name('run_android_probe.py').resolve();apk=a.build.resolve()/'probe.apk'
 inputs={str(q):digest(q) for q in (Path(__file__).resolve(),runner,record_path,apk)}
 dest.mkdir(parents=True,exist_ok=False)
 result=dict(schema=1,operation='nvn_window_source_interval_batch',passed=False,
             build=record,inputs=inputs,intervals=list(range(6)),sessions=[],
             cadence_measured=False,gpu_timing_measured=False,game_modules_executed=False)
 def save():(dest/'batch.json').write_text(json.dumps(result,indent=2)+'\n')
 seen=set();save()
 try:
  for interval in range(6):
   if any(digest(q)!=value for q,value in inputs.items()):raise RuntimeError('batch input changed; no cache record repair allowed')
   evidence=dest/f'interval-{interval}.json'
   argv=[sys.executable,str(runner),'--serial',a.serial,'--build',str(a.build.resolve()),'--evidence',str(evidence),'--window-source-present-interval',str(interval)]
   if a.install and interval==0:argv+=['--install']
   child=subprocess.run(argv,capture_output=True)
   row=dict(interval=interval,argv=argv,returncode=child.returncode)
   for stream in ('stdout','stderr'):
    path=dest/f'interval-{interval}-{stream}.log';path.write_bytes(getattr(child,stream));row[stream]=dict(path=str(path),sha256=digest(path))
   result['sessions'].append(row);save()
   if not evidence.is_file():raise RuntimeError('child did not preserve evidence')
   row['evidence']=dict(path=str(evidence),sha256=digest(evidence));save()
   d=json.loads(evidence.read_text())
   if type(d) is not dict:raise RuntimeError('child evidence must be an object')
   if child.returncode or d.get('probe_stopped') is not True or d.get('screen_and_lifecycle_verified') is not True or type(d.get('window_source_present_interval')) is not int or d['window_source_present_interval']!=interval:raise RuntimeError('interval failed or process absence unproved')
   checks=d.get('window_interval_verification')
   if type(checks) is not list or len(checks)!=2 or any(type(c) is not dict or type(c.get('requested')) is not int or c['requested']!=interval or c.get('platform_argument_snapshot_checked') is not True or c.get('cadence_measured') is not False for c in checks):raise RuntimeError('interval two-generation observation absent')
   nonce=d.get('run_nonce')
   if type(nonce) is not str or not re.fullmatch('[0-9a-f]{32}',nonce) or nonce in seen:raise RuntimeError('batch reused a process namespace')
   seen.add(nonce)
  result['passed']=True
 except (OSError,ValueError,RuntimeError)as e:result['error']=str(e)
 finally:save()
 print(json.dumps(dict(passed=result['passed'],evidence=str(dest/'batch.json'),cadence_measured=False)))
 return 0 if result['passed']else 1
if __name__=='__main__':raise SystemExit(main())

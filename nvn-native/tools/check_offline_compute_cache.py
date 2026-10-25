#!/usr/bin/env python3
"""HOST ONLY finite offline compute provider contracts; NDK link is not GPU/game execution."""
import argparse,copy,hashlib,importlib.util,json,os,subprocess,sys,tempfile
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',type=Path,required=True);p.add_argument('--names',type=Path,required=True);p.add_argument('--ndk',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
 p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2]);p.add_argument('--module',type=Path);p.add_argument('--bridge',type=Path);p.add_argument('--fixtures',type=Path)
 a=p.parse_args();root=a.root.resolve();native=root/'nvn-native';out=a.out.resolve();fixture=(a.fixtures or native/'tests/d2r_bridge').resolve();bridge=(a.bridge or native/'adapters/d2r/host_bridge.c').resolve();module_path=(a.module or native/'tools/offline_compute_cache.py').resolve();names=a.names.resolve()
 if not out.is_relative_to(root/'.local') or out.exists():p.error('new ignored --out below ROOT/.local required')
 out.mkdir(parents=True);proof={'scope':__doc__,'passed':False,'runs':[],'rejections':{},'gpu_submits':0};sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 def run(label,argv):
  argv=list(map(str,argv));r=subprocess.run(argv,cwd=root,env=env,text=True,capture_output=True);proof['runs'].append({'label':label,'argv':argv,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
  if r.returncode:raise RuntimeError(label+' failed')
  return r.stdout
 try:
  spec=importlib.util.spec_from_file_location('nn_offline_compute_cache',module_path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
  header,manifest,outputs=m.build(a.check,out/'cache',root=root);proof['cache_manifest']=manifest;proof['cache_outputs']=outputs
  m.build(a.check,out/'cache',root=root,verify=True)
  raw=json.loads(a.check.read_text())
  for kind in ['unknown_chip','empty','stale_checker','stale_frontend','stale_artifact']:
   bad=copy.deepcopy(raw)
   if kind=='unknown_chip':bad['chip_id']=0x06050003
   elif kind=='empty':bad['cases']=[]
   elif kind=='stale_checker':bad['check_source_sha256']='0'*64
   elif kind=='stale_frontend':bad['frontend_build']['sources'][next(iter(bad['frontend_build']['sources']))]='0'*64
   else:bad['ir3_build']['artifacts'][next(iter(bad['ir3_build']['artifacts']))]='0'*64
   path=out/(kind+'.json');path.write_text(json.dumps(bad))
   try:m.build(path,out/('rejected-'+kind),root=root)
   except (ValueError,KeyError,OSError) as e:proof['rejections'][kind]=str(e)
   else:raise RuntimeError('accepted '+kind)
  # A structurally self-consistent old or relabeled truncated control is not
  # a complete caller-supplied identity. Recompute only these negative fixtures.
  validation_spec=importlib.util.spec_from_file_location('truncated_fixture_validator',native/'tools/shader_fixture.py')
  validation=importlib.util.module_from_spec(validation_spec);validation_spec.loader.exec_module(validation)
  for relabel in [False,True]:
   bad=copy.deepcopy(raw);case=bad['cases'][0];original_fixture=Path(case['fixture']);doc=json.loads(original_fixture.read_text())
   for blob in doc['blobs'].values():blob['path']=str((original_fixture.parent/blob['path']).resolve())
   desc=doc['blobs']['control'];data=Path(desc['path']).read_bytes()[desc['offset']:desc['offset']+0x880]
   blob_path=out/('truncated-'+str(relabel)+'.control.bin');blob_path.write_bytes(data)
   desc.update(path=str(blob_path),offset=0,size=len(data),sha256=sha(blob_path))
   if not relabel:doc['profile']={'name':'d2r-embedded-cs-control-0x880'}
   path=out/('truncated-'+str(relabel)+'.fixture.json');path.write_text(json.dumps(doc));case.update(fixture=str(path),fixture_sha256=sha(path),fixture_check=validation.validate(doc,path.parent))
   check_path=out/('truncated-'+str(relabel)+'.check.json');check_path.write_text(json.dumps(bad))
   try:m.build(check_path,out/('rejected-truncated-'+str(relabel)),root=root)
   except ValueError as e:
    if 'truncated/unknown full-control provenance profile' not in str(e):raise
    proof['rejections']['relabeled_truncated' if relabel else 'legacy_truncated']=str(e)
   else:raise RuntimeError('accepted truncated control')
  # Verify cached generated outputs cannot be silently repaired by --verify.
  original=header.read_bytes();header.write_bytes(original+b'\n/* corrupt cache */\n')
  try:
   try:m.build(a.check,out/'cache',root=root,verify=True)
   except ValueError as e:proof['rejections']['generated_header']=str(e)
   else:raise RuntimeError('accepted corrupted generated header')
  finally:header.write_bytes(original)
  # Malformed generated image metadata must fail despite a correspondingly
  # updated bad-check artifact hash. Original compiler outputs are not modified.
  bad=copy.deepcopy(raw);directory=out/'bad-image';directory.mkdir()
  case=bad['cases'][0];prefix=case['fixture_check']['request_sha256'][:16]
  for suffix in case['artifacts']:
   src=a.check.resolve().parent/(prefix+suffix);dst=directory/(prefix+suffix);dst.write_bytes(src.read_bytes())
  path=directory/(prefix+'.native.h');data=path.read_bytes();assert data.count(b'.schema=1,')==1
  path.write_bytes(data.replace(b'.schema=1,',b'.schema=2,'));case['artifacts']['.native.h']=sha(path)
  bad_path=directory/'check.json';bad_path.write_text(json.dumps(bad))
  try:m.build(bad_path,out/'rejected-image',root=root)
  except ValueError as e:
   if 'native image numeric metadata mismatch' not in str(e):raise
   proof['rejections']['native_image_metadata']=str(e)
  else:raise RuntimeError('accepted native image mismatch')
  tool_dir=module_path.parent
  tool_files=[tool_dir/name for name in ['build_d2r_host.py','build_d2r_activity.py','build_d2r_activity_apk.py','run_d2r_host.py']]
  for path in tool_files:compile(path.read_text(),str(path),'exec')
  help_env=dict(env,PYTHONPATH=str(native/'tools'))
  for name in ['build_d2r_host.py','build_d2r_activity.py']:
   r=subprocess.run([sys.executable,str(tool_dir/name),'--help'],cwd=root,env=help_env,capture_output=True,text=True)
   proof['runs'].append({'label':name+' help only','returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
   if r.returncode or '--compute-cache-check' not in r.stdout:raise RuntimeError('missing normal build option')
  def load(path,name):
   sp=importlib.util.spec_from_file_location(name,path);v=importlib.util.module_from_spec(sp);sp.loader.exec_module(v);return v
  cli=load(tool_dir/'run_d2r_host.py','offline_provider_cli_verify');apk=load(tool_dir/'build_d2r_activity_apk.py','offline_provider_apk_verify')
  options={'host_abi':1,'rewrite_abi':'elf','rules':'native','private_gates':'1','abi':'arm64-v8a','execute':False,'deploy':False,'python':sys.executable,'exefs':str(out),'ndk':str(a.ndk),'jobs':1,'compute_cache_check':str(a.check.resolve())}
  host={'options':options,'modules':str(out/'modules'),'artifacts':{str(module_path):sha(module_path)},'sources':{str(module_path):sha(module_path)},'tools':{str(module_path):sha(module_path)}}
  argv=cli.host_verify_argv(host,root);assert argv[-2:]==['--compute-cache-check',str(a.check.resolve())]
  options.update(host_abi=2,rewrite_abi='elfdesc',library_kind='shared')
  argv=apk.host_verify(host,root);assert argv[-2:]==['--compute-cache-check',str(a.check.resolve())]
  proof['normal_builder_checks']={'cli_option':True,'activity_option':True,'cli_verify_option_forwarded':True,'apk_verify_option_forwarded':True,'no_rewrite_build_deploy_executed':True}
  modules=[native/'src'/f for f in ['nvn.c','compute.c','graphics.c','external_image.c','order.c','render_state.c','sampler_state.c','texture_source.c','counter.c']];backend=native/'tests/d2r_bridge/memory_backend.c'
  source_files=[module_path,bridge,Path(__file__).resolve(),fixture/'offline_compute_provider.c',fixture/'offline_compute_forward.c',native/'adapters/d2r/host_bridge.h',native/'src/nvn_entries.inc',backend,native/'tests/d2r_bridge/window_session_backend.inc',*modules,*tool_files,*list((native/'include/nvn_native').glob('*.h')),names,names.parent/'names-record.json']
  proof['sources']={str(f):sha(f) for f in source_files}
  base=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-I'+str(native/'include'),'-I'+str(native/'third_party/nvn'),'-I'+str(native/'adapters/d2r'),'-DNN_D2R_NVN_NAMES_HEADER="'+str(names)+'"','-DNN_NVN_OFFLINE_COMPUTE_HEADER="'+str(header)+'"']
  for cc,flags in [('clang',['-fsanitize=function','-fsanitize-trap=function']),('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
   for kind in ['real','forward']:
    binary=out/(kind+'-'+cc)
    inputs=[bridge,fixture/('offline_compute_provider.c' if kind=='real' else 'offline_compute_forward.c')]
    extra=['-DNVN_TEST_OFFLINE_HEADER="'+str(header)+'"'] if kind=='real' else ['-DNVN_TEST_ENTRY_COUNT='+str(len(manifest['entries']))]
    if kind=='real':inputs += [*modules,backend]
    run(kind+' '+cc+' compile',[cc,*base,*flags,*extra,*inputs,'-o',binary]);run(kind+' '+cc+' execute',[binary])
  ndk=a.ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
  run('NDK real provider strict link only',[ndk,*base,'-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1','-DNVN_TEST_OFFLINE_HEADER="'+str(header)+'"',bridge,fixture/'offline_compute_provider.c',*modules,backend,'-o',out/'real-android-arm64'])
  # Existing scalar test supplies no registration function: OFF must link independently.
  default=[arg for arg in base if not arg.startswith('-DNN_NVN_OFFLINE_COMPUTE_HEADER=')]
  run('default OFF no provider link',['clang',*default,bridge,native/'tests/d2r_bridge/program_observations.c','-o',out/'default-off'])
  run('default OFF execute',[out/'default-off'])
  proof['sources_unchanged']=all(sha(f)==v for f,v in proof['sources'].items());assert proof['sources_unchanged']
  for f,v in manifest['inputs'].items():assert sha(f)==v
  proof['passed']=True
 except Exception as e:proof['error']=str(e)
 finally:(out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n')
 print(json.dumps({'passed':proof['passed'],'scope':'HOST ONLY; no GPU or original game','proof':str(out/'proof.json')}));return 0 if proof['passed'] else 1
if __name__=='__main__':raise SystemExit(main())

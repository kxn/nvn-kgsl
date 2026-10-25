#!/usr/bin/env python3
"""HOST typed RawStorageClass core+adapter checks, real name extraction, NDK static only."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--nso',type=Path,required=True);p.add_argument('--python',type=Path,required=True);p.add_argument('--ndk-clang',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
r=a.root.resolve();n=r/'nvn-native';b=n/'tests/d2r_bridge';out=a.out.resolve();sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
if out.exists() or not out.is_relative_to(r/'.local'):p.error('fresh ignored --out required')
out.mkdir(parents=True);names=out/'names';tool=n/'adapters/d2r/extract_nvn_names.py';bridge=n/'adapters/d2r/host_bridge.c'
helpers=[n/('src/'+x+'.c') for x in ['order','compute','graphics','render_state','sampler_state', 'texture_source', 'texture_bridge','external_image','counter']]
files=[Path(__file__).resolve(),b/'raw_storage.c',b/'texture_metadata.c',a.nso.resolve(),bridge,n/'adapters/d2r/host_bridge.h',n/'tests/d2r_bridge/memory_backend.c',n/'tests/d2r_bridge/window_session_backend.inc',n/'third_party/kgsl/msm_kgsl.h',*helpers,*n.glob('src/*'),*n.glob('include/nvn_native/*'),*n.glob('third_party/nvn/*'),tool,r/'dm/tools/nso.py',r/'dm/gfx/tools/nvn_slots.py',*r.glob('dm/host/*.h')]
files=[f for f in files if f.is_file()]
proof={'scope':'HOST only actual LINEAR resource getter+typed bridge; original NSO decompressed for name metadata only, no module/GPU execution. NDK static only.','passed':False,'inputs':{str(f):sha(f) for f in sorted(set(files))},'runs':[],'gpu_submits':0,'game_modules_executed':False}
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
def run(label,cmd,expected=0):
 q=subprocess.run(cmd,cwd=out,text=True,capture_output=True,env=env);proof['runs'].append(dict(label=label,argv=cmd,exit=q.returncode,stdout=q.stdout,stderr=q.stderr))
 if q.returncode!=expected:raise RuntimeError(label+' unexpected exit '+str(q.returncode))
 return q.stdout
try:
 argv=[str(a.python.resolve()),str(tool),'--nso',str(a.nso.resolve()),'--out',str(names)]
 # Use the explicitly requested interpreter path (not symlink-resolved system
 # Python) so its existing lz4 package and recorded command identity agree.
 argv[0]=str(a.python.absolute())
 if not names.exists():run('original canonical name extraction',argv)
 run('original names --verify',argv+['--verify'])
 meta=json.loads((names/'names-record.json').read_text());header=(names/'names.inc.h').read_text()
 assert meta['names']==523 and meta['public_prototype_names']==505 and meta['typed_forward_names']==504 and meta['static_call_names']==216
 assert 'nvnTextureGetRawStorageClass' not in meta['unprototyped_fatal_boundaries']
 assert 'NN_D2R_NVN_ENTRY(nvnTextureGetRawStorageClass, PFNNVNTEXTUREGETRAWSTORAGECLASSPROC, NVNrawStorageClass, (const NVNtexture* a0 __attribute__((unused))), (a0), VALUE)' in header
 # A modified cached declaration must fail the real verifier before use.
 bad=out/'bad-names';shutil.copytree(names,bad);f=bad/'names.inc.h';f.write_text(f.read_text().replace('(a0), VALUE)\n','(a0), UNKNOWN)\n',1))
 run('tampered cached generated header rejected',[argv[0],str(tool),'--nso',str(a.nso.resolve()),'--out',str(bad),'--verify'],2)
 assert 'cached artifact differs: names.inc.h' in proof['runs'][-1]['stderr']
 common=['-D_GNU_SOURCE','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn'),'-I'+str(r/'nvn-native/adapters/d2r'),'-I'+str(r),'-pthread','-DNN_D2R_HOST_HLE=1','-DNN_D2R_NVN_NAMES_HEADER="'+str(names/'names.inc.h')+'"','-DNN_RAW_CORE="'+str(n/'src/nvn.c')+'"']
 sources=[b/'raw_storage.c',bridge,r/'nvn-native/tests/d2r_bridge/memory_backend.c',*helpers]
 for fixture in ('raw_storage','texture_metadata'):
  for cc,san in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
   exe=out/(fixture+'-'+cc);typed_sources=[b/(fixture+'.c'),*sources[1:]];run(cc+' typed fixture compile',[cc,*common,*san,*map(str,typed_sources),'-lm','-o',str(exe)])
   result=json.loads(run(cc+' typed fixture run',[str(exe)]).strip().splitlines()[-1]);assert result['passed'] and result['gpu_submits']==0
 ndk=str(a.ndk_clang.resolve());run('NDK29 core strict compile',[ndk,*common,'-O2','-c',str(n/'src/nvn.c'),'-o',str(out/'nvn.o')])
 run('NDK29 typed HLE_FORCE fixture strict no-undefined link, NOT executed',[ndk,*common,'-O2','-fPIC','-shared','-Wl,--no-undefined','-DNN_HOST_TEST_WINDOW_SESSION_FAILURE_BACKEND=1',*map(str,sources),'-lm','-o',str(out/'libraw-fixture.so')])
 readelf=str(a.ndk_clang.resolve().parent/'llvm-readelf')
 for f in ['nvn.o','libraw-fixture.so']:
  elf=run('AArch64 '+f,[readelf,'-h',str(out/f)]);assert 'AArch64' in elf and 'ELF64' in elf
 section=run('NDK HLE_FORCE swforce section',[readelf,'-SW',str(out/'libraw-fixture.so')]);assert 'swforce' in section
 run('original names repeat --verify',argv+['--verify'])
 if any(sha(Path(f))!=h for f,h in proof['inputs'].items()):raise RuntimeError('input mutated during checks')
 proof['generated_metadata']={str(f):sha(f) for f in names.iterdir() if f.is_file()}
 proof['passed']=True
except Exception as e:proof['error']=str(e)
proof['artifacts']={str(f):sha(f) for f in out.iterdir() if f.is_file() and f.name!='proof.json'}
(out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps({'passed':proof['passed'],'proof':str(out/'proof.json')}));raise SystemExit(0 if proof['passed'] else 1)

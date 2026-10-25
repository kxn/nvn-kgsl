#!/usr/bin/env python3
"""Actual IR3 half-FRONT_FACE recipes, unchanged old native outputs, HOST/API29 policy. No GPU."""
import argparse,hashlib,importlib.util,json,os,shlex,shutil,subprocess,sys
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def module(p):
 s=importlib.util.spec_from_file_location('ff_normal_compiler',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in('root','ir3','reference-pipelines','reference-stages','ndk','out'):p.add_argument('--'+k,type=Path,required=True)
 p.add_argument('--historical-source-ref',help='Git commit proving changed tracked producer inputs of historical byte references; never requalifies an old deployment record')
 a=p.parse_args();root=a.root.resolve();n=Path(__file__).resolve().parents[1];out=a.out.resolve()
 if out.exists()or not out.is_relative_to(root/'.local'):p.error('fresh ignored output required')
 record,inputs,_,_=module(n/'tools/compute_driverparams.py').compiler_inputs(root,a.ir3.resolve());inputs[str(n/'tools/compute_driverparams.py')]=sha(n/'tools/compute_driverparams.py')
 for path in[Path(__file__).resolve(),n/'shader/pipeline_artifact.cc',n/'src/pipeline_frontend.c',n/'src/render_state.c',n/'tests/d2r_bridge/front_facing.c',n/'tests/front_facing_words.cc',n/'tests/shaders/front_facing.vert',n/'tests/shaders/front_facing.frag',*[p for p in(n/'include/nvn_native').glob('*.h')]]:inputs[str(path)]=sha(path)
 references=[];historical_sources=[];historical_blobs={};historical_commit=None
 if a.historical_source_ref:
  historical_commit=subprocess.check_output(['git','-C',str(root),'rev-parse',a.historical_source_ref+'^{commit}'],text=True).strip()
 for path in(a.reference_pipelines.resolve(),a.reference_stages.resolve()):
  d=json.loads(path.read_text());inputs[str(path)]=sha(path)
  for group in('inputs','artifacts','outputs'):
   for key,h in d.get(group,{}).items():
    actual=sha(key)
    if actual!=h:
     path_key=Path(key).resolve()
     if group!='inputs'or not historical_commit or not path_key.is_relative_to(root/'nvn-native'):
      raise ValueError('historical reference changed '+key)
     rel=str(path_key.relative_to(root))
     if rel not in historical_blobs:
      historical_blobs[rel]=subprocess.check_output(['git','-C',str(root),'show',historical_commit+':'+rel])
     if hashlib.sha256(historical_blobs[rel]).hexdigest()!=h:
      raise ValueError('historical producer source is not proved by pinned git commit: '+key)
     historical_sources.append(dict(reference=str(path),source=key,declared_sha256=h,
       current_sha256=actual,git_commit=historical_commit,git_path=rel,
       scope='Historical byte-reference producer only; not current deployment qualification'))
    inputs[key]=actual
  references.append(d)
 out.mkdir(parents=True);commands=[];proof=dict(schema=1,kind='native-front-facing-family-host-v1',passed=False,gpu_execution=False,android_execution=False)
 for rel,contents in historical_blobs.items():
  dest=out/'historical-producer-source'/rel;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(contents)
 proof['historical_producer_sources']=historical_sources
 def run(argv,cwd=None,ok=True):
  argv=list(map(str,argv));r=subprocess.run(argv,cwd=cwd,capture_output=True,text=True,env=dict(os.environ,IR3_SHADER_DEBUG='asmroundtrip',ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));commands.append(dict(argv=argv,cwd=str(cwd)if cwd else None,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr));(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
  if ok and r.returncode:raise ValueError((r.stdout+r.stderr)[-8000:])
  return r
 try:
  compiler=a.ir3.resolve()/'build/src/freedreno/fdl/nvn-pipeline-ir3';glslc=a.ndk.resolve()/'shader-tools/linux-x86_64/glslc';vs=out/'position.spv'
  for path in(glslc,Path('/usr/bin/spirv-val'),Path(shutil.which('gcc')).resolve(),Path(shutil.which('g++')).resolve(),Path(shutil.which('clang')).resolve()):inputs[str(path)]=sha(path)
  run([glslc,'--target-env=vulkan1.1','--target-spv=spv1.3','-O',n/'tests/shaders/front_facing.vert','-o',vs]);run(['/usr/bin/spirv-val',vs]);authored=[]
  recipes=json.loads((a.ir3.resolve()/'build/compile_commands.json').read_text());template=next(x for x in recipes if 'nvn_pipeline_artifact.cc'in x['file']);cppflags=shlex.split(template['command'])[1:];cppflags=cppflags[:cppflags.index('-MD')];cppflags+=['-I'+str(n/'include'),'-Werror']
  for case in range(5):
   fs=out/('case'+str(case)+'.spv');stem=out/('case'+str(case));run([glslc,'--target-env=vulkan1.1','--target-spv=spv1.3','-O','-DCASE='+str(case),n/'tests/shaders/front_facing.frag','-o',fs]);run(['/usr/bin/spirv-val',fs]);r=run([compiler,vs,fs,stem,0,'0x06050002'],ok=case!=4)
   if case==4:
    if r.returncode!=9:raise ValueError('active SampleID must remain unsupported')
    authored.append(dict(case=case,rejected=True,returncode=9));continue
   meta=json.loads(Path(str(stem)+'.pipeline.json').read_text());face=meta.get('front_facing');link=json.loads(Path(str(stem)+'.link.json').read_text());full=face['max_full_register']if face else link['fs']['max_reg']
   if case!=2 and (not face or face['regid']>=192 or face['max_separate_half_register']!=-1 or not face['half_scalar']or not face['merged_registers']or not face['need_size']):raise ValueError('actual merged half FRONT_FACE facts')
   if case==2 and face:raise ValueError('non-live face became a claimed hardware input')
   binary=out/('words'+str(case));defs=['-DNN_FRONT_FACING_RECIPE='+json.dumps(str(stem)+'.pipeline.h'),'-DNN_FRONT_FACE_PRESENT='+str(int(case!=2)),'-DNN_FRONT_FACE_REGID='+str(face['regid']if face else 252),'-DNN_FRONT_FACE_NEED_SIZE='+str(int(face['need_size'])if face else int(bool(meta['fragcoord_mask']))),'-DNN_FRONT_FACE_FULL='+str(full)]
   run(['g++',*cppflags,*defs,n/'tests/front_facing_words.cc','-o',binary],template['directory']);words=json.loads(run([binary]).stdout);authored.append(dict(case=case,metadata=meta,word_reference=words))
  # Re-run each explicitly recorded original native pair. No new Cartesian
  # pairing, expected-resource answers, fallback selection or source rewriting.
  native_pairs=recipe_pairs=native_compares=recipe_compares=0
  for ri,ref in enumerate(references):
   for index,row in enumerate(ref['commands']):
    argv=row['argv'];name=Path(argv[0]).name
    if name not in('nvn-pipeline-ir3','nvn-graphics-ir3'):continue
    paths=[i for i,x in enumerate(argv)if str(x).endswith('.spv')]
    if len(paths)!=2:continue
    prefix=argv[paths[1]+1];target=out/('reference'+str(ri)+'-'+str(index));current=list(argv);current[0]=str(a.ir3.resolve()/'build/src/freedreno/fdl'/name);current[paths[1]+1]=str(target);r=run(current,ok=False)
    for suffix in('.vs.bin','.fs.bin'):
     old=Path(prefix+suffix)
     if old.exists():
      if not Path(str(target)+suffix).is_file()or sha(old)!=sha(str(target)+suffix):raise ValueError('original native code changed '+str(old))
      native_compares+=1
    native_pairs+=1
    if name=='nvn-pipeline-ir3'and row['returncode']==0:
     if r.returncode:raise ValueError('old successful recipe now rejects')
     for suffix in('.pipeline.h','.pipeline.json','.pipeline.types.json'):
      old=Path(prefix+suffix)
      if old.exists():
       if sha(old)!=sha(str(target)+suffix):raise ValueError('old exact recipe/type metadata changed '+str(old))
       recipe_compares+=1
     recipe_pairs+=1
  if recipe_pairs!=len(references[0]['pairs'])or native_pairs<188:raise ValueError('incomplete existing source regression')
  base=['-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(n/'include'),'-I'+str(n/'third_party/nvn')];host=[]
  for cc,san in[('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all']),('clang',['-fsanitize=function','-fsanitize-trap=function'])]:
   for enabled in(0,1):
    binary=out/(cc+str(enabled));run([cc,*base,*san,'-fno-pie','-no-pie','-DNN_PIPELINE_FRONT_FACING='+str(enabled),n/'tests/d2r_bridge/front_facing.c',n/'src/pipeline_frontend.c',n/'src/render_state.c','-Wl,--gc-sections','-lm','-o',binary]);host.append(dict(compiler=cc,enabled=enabled,actual=json.loads(run([binary]).stdout)))
  arm=a.ndk.resolve()/'toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang'
  for enabled in(0,1):
   flags=[*base,'-fPIC','-DNN_PIPELINE_FRONT_FACING='+str(enabled)];run([arm,*flags,'-shared',n/'src/pipeline_frontend.c',n/'src/render_state.c',n/'src/kgsl.c','-I'+str(n/'third_party/kgsl'),'-Wl,--no-undefined','-landroid','-ldl','-lm','-o',out/('libfront-facing-api29-'+str(enabled)+'.so')])
  for path,h in inputs.items():
   if sha(path)!=h:raise ValueError('source changed during normal check '+path)
  proof.update(passed=True,authored=authored,active_sample_id_rejected=True,old_native_pair_invocations=native_pairs,old_native_bins_identical=native_compares,old_successful_recipe_pairs=recipe_pairs,old_recipe_type_files_identical=recipe_compares,host=host,actual_API29_full_backend_links=2,default_enabled=False,recipe_ABI_changed=False,public_NVNs_or_game_qualified=False)
 except Exception as e:proof['error']=str(e)
 proof.update(inputs=inputs,artifacts={str(p):sha(p)for p in out.rglob('*')if p.is_file()});(out/'check.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(dict(passed=proof['passed'],error=proof.get('error'),check=str(out/'check.json'))));return 0 if proof['passed']else 1
if __name__=='__main__':raise SystemExit(main())

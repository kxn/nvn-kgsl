#!/usr/bin/env python3
"""Real checked-stage union plus conflict/tamper negative tests; no compiler/GPU execution."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
from pathlib import Path


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(root, tool, checks, merged, out):
    root=root.resolve();out=out.resolve()
    if out.exists() or not out.is_relative_to(root/'.local'):
        raise ValueError('fresh ignored test output required')
    out.mkdir(parents=True)
    spec=importlib.util.spec_from_file_location('checked_stage_merge_test',tool)
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    docs=[m.validate_check(root,p)for p in checks]
    case_count=0;negative_count=0
    def ck(value):
        nonlocal case_count
        case_count+=1
        if not value:raise AssertionError('check '+str(case_count))
    def reject(call,fragment):
        nonlocal negative_count
        try:call()
        except (ValueError,OSError,KeyError,TypeError)as error:
            ck(fragment in str(error));negative_count+=1;return
        raise AssertionError('accepted negative '+fragment)
    union=m.merge_documents(checks,docs)
    stage_keys=set().union(*(set(d['stages'])for d in docs))
    pair_keys=set().union(*({(p['vs'],p['fs'])for p in d['pairs']}for d in docs))
    ck(set(union['stages'])==stage_keys)
    ck({(p['vs'],p['fs'])for p in union['pairs']}==pair_keys)
    ck(len(union['commands'])==sum(len(d['commands'])for d in docs))
    ck(sum(p['passed']for p in union['pairs'])==4)
    ck(len(union['inventory']['entries'])==8 and len(stage_keys)==7)
    ck(len(union['pair_selection'])==len(pair_keys)==7)
    ck(len(union['merge_provenance']['checks'])==2)
    # Full reflection/query/envelope and source/native file locations survive.
    for d in docs:
        for key,entry in d['stages'].items():
            preserved=union['stages'][key]
            for field in('stage','envelope','frontend_passed','resources'):
                if field in entry:ck(preserved[field]==entry[field])
            ck(any(o['entry']==entry for o in union['merge_provenance']['stage_origins'][key]))
        for pair in d['pairs']:
            ck(any(o['entry']==pair for o in union['merge_provenance']['pair_origins']['/'.join((pair['vs'],pair['fs']))]))
    # Compare two duplicate stage sets independently of their publication paths.
    duplicate=copy.deepcopy(docs[0]);duplicate_path=out/'duplicate.json';duplicate_path.write_text(json.dumps(docs[0]))
    paths=[checks[0],duplicate_path]
    dedup=m.merge_documents(paths,[docs[0],duplicate]);ck(len(dedup['stages'])==len(docs[0]['stages'])and len(dedup['pairs'])==len(docs[0]['pairs']))
    for field in('chip_id','native_backend_policy','compiler_source_files','frontend_build','ir3_build'):
        d=copy.deepcopy(docs[1])
        if field=='chip_id':d[field]^=1
        elif field=='native_backend_policy':d[field]['unit_render_scale']=not d[field]['unit_render_scale']
        elif field=='compiler_source_files':d[field]+=1
        else:d[field]['test_unrecognized_identity']=True
        reject(lambda d=d:m.merge_documents(checks,[docs[0],d]),'incompatible normal identity: '+field)
    key=next(iter(docs[0]['stages']))
    for field,value in(('frontend_passed',False),('failure','new failure'),('resources',{})):
        d=copy.deepcopy(docs[0]);d['stages'][key][field]=value
        reject(lambda d=d:m.merge_documents(paths,[docs[0],d]),'stage metadata conflict')
    d=copy.deepcopy(docs[0]);entry=d['stages'][key];new_prefix=str(out/'different-prefix');d['artifacts'][new_prefix+'.spv']=d['artifacts'][entry['prefix']+'.spv'];entry['prefix']=new_prefix
    ck(len(m.merge_documents(paths,[docs[0],d])['stages'])==len(docs[0]['stages']))
    d['artifacts'][new_prefix+'.spv']='0'*64
    reject(lambda:m.merge_documents(paths,[docs[0],d]),'stage metadata conflict')
    native_index=next(i for i,p in enumerate(docs[0]['pairs'])if p['passed'])
    d=copy.deepcopy(docs[0]);d['pairs'][native_index]['program']['vs']['binary_bytes']+=4
    reject(lambda:m.merge_documents(paths,[docs[0],d]),'pair native code/metadata conflict')
    d=copy.deepcopy(docs[0]);p=d['pairs'][native_index];old_prefix=p['prefix'];p['prefix']=str(out/'different-native')
    for stage in('vs','fs'):d['artifacts'][p['prefix']+'.'+stage+'.bin']=d['artifacts'][old_prefix+'.'+stage+'.bin']
    ck(len(m.merge_documents(paths,[docs[0],d])['pairs'])==len(docs[0]['pairs']))
    d['artifacts'][p['prefix']+'.vs.bin']='0'*64
    reject(lambda:m.merge_documents(paths,[docs[0],d]),'pair native code/metadata conflict')
    for field in('inputs','artifacts'):
        d=copy.deepcopy(docs[1]);path=next(p for p in docs[0][field]if p in d[field])if any(p in d[field]for p in docs[0][field])else next(iter(docs[0][field]));d[field][path]='0'*64
        reject(lambda d=d:m.merge_documents(checks,[docs[0],d]),'conflicting '+('input'if field=='inputs'else'artifact')+' hash')
    # Full normal verifier negatives: no real artifact/source is changed.
    def document_negative(name,change,fragment):
        p=out/(name+'.json');d=copy.deepcopy(docs[0]);change(d);p.write_text(json.dumps(d))
        reject(lambda:m.validate_check(root,p),fragment)
    document_negative('schema-bool',lambda d:d.update(schema=True),'unknown normal stage')
    document_negative('policy-numeric',lambda d:d['native_backend_policy'].update(unit_render_scale=1),'unknown native backend')
    document_negative('count-bool',lambda d:d.update(compiler_source_files=True),'invalid normal compiler')
    document_negative('input-corrupt',lambda d:d['inputs'].update({next(iter(d['inputs'])):'0'*64}),'changed input')
    document_negative('artifact-corrupt',lambda d:d['artifacts'].update({next(iter(d['artifacts'])):'0'*64}),'changed artifact')
    document_negative('missing-tool',lambda d:d['inputs'].pop(str(root/'nvn-native/tools/offline_graphics_stages.py')),'missing or changed normal stage tool')
    document_negative('stage-status',lambda d:d['stages'][key].update(frontend_passed=1),'invalid stage key/status')
    document_negative('pair-status',lambda d:d['pairs'][0].update(passed=1),'invalid or duplicate pair')
    document_negative('missing-stage',lambda d:d['stages'].pop(key),'inventory stage missing')
    document_negative('pair-metadata',lambda d:d['pairs'][native_index]['program']['vs'].update(binary_bytes=1),'pair metadata differs')
    document_negative('stage-reflection',lambda d:d['stages'][key].update(resources={}),'stage reflection differs')
    p=out/'duplicate-json.json';p.write_text('{"schema":1,"schema":1}')
    reject(lambda:m.validate_check(root,p),'duplicate JSON key')
    reject(lambda:m.build(root,[checks[0],checks[0]],out/'same-input'),'two distinct')
    reject(lambda:m.build(root,checks,root/'invalid-unignored-output'),'dedicated ignored')
    reject(lambda:m.build(root,checks,root/'.local'),'dedicated ignored')
    before=sha(merged/'check.json')
    reject(lambda:m.build(root,checks,merged),'fresh output')
    ck(sha(merged/'check.json')==before)
    # Existing genuine normal merger and existing stage packager are replayed,
    # including their complete inputs/artifacts, instead of synthetic headers.
    _,actual=m.build(root,checks,merged,verify=True)
    ck(actual['created_pairs']==0 and actual['inventory_entries']==8 and actual['unique_stages']==7 and actual['passed_pairs']==actual['packaged_pairs']==4)
    # Consume the actual generated archive through the current C lookup API.
    # This is exact full-source/control/C1 lookup, not shader execution.
    fixture=out/'lookup.c'
    fixture.write_text(r'''#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvn_native/offline_graphics.h"
#include NN_MERGED_STAGE_HEADER
#define C(x) do{if(!(x))return __LINE__;}while(0)
int main(void){
 const struct nn_offline_graphics_archive*a=&nn_offline_graphics;
 C(a->stage_count==7&&a->pair_count==4);
 for(size_t i=0;i<a->stage_count;i++){
  const struct nn_offline_graphics_stage*s=a->stages+i,*found=NULL;
  C(!nn_offline_graphics_find_stage(a,s->stage,s->source,s->source_bytes,s->control,s->control_bytes,s->specialization,&found)&&found==s);
  unsigned char *source=malloc(s->source_bytes),*control=malloc(s->control_bytes);C(source&&control);
  memcpy(source,s->source,s->source_bytes);memcpy(control,s->control,s->control_bytes);
  source[s->source_bytes-1]^=1;
  C(nn_offline_graphics_find_stage(a,s->stage,source,s->source_bytes,control,s->control_bytes,s->specialization,&found)==ENOTSUP&&!found);
  source[s->source_bytes-1]^=1;control[s->control_bytes-1]^=1;
  C(nn_offline_graphics_find_stage(a,s->stage,source,s->source_bytes,control,s->control_bytes,s->specialization,&found)==ENOTSUP&&!found);
  free(source);free(control);
 }
 for(size_t i=0;i<a->pair_count;i++){
  const struct nn_offline_graphics_pair*p=a->pairs+i,*found=NULL;
  C(!nn_offline_graphics_find_pair(a,p->chip,a->stages+p->vs_index,a->stages+p->fs_index,&found)&&found==p);
 }
 puts("HOST full-source/control/C1 lookup7/pair4; shader/GPU execution=false");return 0;
}
''')
    commands=[]
    common=['-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(root/'nvn-native/include'),'-DNN_MERGED_STAGE_HEADER='+json.dumps(str(merged/'stage-archive/offline_graphics_stages.h'))]
    for compiler,flags in [('gcc',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-pie','-no-pie']),('clang',['-fsanitize=function','-fsanitize-trap=function','-fno-pie','-no-pie'])]:
        binary=out/('lookup-'+compiler)
        argv=[compiler,*common,*flags,str(fixture),str(root/'nvn-native/src/offline_graphics.c'),'-o',str(binary)]
        for command in(argv,[str(binary)]):
            result=subprocess.run(command,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
            commands.append(dict(argv=command,returncode=result.returncode,stdout=result.stdout,stderr=result.stderr))
            ck(result.returncode==0)
    (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
    required={str(tool):sha(tool),str(Path(__file__).resolve()):sha(__file__),str(merged/'check.json'):sha(merged/'check.json'),str(merged/'merge-manifest.json'):sha(merged/'merge-manifest.json')}
    for p in checks:required[str(p)]=sha(p)
    proof={'schema':1,'passed':True,'checks':case_count,'negative_cases':negative_count,'gpu_execution':False,'compiler_execution':False,'actual_counts':{k:actual[k]for k in('inventory_entries','unique_stages','attempted_pairs','passed_pairs','packaged_stages','packaged_pairs','created_pairs')},'inputs':required,'artifacts':{str(p):sha(p)for p in out.iterdir()if p.is_file()}}
    (out/'check.json').write_text(json.dumps(proof,indent=2)+'\n')
    return out/'check.json'


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in('root','tool','merged','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--check',type=Path,action='append',required=True);a=p.parse_args()
    print(run(a.root,a.tool.resolve(),[x.resolve()for x in a.check],a.merged.resolve(),a.out))

#!/usr/bin/env python3
"""Check native sampler descriptors/core ownership from current source; no GPU."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--image-header',type=Path,required=True,help='Checked original sampled schema3 graphics header')
    parser.add_argument('--register-headers',type=Path,required=True,help='Actual Mesa build src/freedreno/registers directory')
    parser.add_argument('--mesa-registers',type=Path,required=True,help='Pinned Mesa source src/freedreno/registers, contains gen_header.py/XML')
    parser.add_argument('--native-root',type=Path,default=Path(__file__).resolve().parents[1],help=argparse.SUPPRESS)
    args=parser.parse_args();native=args.native_root.resolve();out=args.out.resolve();ignored=(native.parent/'.local').resolve()
    if out==ignored or not out.is_relative_to(ignored) or out.exists():parser.error('--out must be a new directory below this worktree .local')
    image=args.image_header.resolve();headers=args.register_headers.resolve();mesa=args.mesa_registers.resolve()
    for p in (image,mesa/'gen_header.py',headers/'adreno/a6xx_descriptors.xml.h'):
        if not p.is_file():parser.error('missing input: '+str(p))
    names=('tests/d2r_bridge/sampler_words.c','tests/d2r_bridge/sampler_core.c','tests/d2r_bridge/sampler_metadata.c','tests/d2r_bridge/sampler_aliases.c','tests/d2r_bridge/handle_carriers.c','tests/d2r_bridge/device_modes.c',
           'tests/d2r_bridge/memory_backend.c','src/nvn.c','src/graphics.c','src/sampler_state.c','src/texture_source.c','src/texture_bridge.c','src/compute.c',
           'src/external_image.c','src/order.c','src/render_state.c','src/counter.c')
    files=[native/n for n in names]+list((native/'src').glob('*.inc'))+[native/'tests/d2r_bridge/window_session_backend.inc']+[p for root in (native/'include',native/'third_party/nvn',mesa) for p in root.rglob('*') if p.is_file() and '__pycache__' not in p.parts]+[image,Path(__file__).resolve()]
    inputs={str(p):sha(p) for p in files};out.mkdir(parents=True);runs=[]
    def run(argv,execute=False,variant='generator'):
        env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
        result=subprocess.run(list(map(str,argv)),capture_output=True,text=True,env=env,timeout=120)
        runs.append(dict(argv=list(map(str,argv)),returncode=result.returncode,stdout=result.stdout,stderr=result.stderr,variant=variant))
        (out/'runs.json').write_text(json.dumps(runs,indent=2)+'\n')
        if result.returncode:raise RuntimeError(result.stderr or result.stdout)
        if execute:
            row=json.loads(result.stdout.strip().splitlines()[-1])
            if row.get('passed') is not True:raise RuntimeError('fixture did not report passed')
        return result.stdout
    generated=out/'headers/adreno';generated.mkdir(parents=True);header_hashes={}
    for n in ('adreno_common','a6xx_enums','a6xx_descriptors'):
        text=run([sys.executable,mesa/'gen_header.py','--rnn',mesa,'--xml',mesa/'adreno'/(n+'.xml'),'c-defines'])
        dest=generated/(n+'.xml.h');dest.write_text(text)
        if dest.read_bytes()!=(headers/'adreno'/dest.name).read_bytes():raise RuntimeError('original XML/header mismatch: '+n)
        header_hashes[str(headers/'adreno'/dest.name)]=sha(dest)
    flags=['-std=c11','-O1','-g','-Wall','-Wextra','-Werror']+['-I'+str(p) for p in (native/'include',native/'third_party/nvn',native/'include/nvn_native',native/'src',native/'android',out/'headers')]
    define=lambda n,p:'-D'+n+'="'+str(p)+'"'
    defs=[define('NN_TEST_NVN_SOURCE',native/'src/nvn.c'),define('NN_MODE_FRONTEND_FIXTURE',native/'tests/d2r_bridge/device_modes.c'),define('NN_MODE_IMAGE_HEADER',image)]
    core=[native/'tests/d2r_bridge/sampler_core.c',native/'tests/d2r_bridge/memory_backend.c']+[native/'src'/n for n in ('graphics.c','sampler_state.c','texture_source.c','texture_bridge.c','compute.c','external_image.c','order.c','render_state.c','counter.c')]
    words=[native/'tests/d2r_bridge/sampler_words.c',native/'src/sampler_state.c']
    carrier_defs=[define('NN_NVN_CORE_SOURCE',native/'src/nvn.c'),define('NN_NVN_MEMORY_BACKEND_SOURCE',native/'tests/d2r_bridge/memory_backend.c')]
    carriers=[native/'tests/d2r_bridge/handle_carriers.c',*core[2:]]
    artifacts={}
    for mode,cc,sanitizer in (('gcc-address','gcc','address'),('gcc-undefined','gcc','undefined'),('clang-function','clang','function')):
        compiler=shutil.which(cc)
        if not compiler:raise RuntimeError('missing compiler: '+cc)
        inputs[compiler]=sha(Path(compiler))
        for fixture,sources,extra in (('words',words,[]),('core',core,defs),('carriers',carriers,carrier_defs),('aliases',[native/'tests/d2r_bridge/sampler_aliases.c',*core[2:]],carrier_defs),('metadata',[native/'tests/d2r_bridge/sampler_metadata.c',*core[1:]],defs+['-Wl,--wrap=nn_bo_alloc','-Wl,--wrap=nn_bo_cache','-Wl,--wrap=nn_queue_submit','-Wl,--wrap=nn_queue_submit_waits'])):
            binary=out/(fixture+'-'+mode)
            run([compiler,*flags,*extra,*(['-fsanitize-trap=function'] if sanitizer=='function' else []),'-fsanitize='+sanitizer,'-fno-sanitize-recover=all','-fno-omit-frame-pointer','-fno-pie','-no-pie',*sources,'-o',binary],variant=mode)
            run([binary],True,mode);artifacts[str(binary)]=sha(binary)
    if any(sha(Path(p))!=h for p,h in inputs.items()):raise RuntimeError('source/tool changed during checks')
    proof={'passed':True,'scope':'HOST actual tracked core and original XML packs; no GPU execution','inputs':inputs,'generated_headers':header_hashes,'artifacts':artifacts,'runs_sha256':sha(out/'runs.json')}
    (out/'proof.json').write_text(json.dumps(proof,indent=2)+'\n');print(out/'proof.json')
if __name__=='__main__':main()

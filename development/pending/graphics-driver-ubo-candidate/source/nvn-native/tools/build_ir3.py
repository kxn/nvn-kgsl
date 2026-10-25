#!/usr/bin/env python3
"""Build a host-only IR3 compiler from the pinned Mesa archive and local overlay.

No Gallium or Vulkan driver is built. The archive is never modified. The
dedicated source copy is checked against archive bytes before each build.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile

MESA_SHA = '794b3d1729075dae7ebd47c7283b7ca0cdb5dec668f7f11cc0a5d82f6138a0a5'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mesa-archive', type=Path, required=True)
    parser.add_argument('--meson', type=Path, required=True)
    parser.add_argument('--host-tools', type=Path, required=True,
                        help='prefix containing usr/bin/{bison,flex,m4} and usr/share/bison')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=6)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    rule_path = root / 'shader/ir3_parser_overlay.py'
    spec = importlib.util.spec_from_file_location('nvn_ir3_parser_overlay', rule_path)
    parser_rule = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(parser_rule)
    runtime_rule_path = root / 'shader/ir3_runtime_constants_overlay.py'
    runtime_spec = importlib.util.spec_from_file_location('nvn_ir3_runtime_overlay', runtime_rule_path)
    runtime_rule = importlib.util.module_from_spec(runtime_spec)
    runtime_spec.loader.exec_module(runtime_rule)
    out = args.out.resolve()
    if out == root or out in root.parents or args.jobs < 1:
        parser.error('use a dedicated output directory and positive job count')
    archive = args.mesa_archive.resolve()
    if sha(archive.read_bytes()) != MESA_SHA:
        parser.error('Mesa archive hash mismatch')
    out.mkdir(parents=True, exist_ok=True)
    source_parent = out / 'source'
    source = source_parent / 'mesa-26.2.5'
    overlay = {
        'src/freedreno/ir3/nvn_ir3_compile.c': (root / 'shader/ir3_compile.c').read_bytes(),
        'src/freedreno/ir3/nvn_cs_artifact.cc': (root / 'shader/cs_artifact.cc').read_bytes(),
        'src/freedreno/ir3/nvn_spirv_ir3.c': (root / 'shader/spirv_ir3.c').read_bytes(),
        'src/freedreno/ir3/nvn_compute_artifact.cc': (root / 'shader/compute_artifact.cc').read_bytes(),
        'src/freedreno/ir3/nvn_compute_image.h': (root / 'include/nvn_native/compute_image.h').read_bytes(),
        'src/freedreno/ir3/nvn_nir_bridge.h': (root / 'shader/nir_bridge.h').read_bytes(),
        'src/freedreno/ir3/nvn_graphics_ir3.c': (root / 'shader/graphics_ir3.c').read_bytes(),
        'src/freedreno/ir3/nvn_graphics_artifact.cc': (root / 'shader/graphics_artifact.cc').read_bytes(),
        'src/freedreno/ir3/nvn_graphics_image.h': (root / 'include/nvn_native/graphics_image.h').read_bytes(),
        'src/freedreno/ir3/nvn_pipeline_ir3.c': (root / 'shader/pipeline_ir3.c').read_bytes(),
        'src/freedreno/ir3/nvn_pipeline_artifact.cc': (root / 'shader/pipeline_artifact.cc').read_bytes(),
        'src/freedreno/ir3/nvn_pipeline_recipe.h': (root / 'include/nvn_native/pipeline_recipe.h').read_bytes(),
        'src/freedreno/ir3/nvn_pipeline_platform.cc': (root / 'shader/pipeline_platform.cc').read_bytes(),
        'src/freedreno/ir3/nvn_pipeline_platform.h': (root / 'include/nvn_native/pipeline_platform.h').read_bytes(),
    }
    manifest = {}
    # Mesa's Python generators may leave bytecode beside their inputs. Remove
    # generated caches from this dedicated copy before checking pristine bytes.
    for cache in source.rglob('__pycache__'):
        if cache.is_symlink():
            parser.error('unexpected cache symlink in isolated source')
        shutil.rmtree(cache)
    with tarfile.open(archive) as tar:
        members = tar.getmembers()
        if any(not m.name.startswith('mesa-26.2.5/') and m.name != 'mesa-26.2.5' for m in members):
            parser.error('unexpected archive root')
        if not source.exists():
            tar.extractall(source_parent, filter='data')
        for member in members:
            if member.issym():
                relative = member.name.removeprefix('mesa-26.2.5/')
                path = source / relative
                if not path.is_symlink() or os.readlink(path) != member.linkname:
                    parser.error('isolated source symlink changed: ' + relative)
                manifest[relative] = sha(('symlink:' + member.linkname).encode())
                continue
            if not member.isfile():
                continue
            relative = member.name.removeprefix('mesa-26.2.5/')
            original = tar.extractfile(member).read()
            expected = original
            if relative == 'src/freedreno/fdl/meson.build':
                expected += b'\n' + (root / 'shader/mesa-tool.meson').read_bytes()
                overlay[relative] = expected
            elif relative == 'src/compiler/nir/meson.build':
                # Upstream otherwise selects nir_stub.c when all drivers are
                # disabled, even for an explicit standalone NIR tool build.
                old = b'with_nir_headers_only = not with_gfx_compute or ('
                new = b"with_nir_headers_only = not (with_gfx_compute or with_tools.contains('nir')) or ("
                if original.count(old) != 1:
                    parser.error('NIR standalone build rule no longer matches pinned source')
                expected = original.replace(old, new)
                overlay[relative] = expected
            elif relative == 'src/freedreno/ir3/ir3_lexer.l':
                expected = parser_rule.patch(original)
                overlay[relative] = expected
            if relative in runtime_rule.FILES:
                expected = runtime_rule.patch(relative, expected)
                overlay[relative] = expected
            path = source / relative
            if relative not in overlay and (not path.is_file() or path.read_bytes() != expected):
                parser.error('isolated source changed outside overlay: ' + relative)
            manifest[relative] = sha(expected)
    for name, data in overlay.items():
        (source / name).write_bytes(data)
        manifest[name] = sha(data)
    # Reject extra files so an unrecorded source cannot enter the dependency set.
    extras = [str(p.relative_to(source)) for p in source.rglob('*')
              if (p.is_file() or p.is_symlink()) and str(p.relative_to(source)) not in manifest]
    if extras:
        parser.error('unrecorded files in isolated source: ' + repr(extras[:5]))
    env = os.environ.copy()
    host = args.host_tools.resolve()
    env['PATH'] = str(host / 'usr/bin') + ':' + str(args.meson.resolve().parent) + ':' + env['PATH']
    env['BISON_PKGDATADIR'] = str(host / 'usr/share/bison')
    env['M4'] = str(host / 'usr/bin/m4')
    env['PYTHONDONTWRITEBYTECODE'] = '1'
    commands = []

    def run(*command):
        argv = list(map(str, command))
        commands.append(argv)
        subprocess.run(argv, check=True, env=env)

    build = out / 'build'
    setup = ['setup', str(build), str(source)]
    if (build / 'build.ninja').exists():
        setup.append('--reconfigure')
    run(args.meson.resolve(), *setup, '-Dgallium-drivers=', '-Dvulkan-drivers=',
        '-Dplatforms=', '-Dtools=freedreno,nir', '-Dfreedreno-kmds=kgsl',
        '-Dopengl=false', '-Degl=disabled', '-Dglx=disabled', '-Dllvm=disabled',
        '-Dbuild-tests=false', '-Dbuildtype=debugoptimized', '--wrap-mode=nofallback')
    targets = ['src/freedreno/fdl/nvn-ir3-compile', 'src/freedreno/fdl/nvn-spirv-ir3', 'src/freedreno/fdl/nvn-graphics-ir3', 'src/freedreno/fdl/nvn-pipeline-ir3', 'src/freedreno/fdl/nvn-pipeline-platform']
    run('ninja', '-C', build, '-j', str(args.jobs), *targets)
    binaries = [build / target for target in targets]
    tool_paths = [args.meson.resolve(), host / 'usr/bin/bison', host / 'usr/bin/flex', host / 'usr/bin/m4']
    tool_paths += [Path(shutil.which(p)) for p in ('cc', 'c++', 'ninja')]
    source_record = json.dumps(manifest, sort_keys=True, indent=2) + '\n'
    (out / 'source-hashes.json').write_text(source_record)
    record = {'mesa_archive_sha256': MESA_SHA, 'source_manifest_sha256': sha(source_record.encode()),
              'builder_sha256': sha(Path(__file__).read_bytes()),
              'source_rules': {str(rule_path.relative_to(root)): sha(rule_path.read_bytes()), str(runtime_rule_path.relative_to(root)): sha(runtime_rule_path.read_bytes())},
              'runtime_constants_profile': runtime_rule.PROFILE,
              'parser_profile': parser_rule.PROFILE,
              'overlay': {k: sha(v) for k, v in overlay.items()}, 'commands': commands,
              'tools': {str(p): sha(p.read_bytes()) for p in tool_paths},
              'artifacts': {str(binary): sha(binary.read_bytes()) for binary in binaries},
              'scope': 'host-only compiler; no device, game or GPU execution'}
    (out / 'build-record.json').write_text(json.dumps(record, indent=2) + '\n')
    for binary in binaries:
        print(binary)


if __name__ == '__main__':
    main()

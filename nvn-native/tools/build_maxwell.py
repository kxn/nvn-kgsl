#!/usr/bin/env python3
"""Build only the vendored shader translator and its host dependencies."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET
from workspace_paths import shader_frontend_root, checkout_identity


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--shader-frontend-root', type=Path, help='Pinned public shader frontend checkout; or NVN_SHADER_FRONTEND_ROOT')
    parser.add_argument('--dotnet', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    project = root / 'nvn-native/shader/maxwell/Maxwell.csproj'
    try:
        frontend = shader_frontend_root(args.shader_frontend_root)
    except ValueError as exc:
        parser.error(str(exc))
    source_roots = {'native': root, 'shader_frontend': frontend}
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    projects, sources = set(), {}

    def record(path):
        namespace = 'native' if path.is_relative_to(root) else 'shader_frontend'
        sources[namespace + '/' + str(path.relative_to(source_roots[namespace]))] = sha(path.read_bytes())

    def visit(path):
        path = path.resolve()
        if path in projects:
            return
        projects.add(path)
        for source in path.parent.rglob('*'):
            if source.is_file() and not {'obj', 'bin', '__pycache__'}.intersection(source.relative_to(path.parent).parts):
                record(source)
        parent = path.parent
        source_root = root if path.is_relative_to(root) else frontend
        while parent.is_relative_to(source_root):
            for name in ('Directory.Build.props', 'Directory.Build.targets', 'Directory.Packages.props',
                         'NuGet.Config', 'nuget.config', 'global.json'):
                if (parent / name).is_file():
                    record(parent / name)
            parent = parent.parent
        for ref in ET.parse(path).iter('ProjectReference'):
            visit(path.parent / ref.attrib['Include'].replace('$(NvnShaderFrontendRoot)', str(frontend)).replace('\\', '/'))

    visit(project)
    if {p.parent.name for p in projects} != {'maxwell', 'Ryujinx.Graphics.Shader', 'Ryujinx.Common', 'Spv.Generator'}:
        parser.error('unexpected project dependency closure')
    record(Path(__file__).resolve())
    record(root / 'nvn-native/tools/workspace_paths.py')
    overlay_dir = out / 'overlay'
    overlay_dir.mkdir(exist_ok=True)
    spec = importlib.util.spec_from_file_location('iadd_overlay', project.parent / 'iadd_overlay.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    original = frontend / 'src/Ryujinx.Graphics.Shader/Instructions/InstEmitIntegerArithmetic.cs'
    patched = overlay_dir / original.name
    patched.write_bytes(module.patch(original.read_bytes()))
    argv = [str(args.dotnet.resolve()), 'build', str(project), '-c', 'Release',
            '--artifacts-path', str(out / 'artifacts'), '-p:NuGetAudit=false',
            '-p:CustomAfterMicrosoftCommonTargets=' + str(project.parent / 'CompilerOverlay.targets'),
            '-p:NvnShaderFrontendRoot=' + str(frontend), '-p:NvnShaderOverlay=' + str(overlay_dir), '--verbosity', 'quiet']
    subprocess.run(argv, check=True)
    binary = out / 'artifacts/bin/Maxwell/release/Maxwell.dll'
    # Hash all runtime files, including transitive NuGet dependencies. Assets
    # manifests record selected versions and content hashes of restored packages.
    artifacts = {str(p): sha(p.read_bytes()) for p in binary.parent.rglob('*') if p.is_file()}
    assets = {str(p): sha(p.read_bytes()) for p in (out / 'artifacts/obj').rglob('project.assets.json')}
    source_record = json.dumps(sources, sort_keys=True, indent=2) + '\n'
    (out / 'source-hashes.json').write_text(source_record)
    result = {'schema': 1, 'scope': 'host Maxwell shader frontend only; no CPU emulator or graphics runtime',
              'sources': sources, 'source_manifest_sha256': sha(source_record.encode()),
              'source_roots': {k: str(v) for k, v in source_roots.items()},
              'source_checkouts': {k: checkout_identity(v) for k,v in source_roots.items()},
              'projects': sorted(('native/' + str(p.relative_to(root))) if p.is_relative_to(root) else ('shader_frontend/' + str(p.relative_to(frontend))) for p in projects),
              'commands': [argv], 'dotnet_sha256': sha(args.dotnet.read_bytes()),
              'dotnet_info': subprocess.check_output([str(args.dotnet), '--info'], text=True),
              'entrypoint': str(binary), 'artifacts': artifacts, 'restore_assets': assets}
    result['compiler_overlay'] = {str(patched): sha(patched.read_bytes()), 'original_sha256': sha(original.read_bytes())}
    (out / 'build-record.json').write_text(json.dumps(result, indent=2) + '\n')
    print(binary)


if __name__ == '__main__':
    main()

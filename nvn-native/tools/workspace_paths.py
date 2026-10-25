"""Separate the standalone library from optional external validation/tool sources."""
import os
from pathlib import Path
import subprocess


def repository_root():
    return Path(__file__).resolve().parents[2]


def d2r_root(value=None):
    """Require an explicit external validation checkout; retain legacy in-tree use."""
    selected = value or os.environ.get('D2R_ROOT')
    if selected is None and (repository_root() / 'dm/host/hle.h').is_file():
        selected = repository_root()
    if selected is None:
        raise ValueError('optional D2R integration requires --d2r-root or D2R_ROOT; the standalone backend does not require D2R')
    path = Path(selected).expanduser().resolve()
    for name in ('dm/host/hle.h', 'dm/tools/nso.py', 'dm/gfx/tools/nvn_slots.py'):
        if not (path / name).is_file():
            raise ValueError('invalid external D2R checkout: missing ' + str(path / name))
    return path


def shader_frontend_root(value=None):
    """Resolve only the public shader compiler checkout, independent of D2R."""
    selected = value or os.environ.get('NVN_SHADER_FRONTEND_ROOT')
    legacy = repository_root() / 'dm/third_party/ryujinx'
    if selected is None and legacy.is_dir():
        selected = legacy
    if selected is None:
        raise ValueError('shader translation requires --shader-frontend-root or NVN_SHADER_FRONTEND_ROOT pointing to the pinned public shader frontend')
    path = Path(selected).expanduser().resolve()
    project = path / 'src/Ryujinx.Graphics.Shader/Ryujinx.Graphics.Shader.csproj'
    if not project.is_file():
        raise ValueError('invalid shader frontend checkout: missing ' + str(project))
    return path


def checkout_identity(path):
    """Record each checkout separately; file manifests remain the source authority."""
    path = Path(path).resolve()
    result = subprocess.run(['git', '-C', str(path), 'rev-parse', 'HEAD'], capture_output=True, text=True)
    return {'path': str(path), 'head': result.stdout.strip() if result.returncode == 0 else None}

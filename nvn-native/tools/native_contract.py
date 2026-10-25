"""Build metadata for the independently declared partial NVN ABI dialect."""
import hashlib
import re


def native_contract(native):
    header = native / 'include/nvn_native/profile.h'
    text = header.read_text()
    profile = re.search(r'^#define NN_NVN_ABI_PROFILE "([a-z0-9.-]+)"$', text, re.M)
    major = re.search(r'^#define NN_NVN_ABI_MAJOR ([0-9]+)$', text, re.M)
    minor = re.search(r'^#define NN_NVN_ABI_MINOR ([0-9]+)$', text, re.M)
    if not all((profile, major, minor)):
        raise ValueError('missing declared native NVN partial-ABI identity')
    sources = ('include/nvn_native/profile.h', 'third_party/nvn/nvn.h',
               'src/nvn.c', 'src/nvn_entries.inc')
    return {'schema': 1, 'profile': profile[1],
            'reported_api': {'major': int(major[1]), 'minor': int(minor[1])},
            'identity_kind': 'project_declared_partial_abi', 'partial': True,
            'strict_unsupported': True, 'original_driver_version_measured': False,
            'unsupported_device_properties': 'ENOTSUP; caller output unchanged',
            'sources': {name: hashlib.sha256((native / name).read_bytes()).hexdigest()
                        for name in sources}}

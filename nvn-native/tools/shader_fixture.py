#!/usr/bin/env python3
"""Validate bounded shader inputs before translation; never infer missing state.

This is a fixture contract, not a shader decoder or NVN control parser.
Blob hashes cover selected bytes; offsets are relative to the input file.
The request key includes explicit state and compiler/profile versions, not paths.
"""
import argparse
import hashlib
import json
from pathlib import Path


def digest(data):
    return hashlib.sha256(data).hexdigest()


def validate(document, directory):
    required = {'schema', 'stage', 'profile', 'compiler', 'blobs', 'state', 'provenance'}
    if set(document) != required or document['schema'] != 1:
        raise ValueError('expected schema 1 with exactly: ' + ', '.join(sorted(required)))
    if document['stage'] not in ('vs', 'tcs', 'tes', 'gs', 'fs', 'cs'):
        raise ValueError('unsupported shader stage')
    for name in ('profile', 'compiler', 'provenance'):
        if not isinstance(document[name], dict) or not document[name]:
            raise ValueError(name + ' must be a nonempty object')
    # Compiler inputs must be explicit, including unsupported/unresolved facts.
    state = document['state']
    if not isinstance(state, dict) or set(state) != {'resources', 'graphics', 'compute', 'host', 'precision'}:
        raise ValueError('state requires resources, graphics, compute, host, precision')
    for name in ('resources', 'host', 'precision'):
        if not isinstance(state[name], dict):
            raise ValueError(name + ' must be an object')
    if document['stage'] == 'cs':
        c = state['compute']
        if not isinstance(c, dict) or set(c) != {'local_size', 'shared_bytes', 'local_bytes'}:
            raise ValueError('compute requires local_size, shared_bytes, local_bytes')
        local = c['local_size']
        if not isinstance(local, list) or len(local) != 3 or any(type(x) is not int or x <= 0 for x in local):
            raise ValueError('local_size must contain three positive integers')
        if any(type(c[k]) is not int or c[k] < 0 for k in ('shared_bytes', 'local_bytes')):
            raise ValueError('memory sizes must be nonnegative integers')
        if state['graphics'] is not None:
            raise ValueError('compute fixture must not contain graphics state')
    elif state['compute'] is not None or not isinstance(state['graphics'], dict):
        raise ValueError('graphics fixture requires graphics state and null compute')
    blobs = document['blobs']
    if not isinstance(blobs, dict) or set(blobs) != {'code', 'control', 'c1'}:
        raise ValueError('code, control and c1 must be explicitly supplied')
    normalized = {}
    for name, desc in blobs.items():
        if not isinstance(desc, dict) or set(desc) != {'path', 'offset', 'size', 'sha256'}:
            raise ValueError(name + ': expected path, offset, size, sha256')
        offset, size = desc['offset'], desc['size']
        if type(offset) is not int or type(size) is not int or offset < 0 or size < 0:
            raise ValueError(name + ': invalid range')
        if size > 16 * 1024 * 1024 or (name != 'c1' and size == 0):
            raise ValueError(name + ': empty or excessive input')
        path = (directory / desc['path']).resolve()
        if offset + size > path.stat().st_size:
            raise ValueError(name + ': range exceeds file')
        with path.open('rb') as stream:
            stream.seek(offset)
            data = stream.read(size)
        actual = digest(data)
        if len(data) != size or actual != desc['sha256']:
            raise ValueError(name + ': input hash mismatch')
        normalized[name] = {'size': size, 'sha256': actual}
    if normalized['code']['size'] % 8:
        raise ValueError('Maxwell input must be aligned to 64-bit words')
    # Provenance is audited separately; its labels do not change compiled code.
    request = {k: document[k] for k in ('schema', 'stage', 'profile', 'compiler', 'state')}
    request['blobs'] = normalized
    key = digest(json.dumps(request, sort_keys=True, separators=(',', ':'), allow_nan=False).encode())
    return {'schema': 1, 'request_sha256': key, 'inputs': normalized,
            'scope': 'bounded fixture validation only; shader semantics and GPU execution not validated'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fixture', type=Path)
    args = parser.parse_args()
    try:
        result = validate(json.loads(args.fixture.read_text()), args.fixture.resolve().parent)
    except (ValueError, TypeError, KeyError, OSError) as error:
        parser.exit(1, f'fixture rejected: {error}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

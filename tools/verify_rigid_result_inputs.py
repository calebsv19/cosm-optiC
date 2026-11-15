#!/usr/bin/env python3
"""Check the explicit BR4 development source binding; never refresh it implicitly."""
import argparse
import hashlib
import json
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--core-scene', type=Path, required=True)
p.add_argument('--cjson', type=Path, required=True)
p.add_argument('--manifest', type=Path, default=Path(__file__).with_name('rigid_result_shared_inputs.json'))
a = p.parse_args()
manifest = json.loads(a.manifest.read_text())
for name, wanted in manifest['files'].items():
    if hashlib.sha256((a.core_scene / name).read_bytes()).hexdigest() != wanted:
        raise SystemExit('Rigid result shared source drift requires review: ' + name)
if hashlib.sha256(a.cjson.read_bytes()).hexdigest() != manifest['cjson_sha256']:
    raise SystemExit('Rigid physical projection serializer drift requires review')
print('Rigid result shared input parity passed: core_scene ' + manifest['version'])

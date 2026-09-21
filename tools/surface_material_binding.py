#!/usr/bin/env python3
"""Prepare a retained M3 scene binding from a digest-pinned mapping/document pair.

Writes a new candidate only. The renderer's preflight is the final capability gate.
External source documents remain immutable inputs; no graph is flattened.
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
from pathlib import Path

KINDS = {'layer_graph', 'solid_graph', 'authored_manifest', 'surface_authoring_document'}


def load(path: Path):
    data = path.read_bytes()
    return json.loads(data), hashlib.sha256(data).hexdigest()


def bind(scene, object_id, mapping_path, document_path, kind, expected_document, expected_mapping):
    if kind not in KINDS:
        raise ValueError('unsupported source document kind')
    mapping, mapping_digest = load(mapping_path)
    document, document_digest = load(document_path)
    if document_digest != expected_document or mapping_digest != expected_mapping:
        raise ValueError('stale source document or mapping digest')
    if kind == 'surface_authoring_document':
        if document.get('schema') != 'ray_tracing.surface_authoring_document':
            raise ValueError('wrong surface authoring document schema')
        reference = document.get('surface_mapping', {})
        mapping_id = reference.get('id')
        if reference.get('digest_sha256') != mapping_digest:
            raise ValueError('surface document mapping reference is stale')
        if document.get('source_object_id') != object_id:
            raise ValueError('surface document belongs to another object')
    else:
        mapping_id = document.get('surface_mapping_ref')
    if not isinstance(mapping_id, str) or not mapping_id or len(mapping_id.encode()) >= 64:
        raise ValueError('source document requires a stable surface mapping reference')
    candidate = copy.deepcopy(scene)
    objects = [obj for obj in candidate.get('objects', []) if obj.get('object_id') == object_id]
    if len(objects) != 1:
        raise ValueError('object reference is missing or ambiguous')
    rows = candidate.setdefault('extensions', {}).setdefault('ray_tracing', {}).setdefault('authoring', {}).setdefault('object_materials', [])
    matches = [row for row in rows if row.get('object_id') == object_id]
    if len(matches) != 1:
        raise ValueError('requires exactly one retained material source row')
    row = matches[0]
    if kind == 'layer_graph':
        if not document.get('nodes'):
            raise ValueError('empty layer graph')
        # Explicit adapter invocation adopts the source graph as the editable source.
        row.pop('material_texture_stack', None)
        row.pop('materialTextureStack', None)
        row.pop('materialGraph', None)
        row['material_graph'] = document
    elif kind == 'solid_graph':
        reference = objects[0].get('procedural_solid_material_ref', {})
        if objects[0].get('object_type') != 'mesh_asset_instance' or not reference.get('binding_path') or not reference.get('authored_binding_path'):
            raise ValueError('solid graph requires an existing mesh region and authored-material binding')
        reference['graph_path'] = str(document_path.resolve())
    elif kind == 'authored_manifest':
        if document.get('source_object_id') != object_id:
            raise ValueError('authored manifest belongs to another object')
        row['authored_texture'] = {'manifest_path': str(document_path.resolve()), 'binding_mode': 'override'}
    binding = row.setdefault('surface_material_binding', {'version': 1, 'required_capability': 'optic.surface_material_v3'})
    if binding.get('version') != 1 or binding.get('required_capability') != 'optic.surface_material_v3':
        raise ValueError('unsupported existing binding')
    maps = binding.setdefault('mappings', [])
    item = {'id': mapping_id, 'definition': mapping, 'path': str(mapping_path.resolve()), 'sha256': mapping_digest}
    prior = [entry for entry in maps if entry.get('id') == mapping_id]
    if prior and (len(prior) != 1 or prior[0] != item):
        raise ValueError('mapping identity already has a different definition')
    if not prior:
        maps.append(item)
    resources = binding.setdefault('source_documents', [])
    resource = {'kind': kind, 'path': str(document_path.resolve()), 'sha256': document_digest}
    if resource not in resources:
        resources.append(resource)
    objects[0].setdefault('extensions', {}).setdefault('ray_tracing', {})['surface_mapping'] = mapping
    return candidate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ('scene', 'mapping', 'document', 'output'):
        parser.add_argument('--'+arg, type=Path, required=True)
    parser.add_argument('--object-id', required=True)
    parser.add_argument('--kind', choices=sorted(KINDS), required=True)
    parser.add_argument('--expected-document-sha256', required=True)
    parser.add_argument('--expected-mapping-sha256', required=True)
    args = parser.parse_args()
    candidate = bind(load(args.scene)[0], args.object_id, args.mapping, args.document, args.kind,
                     args.expected_document_sha256, args.expected_mapping_sha256)
    with args.output.open('x') as file:
        json.dump(candidate, file, indent=2)
        file.write('\n')
    print(json.dumps({'candidate': str(args.output.resolve()), 'state': 'prepared_requires_renderer_preflight',
                      'object_id': args.object_id, 'source_kind': args.kind}))


if __name__ == '__main__':
    main()

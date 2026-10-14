"""Fixed discovery/dispatch tools for clients that cache their initial tool list."""
import hashlib
import json
import math

CATALOG_TOOL = 'editor_catalog'
COMMAND_TOOL = 'editor_command'
RESERVED = {CATALOG_TOOL, COMMAND_TOOL}


def fixed_tools():
    return [
        {'name': CATALOG_TOOL,
         'description': 'Read the current optiC command catalog and catalog_id. Use this before editor_command when your client caches tool definitions. Does not attach, edit or save a scene.',
         'annotations': {'readOnlyHint': True, 'openWorldHint': False},
         'inputSchema': {'type': 'object', 'properties': {}, 'required': [], 'additionalProperties': False}},
        {'name': COMMAND_TOOL,
         'description': 'Invoke a tool listed by editor_catalog with its catalog_id and exact arguments. Uses the same scene attachment and editor command as the named tool. May mutate, Undo/Redo or Save; mutations require expected_revision inside arguments. No arbitrary methods or code.',
         'annotations': {'readOnlyHint': False, 'openWorldHint': False, 'idempotentHint': False},
         'inputSchema': {'type': 'object', 'properties': {
             'catalog_id': {'type': 'string'}, 'tool': {'type': 'string'},
             'arguments': {'type': 'object'}},
             'required': ['catalog_id', 'tool', 'arguments'], 'additionalProperties': False}}
    ]


def catalog_id(catalog):
    payload = {'tools': catalog.tools, 'methods': {name: row[0] for name, row in catalog.methods.items()}}
    return hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(',', ':'), allow_nan=False).encode()).hexdigest()


def read_catalog(catalog, scene):
    return {'ok': True, 'schema_version': 'optic_editor_catalog_v1',
            'catalog_id': catalog_id(catalog), 'scene_path': str(scene) if scene else None,
            'tools': catalog.tools}


def unwrap(catalog, arguments):
    validate(fixed_tools()[1]['inputSchema'], arguments)
    if arguments['catalog_id'] != catalog_id(catalog):
        raise ValueError('stale catalog_id; call editor_catalog and review the current schema before retrying')
    name = arguments['tool']
    if name in RESERVED or name not in catalog.methods:
        raise ValueError('editor_command only invokes tools in the current editor_catalog')
    return name, arguments['arguments']


def validate(schema, value, path='arguments'):
    """Validate the schema vocabulary emitted by this bridge; fail closed on others.

    The native document validator still owns command semantics and revision guards.
    Both direct and compatibility dispatch use this same argument boundary.
    """
    if schema is True:
        return
    if schema is False:
        raise ValueError(path + ' is forbidden')
    supported = {'type', 'description', 'properties', 'required', 'additionalProperties',
                 'items', 'minItems', 'maxItems', 'minLength', 'maxLength',
                 'minimum', 'maximum', 'exclusiveMinimum', 'exclusiveMaximum', 'enum', 'const'}
    if not isinstance(schema, dict) or set(schema) - supported:
        raise ValueError(path + ' uses an unsupported argument schema')
    kind = schema.get('type')
    numeric = isinstance(value, (int, float)) and not isinstance(value, bool)
    matches = {'object': isinstance(value, dict), 'array': isinstance(value, list),
               'string': isinstance(value, str), 'boolean': isinstance(value, bool),
               'integer': numeric and isinstance(value, int), 'number': numeric,
               'null': value is None}
    if kind is not None and not matches.get(kind, False):
        raise ValueError(path + ' must be ' + str(kind))
    if isinstance(value, float) and not math.isfinite(value):
        raise ValueError(path + ' must be finite')
    if 'const' in schema and (value != schema['const'] or type(value) is not type(schema['const'])):
        raise ValueError(path + ' does not match const')
    if 'enum' in schema and not any(value == item and type(value) is type(item) for item in schema['enum']):
        raise ValueError(path + ' is not in enum')
    if isinstance(value, dict):
        required = schema.get('required', [])
        if any(key not in value for key in required):
            raise ValueError(path + ' is missing required fields')
        properties = schema.get('properties', {})
        additional = schema.get('additionalProperties', True)
        for key, item in value.items():
            validate(properties.get(key, additional), item, path + '.' + key)
    if isinstance(value, list):
        if len(value) < schema.get('minItems', 0) or len(value) > schema.get('maxItems', len(value)):
            raise ValueError(path + ' has an invalid item count')
        for index, item in enumerate(value):
            validate(schema.get('items', True), item, path + '[' + str(index) + ']')
    if isinstance(value, str):
        if len(value) < schema.get('minLength', 0) or len(value) > schema.get('maxLength', len(value)):
            raise ValueError(path + ' has an invalid string length')
    if numeric:
        if ('minimum' in schema and value < schema['minimum']) or ('maximum' in schema and value > schema['maximum']):
            raise ValueError(path + ' is outside bounds')
        if ('exclusiveMinimum' in schema and value <= schema['exclusiveMinimum']) or ('exclusiveMaximum' in schema and value >= schema['exclusiveMaximum']):
            raise ValueError(path + ' is outside exclusive bounds')

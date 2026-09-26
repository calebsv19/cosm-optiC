"""Deterministic first placement for a newly imported managed mesh.

The STL stays unchanged. The managed runtime mesh is centered on its bounds
and floor, and the new scene instance receives an editable, undoable transform.
"""
import json
import math
from pathlib import Path


def _number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def _horizontal_floor(scene):
    best = None
    for obj in scene.get('objects', []):
        if obj.get('object_type') != 'plane_primitive' or not obj.get('flags', {}).get('visible', True):
            continue
        primitive = obj.get('primitive', {})
        frame = primitive.get('frame', {})
        normal = frame.get('normal', {})
        width, height = primitive.get('width'), primitive.get('height')
        if not (_number(width) and _number(height) and width > 0 and height > 0 and
                _number(normal.get('z')) and abs(normal['z']) >= 0.9):
            continue
        origin, axis_u, axis_v = frame.get('origin', {}), frame.get('axis_u', {}), frame.get('axis_v', {})
        if not all(_number(v.get(axis)) for v in (origin, axis_u, axis_v) for axis in ('x', 'y', 'z')):
            continue
        corners = [
            {axis: origin[axis] + su * width * axis_u[axis] / 2 + sv * height * axis_v[axis] / 2
             for axis in ('x', 'y', 'z')}
            for su in (-1, 1) for sv in (-1, 1)
        ]
        bounds = {axis: (min(p[axis] for p in corners), max(p[axis] for p in corners))
                  for axis in ('x', 'y')}
        footprint = (bounds['x'][1] - bounds['x'][0]) * (bounds['y'][1] - bounds['y'][0])
        if footprint > 0 and (best is None or footprint > best[0]):
            best = (footprint, bounds, origin['z'])
    return best


def _context(scene):
    floor = _horizontal_floor(scene)
    if floor:
        _, bounds, z = floor
        return ((bounds['x'][0] + bounds['x'][1]) / 2,
                (bounds['y'][0] + bounds['y'][1]) / 2, z,
                min(bounds['x'][1] - bounds['x'][0], bounds['y'][1] - bounds['y'][0]))
    # Without a construction floor, use the authored object cluster.  Medians
    # prevent a single distant imported mesh from making the next one enormous.
    points = []
    for obj in scene.get('objects', []):
        if not obj.get('flags', {}).get('visible', True):
            continue
        position = obj.get('transform', {}).get('position', {})
        if all(_number(position.get(axis)) for axis in ('x', 'y', 'z')):
            points.append(position)
    if not points:
        return 0.0, 0.0, 0.0, 1.0
    def median(values):
        values = sorted(values)
        middle = len(values) // 2
        return values[middle] if len(values) % 2 else (values[middle - 1] + values[middle]) / 2
    cx, cy = (median([p[axis] for p in points]) for axis in ('x', 'y'))
    z = min(p['z'] for p in points)
    if len(points) < 2:
        return cx, cy, z, 1.0
    spread = max(median([abs(p['x'] - cx) for p in points]),
                 median([abs(p['y'] - cy) for p in points])) * 4
    return cx, cy, z, max(1.0, spread)


def _open_floor_position(scene, bounds):
    """Choose a visible, deterministic floor slot away from existing objects."""
    cx = (bounds['x'][0] + bounds['x'][1]) / 2
    cy = (bounds['y'][0] + bounds['y'][1]) / 2
    width = bounds['x'][1] - bounds['x'][0]
    height = bounds['y'][1] - bounds['y'][0]
    occupied = []
    for obj in scene.get('objects', []):
        if obj.get('object_type') != 'mesh_asset_instance' or not obj.get('flags', {}).get('visible', True):
            continue
        position = obj.get('transform', {}).get('position', {})
        if _number(position.get('x')) and _number(position.get('y')):
            occupied.append((position['x'], position['y']))
    if not occupied:
        return cx, cy
    offsets = [(0, 0), (-1, 0), (1, 0), (0, -1), (0, 1),
               (-1, -1), (1, -1), (-1, 1), (1, 1)]
    def score(offset):
        x = cx + offset[0] * width / 4
        y = cy + offset[1] * height / 4
        clearance = min(math.hypot(x - px, y - py) for px, py in occupied)
        return clearance - 0.2 * math.hypot(x - cx, y - cy)
    chosen = max(offsets, key=score)
    return cx + chosen[0] * width / 4, cy + chosen[1] * height / 4


def place_new_instance(scene, obj, runtime_path: Path):
    """Fit the mesh to 15% of the floor or object-cluster span and ground it."""
    runtime = json.loads(runtime_path.read_text())
    bounds = runtime['local_bounds']
    low, high = bounds['min'], bounds['max']
    if not all(_number(low.get(a)) and _number(high.get(a)) and high[a] >= low[a]
               for a in ('x', 'y', 'z')):
        raise ValueError('compiled mesh has invalid bounds')
    span = max(high[a] - low[a] for a in ('x', 'y', 'z'))
    if span <= 0:
        raise ValueError('compiled mesh has no measurable size')
    cx, cy, floor_z, context_span = _context(scene)
    floor = _horizontal_floor(scene)
    if floor:
        cx, cy = _open_floor_position(scene, floor[1])
    factor = 0.15 * context_span / span
    if not _number(factor) or factor <= 0:
        raise ValueError('cannot place compiled mesh')
    obj['transform'] = {
        'position': {'x': cx - (low['x'] + high['x']) * factor / 2,
                     'y': cy - (low['y'] + high['y']) * factor / 2,
                     'z': floor_z - low['z'] * factor},
        'rotation': {'x': 0.0, 'y': 0.0, 'z': 0.0},
        'scale': {'x': factor, 'y': factor, 'z': factor},
        'pivot_policy': 'bounds_center',
    }
    obj['display_name'] = Path(scene['extensions']['ray_tracing']['managed_mesh_assets']['assets'][
        obj['extensions']['ray_tracing']['managed_mesh']['asset_id']]['original_name']).stem
    return {'size': {a: (high[a] - low[a]) * factor for a in ('x', 'y', 'z')},
            'center': {'x': cx, 'y': cy}, 'floor_z': floor_z}

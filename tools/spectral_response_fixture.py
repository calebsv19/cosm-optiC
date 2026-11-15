"""Synthetic visible spectra, passive reflectances and matched ordinary V3 scenes.

These curves are declared controls, not measured lamp or pigment data.
"""
import copy
import math
import re
from pathlib import Path

from spectral_room_testbench import material, plane, room, scene_root, v

REPO = Path(__file__).resolve().parents[1]
ROWS = [tuple(map(float, row.split(','))) for row in re.findall(r'\{([^{}]+)\}',
    (REPO / 'src/render/cie_1931_2deg_380_780.inc').read_text())]
WAVELENGTHS = sorted(set(range(380, 781, 20)) | {405, 410, 450, 535, 610})


def cie(nm):
    # Match the declared vacuum-to-air observer convention. The table is shared
    # input data; the product quadrature below is independent of native sampling.
    u = (1000 / nm) ** 2
    at = nm / (1 + 1e-8 * (8060.51 + 2480990 / (132.274-u) + 17455.7 / (39.32957-u))) - 379
    i = int(at)
    return [a*(i+1-at) + b*(at-i) for a, b in zip(ROWS[i], ROWS[i+1])]


def value(curve, nm):
    for (a, x), (b, y) in zip(curve, curve[1:]):
        if a <= nm <= b:
            return x + (y-x)*(nm-a)/(b-a)
    return 0


def xyz(curve, reflectance=None, step=.1):
    result = [0., 0., 0.]
    for i in range(round(400/step)):
        nm = 380 + (i+.5)*step
        weight = value(curve, nm)*step
        if reflectance is not None:
            weight *= value(reflectance, nm)
        for j, observer in enumerate(cie(nm)):
            result[j] += weight * observer
    return result


def power(curve):
    return sum((b-a)*(x+y)/2 for (a, x), (b, y) in zip(curve, curve[1:]))


def gaussian(nm, center, sigma):
    q = math.exp(-.5*((nm-center)/sigma)**2)
    return q if q >= 1e-10 else 0


def solve3(matrix, rhs):
    a = [list(row)+[float(target)] for row, target in zip(matrix, rhs)]
    for i in range(3):
        best = max(range(i, 3), key=lambda j: abs(a[j][i]))
        a[i], a[best] = a[best], a[i]
        assert abs(a[i][i]) > 1e-14
        pivot = a[i][i]
        a[i] = [x/pivot for x in a[i]]
        for j in range(3):
            if j != i:
                scale = a[j][i]
                a[j] = [x-scale*y for x, y in zip(a[j], a[i])]
    return [row[3] for row in a]


def sources():
    # Keep emitted support >=400nm; 380nm is the observer/curve boundary only.
    flat = [[nm, .018 if nm > 400 else 0] for nm in WAVELENGTHS]
    target = xyz(flat)
    curves = {'white_broad': flat}
    definitions = {
        'warm_2800': lambda nm: (550/nm)**5 * math.expm1(.01438776877/(550e-9*2800)) /
            math.expm1(.01438776877/(nm*1e-9*2800)),
        'green_broad': lambda nm: .65 + 1.6*gaussian(nm, 540, 45) + .3*gaussian(nm, 450, 25),
        'blue_broad': lambda nm: .25 + 2*gaussian(nm, 460, 40),
        'violet_405': lambda nm: gaussian(nm, 405, 9),
        'blue_450': lambda nm: gaussian(nm, 450, 18),
        'green_535': lambda nm: gaussian(nm, 535, 20),
    }
    for name, fn in definitions.items():
        curve = [[nm, fn(nm) if nm > 400 else 0] for nm in WAVELENGTHS]
        scale = target[1]/xyz(curve)[1]
        curves[name] = [[nm, val*scale] for nm, val in curve]
    bases = [[[nm, gaussian(nm, center, 12) if nm > 400 else 0] for nm in WAVELENGTHS]
             for center in (450, 535, 610)]
    columns = [xyz(b) for b in bases]
    coefficients = solve3([[col[i] for col in columns] for i in range(3)], target)
    assert all(c > 0 for c in coefficients)
    curves['white_three_peak'] = [[nm, sum(c*b[k][1] for c, b in zip(coefficients, bases))]
                                 for k, nm in enumerate(WAVELENGTHS)]
    return curves


def reflectances():
    return {
        'neutral': [[380, .72], [780, .72]],
        'orange': [[380, .035], [450, .035], [500, .09], [550, .35], [600, .78], [650, .82], [780, .82]],
        'green': [[nm, .025+.72*gaussian(nm, 535, 32)] for nm in WAVELENGTHS],
        'amber': [[nm, .025+.72*gaussian(nm, 580, 12)] for nm in WAVELENGTHS],
        'cyan': [[380, .025], [420, .15], [450, .72], [530, .72], [570, .06], [620, .025], [780, .025]],
        'blue': [[nm, .025+.72*gaussian(nm, 450, 28)] for nm in WAVELENGTHS],
        'violet': [[nm, .025+.72*gaussian(nm, 410, 16)] for nm in WAVELENGTHS],
        'black': [[380, 0], [780, 0]],
    }


def fixture(source, wall='orange', width=320, height=240, samples=1024,
            seed=19, component='total', view='room'):
    q, scene = room()
    scene['scene_id'] = 'visible_spectral_response_v1'
    scene['objects'] = scene['objects'][:6]
    profile = q.pop('spectral')
    profile['quality_profile'] = 'surface_path_v1'
    p = profile['surface_path_v1']
    p['materials'] = p['materials'][:6]
    rho = reflectances()
    p['materials'][2] = material('left', rho[wall])
    # Viewed left-to-right: neutral, orange, amber, green, cyan, blue, violet.
    swatches = ('neutral', 'orange', 'amber', 'green', 'cyan', 'blue', 'violet')
    for i, name in enumerate(swatches):
        object_id = 'swatch_'+name
        x = .75 - .25*i
        scene['objects'].append(plane(object_id, (x, 1.993, 1.35), (1, 0, 0),
            (0, 0, 1), (0, -1, 0), .19, .32))
        p['materials'].append(material(object_id, rho[name]))
    p.update(samples=samples, seed=seed, output_component=component,
        probes=[{'name': 'center', 'x': width//2, 'y': height//2}])
    p['light']['spectrum'] = {'continuum': copy.deepcopy(source)}
    q['render'].update(width=width, height=height)
    if view == 'shadow':
        q['inspection'].update(camera_position=v(-.55, 1.8, .9),
            camera_look_at=v(.1, 1.42, 0), camera_zoom=1.5)
    scene['extensions'] = {'ray_tracing': {'spectral_transport_v1': profile,
        'editor_render_integrator_v1': 'disney_v3_spectral'}}
    assert 'spectral' not in q
    assert width*height*samples*12 <= 1200000000
    return q, scene


def transfer_fixture(source, reflectance, seed=19):
    scene = scene_root([
        plane('receiver', (0, 0, 0), (0, 1, 0), (0, 0, 1), (1, 0, 0), .8, .8),
        plane('panel', (1, 0, 0), (0, 1, 0), (0, 0, -1), (-1, 0, 0), .6, .6)])
    profile = {'quality_profile': 'surface_path_v1', 'exposure': 1000,
        'surface_path_v1': {'samples': 8192, 'max_depth': 2, 'seed': seed,
            'materials': [material('receiver', .8), material('panel', reflectance)],
            'light': {'type': 'area', 'origin_m': [.4, 1, 0], 'axis_u': [0, 1, 0],
                'axis_v': [0, 0, 1], 'width_m': .12, 'height_m': .12,
                'spectrum': {'continuum': source}},
            'probes': [{'name': 'indirect_receiver', 'x': 0, 'y': 0}]}}
    scene['extensions'] = {'ray_tracing': {'spectral_transport_v1': profile}}
    return {'schema_version': 'ray_tracing_agent_render_request_v2',
        'render': {'integrator_3d': 'disney_v3_spectral', 'width': 1, 'height': 1},
        'inspection': {'camera_position': v(.35, 0, 0), 'camera_look_at': v(0, 0, 0),
            'camera_zoom': 1000}}, scene


def geometry_reference(n=80, k=16):
    total = 0.
    for j in range(n):
        y = -.3+.6*(j+.5)/n
        for i in range(n):
            z = -.3+.6*(i+.5)/n
            incoming = 0.
            for sj in range(k):
                sy = 1+.12*((sj+.5)/k-.5)
                for si in range(k):
                    sz = .12*((si+.5)/k-.5)
                    r2 = .6**2+(y-sy)**2+(z-sz)**2
                    incoming += .6**2/r2**2
            incoming *= .12**2/k**2
            total += incoming/(math.pi**2*(1+y*y+z*z)**2)
    return total*.6**2/n**2

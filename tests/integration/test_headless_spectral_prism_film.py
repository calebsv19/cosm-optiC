#!/usr/bin/env python3
"""Native prism film contract against analytical optics and independent quadrature."""
import argparse
import copy
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

from test_headless_spectral_optics import white_lines

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'tools'))
from optical_comparison_artifacts import bmp_rgba

def run(name, q):
    d=ROOT/name;d.mkdir()
    (d/'config').symlink_to(REPO/'config',target_is_directory=True)
    q=copy.deepcopy(q)
    q['run_id']=name
    q['output']={'root':'output','overwrite':False}
    (d/'request.json').write_text(json.dumps(q,indent=2))
    cmd=[str(CLI),'--request','request.json','--render','--summary','summary.json','--summary-file-only']
    start=time.monotonic()
    with (d/'log.txt').open('w') as log:
        subprocess.run(cmd,cwd=d,env={**os.environ,'TMPDIR':str(ROOT),},
                       stdout=log,stderr=subprocess.STDOUT,timeout=60,check=True)
    s=json.loads((d/'summary.json').read_text())
    w,h,p=bmp_rgba(d/'output/frames/frame_0000.bmp')
    print(name,round(time.monotonic()-start,2),s['spectral_surface_map']['key'],flush=True)
    return d,s,w,h,p

def pixel(frame,x,y):
    _,_,w,h,p=frame
    assert 0<=x<w and 0<=y<h
    i=(y*w+x)*4
    return list(p[i:i+3])

def expected(rgb,exposure,soft):
    v=[x*exposure for x in rgb]
    if soft:
        low=min(v)
        if low<0:
            y=max(0,sum(a*b for a,b in zip(v,(.2126729,.7151522,.0721750))))
            v=[y/(y-low)*(x-low) for x in v]
        peak=max(v);v=[x/(1+peak) for x in v]
    out=[]
    for x in v:
        x=max(0,min(1,x))
        out.append(round(255*(12.92*x if x<=.0031308 else 1.055*x**(1/2.4)-.055)))
    return out

WIDTH, HEIGHT = 257, 63
RADIUS = .0014
CENTER = .262
ZOOM = 5.2
APERTURE = .00085
CIE=[tuple(map(float,r.split(','))) for r in re.findall(r'\{([^{}]+)\}',
     (REPO/'src/render/cie_1931_2deg_380_780.inc').read_text())]
def air(nm):
    s=(1000/nm)**2
    return nm/(1+1e-8*(8060.51+2480990/(132.274-s)+17455.7/(39.32957-s)))
def observer(nm):
    k=air(nm)-379
    i=int(k)
    return [a*(1-k+i)+b*(k-i) for a,b in zip(CIE[i],CIE[i+1])]
def index(nm):
    l=(air(nm)/1000)**2
    return math.sqrt(1+sum(b*l/(l-c) for b,c in ((1.03961212,.00600069867),
                        (.231792344,.0200179144),(1.01046945,103.560653))))
def optical(nm,s):
    n=index(nm)
    inc=math.radians(50)
    inside=math.asin(math.sin(inc)/n)
    second=math.pi/3-inside
    ext=math.asin(n*math.sin(second))
    exit_heading=math.pi/3-ext
    # Analytical intersections with x=0 and y=.2-x/sqrt(3).
    x0=-.12855752193730788-s*math.sin(inc)
    y0=-.1532088886237956+s*math.cos(inc)
    entry=y0-x0*math.tan(inc)
    x=(.2-entry)/(math.tan(inside)+1/math.sqrt(3))
    y=.2-x/math.sqrt(3)+(1-x)*math.tan(exit_heading)
    def t(a,b):
        return 1-.5*((math.sin(a-b)/math.sin(a+b))**2+(math.tan(a-b)/math.tan(a+b))**2)
    return y,t(inc,inside)*t(second,ext)

def strip_oracle(nms,step):
    points=[CENTER+(i-(WIDTH-1)/2)*step for i in range(WIDTH)]
    xyz=[[0.,0.,0.] for _ in points]
    disk=[]
    for j in range(16):
        r=APERTURE*math.sqrt((j+.5)/16)
        for i in range(32):
            a=(i+.5)*math.tau/32
            disk.append((r*math.cos(a),r*math.sin(a)))
    for nm,watts in nms:
        y0,trans=optical(nm,0)
        shift=(optical(nm,APERTURE)[0]-y0)/APERTURE
        color=observer(nm)
        factor=watts*trans*2/(math.pi**2*RADIUS**2*len(disk))
        for u,z in disk:
            y=y0+shift*u
            reach=math.sqrt(RADIUS**2-z*z)
            i0=max(0,math.ceil((y-reach-points[0])/step))
            i1=min(WIDTH-1,math.floor((y+reach-points[0])/step))
            for i in range(i0,i1+1):
                w=factor*(1-((points[i]-y)**2+z*z)/RADIUS**2)
                for c in range(3):xyz[i][c]+=w*color[c]
    return xyz

def prepare():
    fixture=REPO/'tests/fixtures'
    q=json.loads((fixture/'spectral_surface_map/request.json').read_text())
    q['scene']['runtime_scene_path']=str(fixture/'spectral_prism/scene_runtime.json')
    q['render'].update(width=WIDTH,height=HEIGHT)
    q['inspection'].update(camera_zoom=ZOOM,camera_position={'x':.9,'y':CENTER,'z':0},
                           camera_look_at={'x':1,'y':CENTER,'z':0})
    s=q['spectral']
    s.update(exposure=.00024,receiver_radius_m=RADIUS,
             linear_strip={'x':0,'y':HEIGHT//2,'count':WIDTH},
             selected_pixel={'x':WIDTH//2,'y':HEIGHT//2},
             sensors=[{'type':'radiometric','response':[[380,1],[780,1]]}])
    s['surface_map']['photons_per_source']=262144
    s['surface_map']['sources'][0]['aperture_m']=APERTURE
    return q

def strip(frame):
    s=frame[1]
    rows=s['spectral_linear_strip']['xyz']
    assert rows[WIDTH//2]==s['spectral_selected_pixel']['xyz']
    return rows

def profile_error(values,oracle):
    return sum(abs(x-y) for a,b in zip(values,oracle) for x,y in zip(a,b))/sum(sum(r) for r in oracle)
def metrics(values):
    rows=[r[1] for r in values]
    total=sum(rows)
    def percentile(f):
        acc=0
        for i,r in enumerate(rows):
            if r>0 and acc+r>=total*f:return i-.5+(total*f-acc)/r
            acc+=r
        raise ValueError('empty profile')
    return {'width_px':percentile(.9)-percentile(.1),'integrated_y':total}

def run_reference():
    base=prepare()
    results={'broadband':[],'lines':[]}
    step=None
    for seed in (19,104729):
        for exact in (False,True):
            q=copy.deepcopy(base)
            q['spectral']['surface_map']['seed']=seed
            if exact:q['spectral']['surface_observer']='deposit_wavelength'
            frame=run(f'reference-{seed}-{int(exact)}',q)
            if step is None:
                fov=frame[1]['evaluated_camera']['fov_y']
                step=2*.1*math.tan(math.radians(fov/2))/ZOOM/HEIGHT
                oracle=strip_oracle([(380+(i+.5)*.5,.0025*.5) for i in range(800)],step)
            err=profile_error(strip(frame),oracle)
            print('XYZ strip L1',seed,exact,err,flush=True)
            assert err < (.01 if exact else .06), err
            if not exact:legacy=frame
            else:
                assert legacy[1]['spectral_surface_map']['key']==frame[1]['spectral_surface_map']['key']
                assert err < results['broadband'][-1]['xyz_l1_relative'] * .5
                assert legacy[1]['spectral_center_sensor_signals']==frame[1]['spectral_center_sensor_signals']
                assert legacy[1]['spectral_surface_map']['bands']==frame[1]['spectral_surface_map']['bands']
            results['broadband'].append({'seed':seed,'exact':exact,'xyz_l1_relative':err})
    for nm in (405,450,530,610,650,690):
        q=copy.deepcopy(base)
        q['spectral']['surface_observer']='deposit_wavelength'
        m=q['spectral']['surface_map']
        m['photons_per_source']=65536
        m['sources'][0].update(lines=[[nm,1]],continuum=[])
        frame=run(f'line-{nm}',q)
        native=frame[1]['spectral_surface_map']['bands'][0]
        y,t=optical(nm,0)
        assert abs(native['centroid_m'][1]-y)<.00004
        assert abs(native['watts']-t)<1e-6
        measured=metrics(strip(frame))
        reference=metrics(strip_oracle([(nm,1)],step))
        assert abs(measured['width_px']-reference['width_px'])<.3
        assert abs(measured['integrated_y']/reference['integrated_y']-1)<.015
        results['lines'].append({'nm':nm,'measured':measured,'reference':reference,
                                 'centroid_error_mm':1000*abs(native['centroid_m'][1]-y),
                                 'watts':native['watts'],'reference_watts':t})
    results.update(pixel_pitch_m=step,oracle_xyz=oracle)
    (ROOT/'reference-results.json').write_text(json.dumps(results,indent=2))
    print('PASS independent broadband and six narrow-line strips',flush=True)
def frame_settings(radius,grid,photons,seed,shift=0,coarse=False):
    q=prepare()
    s=q['spectral']
    s.update(receiver_radius_m=radius,surface_observer='deposit_wavelength',pixel_grid=grid)
    q['inspection']['camera_position']['y']+=shift
    q['inspection']['camera_look_at']['y']+=shift
    if coarse:q['inspection']['camera_zoom']=1.15
    m=s['surface_map'];m.update(photons_per_source=photons,seed=seed)
    m['sources'][0].update(lines=[[450,1]],continuum=[])
    return q

def profile(f):
    return f[1]['spectral_linear_strip']['xyz']

def work(f):
    m=f[1]['spectral_surface_map']
    return m['build_ms']+m['film_ms']

def pixel_area(radius,pitch,shift):
    # Integrate a uniform disk after the two Snell interfaces, a normalized
    # parabolic gather, and a 16x16 rectangular detector quadrature.
    n=16
    center,t=optical(450,0)
    slope=(optical(450,APERTURE)[0]-center)/APERTURE
    samples=[]
    for j in range(24):
        r=APERTURE*math.sqrt((j+.5)/24)
        for k in range(48):
            theta=math.tau*(k+.5)/48
            samples.append((center+slope*r*math.cos(theta),r*math.sin(theta)))
    factor=t*2/(math.pi**2*radius**2*len(samples)*n*n)
    radiance=[0.]*WIDTH
    left=CENTER+shift-(WIDTH-1)/2*pitch
    for y,z in samples:
        for j in range(n):
            dz=((j+.5)/n-.5)*pitch-z
            if abs(dz)>=radius:continue
            rr=math.sqrt(radius*radius-dz*dz)
            for i in range(n):
                start=left+((i+.5)/n-.5)*pitch
                a=max(0,math.ceil((y-rr-start)/pitch))
                b=min(WIDTH-1,math.floor((y+rr-start)/pitch))
                for k in range(a,b+1):
                    dy=start+k*pitch-y
                    radiance[k]+=factor*(1-(dy*dy+dz*dz)/radius**2)
    cie=observer(450)
    return [[v*c for c in cie] for v in radiance]


def check_display():
    q = prepare()
    s = q['spectral']
    s.update(glass='constant', constant_ior=1, exposure=.01)
    lamp = s['surface_map']['sources'][0]
    lamp.update(origin_m=[.8, CENTER, 0], direction=[1, 0, 0],
                aperture_m=.05, continuum=[], lines=white_lines([450, 550, 610]))
    s['surface_map']['photons_per_source'] = 256
    old = run('neutral-clip', q)
    s['display_transform'] = 'spectral_soft_v1'
    low = run('neutral-soft', q)
    assert low[1]['spectral_linear_strip'] == old[1]['spectral_linear_strip']
    assert low[1]['spectral_surface_map']['key'] == old[1]['spectral_surface_map']['key']
    s['exposure'] = .1
    high = run('neutral-bright', q)
    for f, exposure in ((low, .01), (high, .1)):
        rgb = pixel(f, WIDTH//2, HEIGHT//2)
        assert max(rgb)-min(rgb) <= 1 and 0 < min(rgb) <= max(rgb) < 255
        assert rgb == expected(f[1]['spectral_selected_pixel']['signed_linear_rgb'], exposure, True)
    assert all(a<b for a,b in zip(pixel(low, WIDTH//2, HEIGHT//2), pixel(high, WIDTH//2, HEIGHT//2)))


def check_pixel_grid():
    for phase in (0, .5):
        pitch = .2*math.tan(math.radians(55/2))/1.15/HEIGHT
        q = frame_settings(.0008, 1, 65536, 19, phase*pitch, coarse=True)
        center = run(f'pixel-center-{phase}', q)
        assert center[1]['evaluated_camera']['fov_y'] == 55
        q['spectral']['pixel_grid'] = 4
        filtered = run(f'pixel-filtered-{phase}', q)
        assert center[1]['spectral_surface_map']['key'] == filtered[1]['spectral_surface_map']['key']
        oracle = pixel_area(.0008, pitch, phase*pitch)
        center_error = profile_error(profile(center), oracle)
        filtered_error = profile_error(profile(filtered), oracle)
        assert filtered_error < .025 and filtered_error < center_error * .15


def check_refusals():
    cases = {
        'unknown-display': {'display_transform': 'unknown'},
        'invalid-observer': {'surface_observer': 1},
        'strip-outside-film': {'linear_strip': {'x': WIDTH - 1, 'y': 0, 'count': 2}},
        'invalid-grid': {'pixel_grid': 5},
        'progressive-observer': {
            'surface_observer': 'deposit_wavelength',
            'sampling': {'mode': 'converge', 'min_passes': 16, 'max_passes': 16},
        },
    }
    for name, fields in cases.items():
        q = prepare()
        q['spectral'].update(fields)
        q['output'] = {'root': str(ROOT / name), 'overwrite': False}
        request = ROOT / (name + '.json')
        request.write_text(json.dumps(q))
        result = subprocess.run([CLI, '--request', request, '--preflight'],
                                cwd=ROOT, capture_output=True, timeout=20)
        assert result.returncode != 0, name


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--cli', required=True, type=Path)
    CLI = parser.parse_args().cli.resolve()
    with tempfile.TemporaryDirectory(prefix='optic-prism-film-') as temp:
        ROOT = Path(temp)
        check_display()
        run_reference()
        check_pixel_grid()
        check_refusals()

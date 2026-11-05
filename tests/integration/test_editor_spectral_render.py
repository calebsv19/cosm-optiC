"""Exercise V3 through the editor's actual Render frame controls."""
import argparse
import hashlib
import json
import math
import os
import shutil
import struct
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def prepare(root, prism, reconstruction=None):
    runtime = root / 'data/runtime'
    runtime.mkdir(parents=True)
    write(runtime / 'animation_config.json', dict(editorMode=1, spaceMode=1, windowWidth=1280,
          windowHeight=800, inputRoot=str(REPO/'config'), outputRoot=str(runtime)))
    write(runtime / 'scene_config.json', {'window': {'width': 1280, 'height': 800}})
    source = REPO/'tests/fixtures/spectral_prism'
    scene = json.loads((source/'scene_runtime.json').read_text())
    camera = [.987,.251,.004] if prism else [2,-3.3,1.8]
    target = [1,.257,0] if prism else [0,0,-.4]
    dx,dy,dz = (b-a for a,b in zip(camera,target))
    yaw,pitch = math.atan2(dx,-dy),math.atan2(dz, math.hypot(dx,dy))
    scene['cameras'][0].update(position=dict(zip('xyz',camera)),target=dict(zip('xyz',target)),
                                yaw=yaw,look_pitch=pitch)
    ray = scene['extensions']['ray_tracing'] = {'authoring': {
        'camera_path': {'mode':'BEZIER_CUBIC','points':[dict(x=camera[0],y=camera[1],rotation=yaw,handleLink=False)]},
        'camera_path_depth': {'points':[dict(z=camera[2],lookPitch=pitch)]}}}
    if prism:
        shutil.copytree(source/'assets',root/'assets')
        profile = json.loads((REPO/'tests/fixtures/spectral_surface_map/request.json').read_text())['spectral']
        ray['spectral_transport_v1'] = profile
        profile['surface_map']['photons_per_source'] = 65536
        profile['surface_map']['sources'][0]['aperture_m'] = .00085
        profile['receiver_radius_m'] = .0011
        profile['exposure'] = .00075
        # A bounded synthetic heterogeneous VF3D frame, in scene meters. The
        # detector is at x=1. Only the final part of the outgoing beam is in fog.
        nx,ny,nz = 8,20,12
        density = [.4+.6*(i%nx)/(nx-1) for i in range(nx*ny*nz)]
        n=len(density)
        header=struct.pack('<5I4xdQd7fI3I4x',0x56463344,1,nx,ny,nz,0.,0,.1,
                           .969,.212,-.024,.004,0,0,1,0,0,0,0)
        (root/'detector_fog.vf3d').write_bytes(header+struct.pack(f'<{n}f',*density)+bytes(n*17))
        profile['participating_volume_v1']=dict(kind='vf3d_density',source_path='detector_fog.vf3d',
            frame_index=0, sigma_a_per_m=[[380,1],[780,1]],sigma_s_per_m=[[380,75],[780,45]],
            g=.35,cell_m=.0015)
    else:
        demo=json.loads((REPO/'tests/fixtures/disney_v2_visual_matrix/mirror_surface_unification/scene_prism_runtime.json').read_text())
        scene['objects']=[demo['objects'][i] for i in (0,2,3)]
        ray['authoring']['object_materials']=[
            dict(object_id=obj['object_id'],material_id=0,object_color=color)
            for obj,color in zip(scene['objects'],(0x999999,0xd63324,0x2467c4))]
        scene['lights']=[dict(light_id='key',kind='point',position=dict(x=-1,y=-1.8,z=2.4),
                              intensity=7.5,radius=0)]
    if prism and reconstruction:
        profile['participating_volume_v1'].update(reconstruction=reconstruction,cell_m=.0005,phase_tolerance=.01)
        profile['surface_map']['gather']='adaptive_scattered'
    write(root/'scene.json',scene)


def run(binary,out,prism,reconstruction=None):
    prepare(out,prism,reconstruction)
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(REPO),
             RAY_TRACING_RUNTIME_MESH_ASSET_CACHE_ROOT=str(out/'cache'))
    if prism: env['OPTIC_TEST_PRISM']='1'
    with (out/'native.log').open('w') as log:
        subprocess.run([binary,out,out/'scene.json','--spectral-ui'],env=env,
                       stdout=log,stderr=subprocess.STDOUT,check=True,timeout=1800 if reconstruction else 180)
    for tag in (['surface','volume'] if prism else ['direct']):
        receipt=json.loads((out/(tag+'.json')).read_text())
        assert receipt['settings']['integrator']=='disney_v3_spectral'
        assert receipt['settings']['spectral_deposition']==dict(surface='surface',volume='surface_volume',direct='direct')[tag]
        if tag=='volume':
            assert receipt['settings']['gather_override']=='adaptive_scattered'
            assert receipt['surface_gather']=='adaptive_scattered' and receipt['expanded_surface_deposits']>0
        image=Path(receipt['image_path']).read_bytes()
        assert receipt['output_verified'] and receipt['image_sha256']==hashlib.sha256(image).hexdigest()
        offset=struct.unpack_from('<I',image,10)[0]
        assert len(set(image[offset:]))>24, tag
        assert (receipt['surface_deposits']>0)==prism
        assert (receipt['volume_scatter_events']>0)==(tag=='volume')
        assert receipt['volume_attached']==(tag=='volume')
        if prism:
            assert receipt['settings']['photons_per_source_override']==65536
            assert receipt['settings']['exposure_stops']==1
        if tag=='volume':
            assert receipt['spectral_volume_frame_sha256']==hashlib.sha256((out/'detector_fog.vf3d').read_bytes()).hexdigest()
            assert receipt['image_sha256']!=json.loads((out/'surface.json').read_text())['image_sha256']
        print(tag,receipt['image_sha256'],receipt['surface_deposits'],receipt['volume_scatter_events'])


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--binary',type=Path,required=True)
    parser.add_argument('--output-root',type=Path,required=True)
    parser.add_argument('--reconstruction',choices=['continuous','adaptive'])
    args=parser.parse_args()
    for tag in ('ordinary','prism'):
        run(args.binary.resolve(),args.output_root.resolve()/tag,tag=='prism',args.reconstruction)

    if args.reconstruction:
        repeat=args.output_root.resolve()/'prism-repeat'
        run(args.binary.resolve(),repeat,True,args.reconstruction)
        def image_hash(folder):
            return json.loads((folder/'volume.json').read_text())['image_sha256']
        assert image_hash(repeat)==image_hash(args.output_root.resolve()/'prism')
        print('PASS deterministic editor volume replay')

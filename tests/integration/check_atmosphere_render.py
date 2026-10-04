#!/usr/bin/env python3
"""Render retained static/animated atmosphere, validate exact cache identity."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from vf3d_initial_state_preset_tool import HEADER_STRUCT
from scene_atmosphere_assets import generate
from scene_workflow_bundle import snapshot_inputs, seal, verify

parser=argparse.ArgumentParser()
parser.add_argument('--root',type=Path,required=True)
parser.add_argument('--cli',type=Path,required=True)
args=parser.parse_args();root=args.root.resolve();cli=args.cli.resolve()
scene=json.loads((root/'scene_runtime.json').read_text())
attachment=scene['extensions']['ray_tracing']['authoring']['atmosphere_attachment']
static=copy.deepcopy(attachment)
static.update({key:value for key,value in generate(root/'scene_runtime.json','room_haze_v1').items() if key in ('source_path','source_kind')})
static['playback']['animated']=False
attachment.update(source_path='assets/cache/manifest.json',source_kind='manifest')
attachment['appearance'].update(density_scale=1,absorption_gain=.6,scatter_gain=2)
attachment['placement'].update(translation=[0,0,0],rotation_degrees=[0,0,0],scale=[1,1,1])
attachment['playback'].update(animated=True,rate_numerator=24,rate_denominator=1,timeline_start=0,source_start=0,missing='hold')
# The older portability fixture uses identical header IDs. This task-owned copy
# makes actual cache identity and visible density differences explicit.
for index in (0,2,4):
    path=root/f'assets/cache/frame_{index:06d}.vf3d'
    original=(ROOT/f'tests/fixtures/scene_project_worker_snapshot/assets/vf3d/active/frame_{index:06d}.vf3d').read_bytes()
    header=list(HEADER_STRUCT.unpack(original[:HEADER_STRUCT.size]));header[6]=index;header[5]=index/24
    header[8:12]=[-2,-2,-2,.5]
    count=header[2]*header[3]*header[4]
    density=struct.unpack(f'<{count}f',original[HEADER_STRUCT.size:HEADER_STRUCT.size+count*4])
    # The older fixture is an empty volume intended only for portability. Give
    # this render proof explicit populated fields, without changing that fixture.
    values=[.02 if index==0 else .3 if index==2 else 1.0 for _ in density]
    path.write_bytes(HEADER_STRUCT.pack(*header)+struct.pack(f'<{count}f',*values)+original[HEADER_STRUCT.size+count*4:])

def render(document,frame,name,expected=None,work=root,inspection=None,refusal=None):
    source=work/f'{name}.scene.json';source.write_text(json.dumps(document))
    request={'schema_version':'ray_tracing_agent_render_request_v1','run_id':name,
        'scene':{'runtime_scene_path':str(source)},'volume':{'enabled':False},
        'render':{'start_frame':frame,'frame_count':1,'width':96,'height':64,
                  'temporal_frames':1,'integrator_3d':'direct_light'},
        'output':{'root':str(work/name),'overwrite':True}}
    if inspection:
        request['inspection']=inspection
        if inspection.get('caustic_product_mode')=='photon_map':
            request['render']['integrator_3d']='disney_v2'
    path=work/f'{name}.request.json';path.write_text(json.dumps(request));summary=work/f'{name}.summary.json'
    with (work/f'{name}.log').open('w') as log:
        result=subprocess.run([str(cli),'--request',str(path),'--render','--summary',str(summary),'--summary-file-only'],
            cwd=work,env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(work)),stdout=log,stderr=subprocess.STDOUT,timeout=120)
    if refusal:
        diagnostic=(work/f'{name}.log').read_text()
        if summary.exists(): diagnostic+=summary.read_text()
        assert result.returncode and refusal in diagnostic,diagnostic[-3000:]
        return None,None
    result.check_returncode()
    report=json.loads(summary.read_text())
    if expected is not None:
        identity=report['evaluated_cache'];assert identity['valid'] and identity['source_frame_index']==expected,identity
        selected=work/f'assets/cache/frame_{expected:06d}.vf3d'
        assert identity['content_sha256']==hashlib.sha256(selected.read_bytes()).hexdigest()
        assert report['volume_attached'] and report['volume_summary_built']
    image=(work/name/'frames'/f'frame_{frame:04d}.bmp').read_bytes()
    assert len(set(image[128:]))>8,'empty image'
    return image,report

images=[]
for frame,expected in ((0,0),(4,4),(1,0),(3,2),(7,4),(2,2)):
    image,_=render(scene,frame,f'cache_{frame}',expected);images.append(image)
assert images[0]!=images[1],'animated density did not change the rendered image'
disabled=copy.deepcopy(scene);disabled['extensions']['ray_tracing']['authoring']['atmosphere_attachment']['enabled']=False
image,report=render(disabled,4,'disabled')
assert not report['evaluated_cache']['valid'] and not report['volume_attached']
# Use the dense frame: the first frame's weak haze can quantize to background.
assert image!=images[1],'enable/disable did not affect rendered atmosphere'
single=copy.deepcopy(scene)
single_value=single['extensions']['ray_tracing']['authoring']['atmosphere_attachment']
single_value.update(source_path='assets/cache/frame_000004.vf3d',source_kind='vf3d')
single_value['playback']['animated']=False
_,single_report=render(single,0,'static_frame4')
assert single_report['evaluated_cache']['source_frame_index']==4,'single file used ordinal zero instead of its actual header ID'
static_scene=copy.deepcopy(scene);static_scene['extensions']['ray_tracing']['authoring']['atmosphere_attachment']=static
static_image,static_report=render(static_scene,0,'static')
assert static_report['evaluated_cache']['valid'] and static_report['volume_attached']
placed=copy.deepcopy(scene)
placed['extensions']['ray_tracing']['authoring']['atmosphere_attachment']['placement']['translation']=[100,100,100]
placed_image,placed_report=render(placed,4,'placed')
assert placed_report['volume_attached'] and placed_image!=images[1],'placement did not move the rendered density'
# Normal preparation must accept placed ordinary caches and refuse unverified
# photon-volume normalization before starting the photon render.
for mode in ('spatial_cache','transport'):
    _,report=render(placed,4,f'placed_{mode}',inspection={
        'caustic_mode':mode,'caustic_volume_enabled':True,
        'caustic_surface_enabled':False,'caustic_sidecar_enabled':False,
        'caustic_sample_budget':32,'caustic_max_path_depth':2})
    assert report['volume_attached']
render(placed,4,'placed_photon_refusal',inspection={
    'caustic_product_mode':'photon_map','caustic_photon_volume_query_enabled':True,
    'caustic_photon_render_contribution_enabled':True,'caustic_photon_render_prep_population_enabled':True},
    refusal='photon-volume caches require identity placement')
water_scene=copy.deepcopy(scene)
water=water_scene['extensions']['ray_tracing']['authoring']['atmosphere_attachment']
water.update(source_path='assets/bundle/scene_bundle.json',source_kind='scene_bundle')
for frame,expected in ((4,4),(1,0),(3,2),(0,0)):
    _,report=render(water_scene,frame,f'water_{frame}')
    assert report['water_surface_source_found'] and report['water_surface_loaded'] and report['water_surface_mesh_attached'],report['water_surface']
    assert report['water_surface']['loaded_first_frame_index']==expected,report['water_surface']
    fluid_digest=hashlib.sha256((root/f'assets/bundle/frame_{expected:06d}.vf3d').read_bytes()).hexdigest()
    water_digest=hashlib.sha256((root/f'assets/bundle/water_surface_{expected:06d}.json').read_bytes()).hexdigest()
    assert report['evaluated_cache']['content_sha256']==hashlib.sha256((fluid_digest+water_digest).encode()).hexdigest()
raw_bundle=json.loads((root/'assets/bundle/scene_bundle.json').read_text())
raw_bundle['fluid_source'].update(kind='vf3d',path='frame_000004.vf3d')
(root/'assets/bundle/raw_bundle.scene.json').write_text(json.dumps(raw_bundle))
raw_bundle_scene=copy.deepcopy(water_scene)
raw_bundle_scene['extensions']['ray_tracing']['authoring']['atmosphere_attachment']['source_path']='assets/bundle/raw_bundle.scene.json'
_,raw_bundle_report=render(raw_bundle_scene,0,'raw_bundle')
assert raw_bundle_report['evaluated_cache']['source_frame_index']==4 and raw_bundle_report['water_surface_loaded']
# Verify complete static and water-bundle exports, then render relocated inputs.
# Disposable package payloads are task owned and removed after image/hash proof.
for document,name in ((static_scene,'static'),(water_scene,'water')):
    source=root/f'{name}-export.scene.json';source.write_text(json.dumps(document))
    intent=root/f'{name}-export.request.json'
    intent.write_text(json.dumps({'schema_version':'ray_tracing_agent_render_request_v1',
        'scene':{'runtime_scene_path':str(source)},'volume':{'enabled':False},
        'render':{'start_frame':0,'frame_count':1,'width':96,'height':64,'temporal_frames':1,'integrator_3d':'direct_light'},
        'output':{'root':'results','overwrite':False}}))
    with tempfile.TemporaryDirectory(prefix=f'{name}-export-',dir=root) as scratch:
        package=Path(scratch)/'snapshot'
        snapshot_inputs(source,intent,package);seal(package,f'{name}-cache-proof',cli)
        assert verify(package)['verified']
        baseline,_=render(document,0,f'{name}_export_baseline')
        relocated,_=render(document,0,f'{name}_relocated',work=package)
        assert baseline==relocated,f'{name} relocation changed render output'
print('Atmosphere render PASS: sparse actual IDs/full hashes, seeks, density/enable/placement image changes, ordinary placed caustic preparation/photon refusal, static preset, real water mesh/cache identity, complete sealed exports and exact relocated output')

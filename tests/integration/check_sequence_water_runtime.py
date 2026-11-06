#!/usr/bin/env python3
"""Real immutable rigid recording + real water cache clock qualification; no physics.
Use a prepared geometry-only scene bound to an accepted result with >=480 ticks.
Output is a fresh local fixture, with a bounded copy of the supplied water cache.
"""
import argparse,copy,hashlib,json,os,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for name in ['base','water','consumer','out']:p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();root=a.out.resolve();root.mkdir(parents=True,exist_ok=False);scene=json.loads(a.base.read_text());author=scene['extensions']['ray_tracing']['authoring'];revision=scene['extensions']['ray_tracing']['rigid_result']['result_revision'];files=list(a.water.rglob('*'));assert not any(f.is_symlink() for f in files);assert sum(f.stat().st_size for f in files if f.is_file())<32*1024*1024
shutil.copytree(a.water,root/'assets/water');rate=dict(numerator=24,denominator=1);extent=dict(start_frame=0,frame_count=72);ratio=lambda n,d=1:dict(numerator=n,denominator=d);author['scene_timeline']=dict(version=1,rate=rate,range=extent,tracks=[]);author['sequence']=dict(schema='ray_sequence_clock_v1',version=1,rate=rate,range=extent,source=dict(id=scene['scene_id'],revision=revision,rate=ratio(240),first_tick=0,last_tick=scene['extensions']['rigid_scene']['end_tick']),segments=[dict(id='play',start_frame=0,frame_count=12,kind='play',source_start=ratio(0),speed=ratio(1)),dict(id='hold',start_frame=12,frame_count=24,kind='hold',source_start=ratio(1,2),speed=ratio(0)),dict(id='resume',start_frame=36,frame_count=36,kind='play',source_start=ratio(1,2),speed=ratio(1))]);author['atmosphere_attachment']=dict(schema_version='ray_atmosphere_attachment_v1',id='atmosphere/main',source_kind='scene_bundle',source_path='assets/water/scene_bundle.json',enabled=True,visible=False,affects_lighting=False,appearance=dict(density_scale=0,opacity=0),playback=dict(clock='sequence_source',animated=True,rate_numerator=24,rate_denominator=1,timeline_start=0,source_start=0,missing='hold'))
path=root/'scene.json';path.write_text(json.dumps(scene));env=dict(os.environ,XDG_CONFIG_HOME=str(root/'config'),RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE='disabled',UBSAN_OPTIONS='halt_on_error=1')
def call(label,sample,frames=(),ok=True):
 cmd=[str(a.consumer),str(sample),'--clock-only',*map(str,frames)];r=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=300);(root/(label+'.stderr')).write_text(r.stderr);assert (r.returncode==0)==ok,(label,r.returncode,r.stderr[-2000:]);return [json.loads(l[4:]) for l in r.stdout.splitlines() if l.startswith('BR4 ')]
frames=[12,0,36,37,60,71,12,35];rows=call('samples',path,frames);expected=[120,0,120,130,360,470,120,120]
assert [r['source_tick'] for r in rows]==expected
assert [r['cache']['source_frame'] for r in rows]==[min(t//10,24) for t in expected]
assert all(r['cache']['kind']==2 for r in rows)
assert rows[0]['poses']==rows[2]['poses']==rows[6]['poses']==rows[7]['poses']
assert rows[0]['triangles'] and rows[0]['triangles']==rows[7]['triangles']
assert rows[0]['cache']==rows[2]['cache']==rows[6]['cache']==rows[7]['cache']
assert rows[2]['cache']!=rows[3]['cache'] and rows[2]['poses']!=rows[3]['poses']
assert rows[4]['cache']==rows[5]['cache'] and rows[4]['poses']!=rows[5]['poses']
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from sequence_water_schedule import expected_cache
for tick,row in zip(expected,rows):
    cache=expected_cache(scene,path,tick)
    assert cache['tick']==row['cache']['source_frame'] and cache['content_digest']==row['cache']['content_sha256']

for label,change in [('legacy-unmapped',lambda x:x['playback'].pop('clock')),('null-clock',lambda x:x['playback'].update(clock=None)),('unknown-clock',lambda x:x['playback'].update(clock='wall_time')),('visible-volume',lambda x:x.update(visible=True)),('wrong-rate',lambda x:x['playback'].update(rate_numerator=0))]:
 s=copy.deepcopy(scene);change(s['extensions']['ray_tracing']['authoring']['atmosphere_attachment']);f=root/(label+'.json');f.write_text(json.dumps(s));call(label,f,[12],False)
result=dict(passed=True,revision=revision,bodies=len(rows[0]['poses']),presentation_frames=frames,source_ticks=expected,water_frames=[r['cache']['source_frame'] for r in rows],exact_hold_rigid_and_water=True,resume_advances_both=True,end_hold_explicit=True,negative_controls=5,forged_water_frame_refused=True,input_hashes={str(f.relative_to(a.water)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files if f.is_file()});(root/'verification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({k:v for k,v in result.items() if k!='input_hashes'}))

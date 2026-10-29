#!/usr/bin/env python3
"""Render an existing BR6 fixture at exact 30fps presentation times.

Creates a new presentation-only scene and request beside its contained result.
Produces a full sequence plus per-selected-frame visibility/provenance audits.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess


def write(path,value):path.write_text(json.dumps(value,indent=2)+'\n')
def load(path):return json.loads(path.read_bytes())


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--qualification',type=Path,required=True)
    p.add_argument('--case',choices=('asymmetric-drop','asymmetric-pillar','asymmetric-throw','prism-drop','prism-pillar','prism-throw'),required=True)
    p.add_argument('--renderer',type=Path,required=True)
    a=p.parse_args();root=a.qualification.resolve();case=root/a.case
    limits=load(root/'declared-limits.json');scene=load(case/'scene.json')
    metrics=load(root/(a.case+'-metrics.json'));proof=case/'visual';proof.mkdir()
    scene['extensions']['ray_tracing'].setdefault('authoring',{})['scene_timeline']={
        'version':1,'rate':{'numerator':limits['render_rate_hz'],'denominator':1},
        'range':{'start_frame':0,'frame_count':limits['render_frame_count']},'tracks':[]}
    path=case/'scene_30fps.json';write(path,scene)
    env=dict(os.environ,XDG_CONFIG_HOME=str(proof/'config'),
        RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_MODE='disabled',
        RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_ROOT=str(proof/'mesh-cache'))
    def run_request(start,count,name,render):
        output=proof/name
        request={'schema_version':'ray_tracing_agent_render_request_v1','run_id':a.case+'_'+name,
          'scene':{'runtime_scene_path':str(path)},'volume':{'enabled':False},
          'render':{'start_frame':start,'frame_count':count,'width':limits['render_width'],
                    'height':limits['render_height'],'integrator_3d':'direct_light','temporal_frames':1,
                    'normalized_t':start/(limits['render_frame_count']-1)},
          'inspection':{'preset':'glass_preview','environment_light_mode':'ambient','ambient_strength':.30,
              'environment_preset':'sky','background_brightness':.42,'background_color':{'r':.9,'g':.95,'b':1.}},
          'output':{'root':str(output),'overwrite':False},
          'progress':{'summary_path':str(proof/(name+'-summary.json')),'progress_path':str(proof/(name+'-progress.json'))}}
        request_path=proof/(name+'-request.json');write(request_path,request)
        with (proof/(name+'.log')).open('w') as log:
            result=subprocess.run([str(a.renderer),'--request',str(request_path),'--render' if render else '--preflight'],cwd=Path(__file__).resolve().parents[1],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
        assert result.returncode==0,(name,result.returncode)
        summary=load(proof/(name+'-summary.json'))
        if render:assert summary['frames_rendered']==count
        return summary
    # Full temporal sequence. This is physical-time playback, not interpolation.
    summary=run_request(0,limits['render_frame_count'],'sequence',True)
    frames=list((proof/'sequence/frames').glob('frame_*.bmp'))
    assert len(frames)==limits['render_frame_count']
    assert len({p.read_bytes() for p in frames})>3,'moving scene must visibly change'
    impact=metrics['contact_ticks'][0]//8
    selected=sorted(set([0,max(0,impact-1),impact,min(60,impact+1),30,45,60]))
    checks=[]
    for frame in selected:
        audit=run_request(frame,1,'audit_'+str(frame),False)
        row=next(o for o in audit['object_audit'] if o['object_id']=='projectile')
        pose=next(o for o in audit['evaluated_objects'] if o['object_id']=='projectile')
        assert row['primary_hit_pixels']>=limits['minimum_projectile_primary_hit_pixels'],(a.case,frame,row)
        assert pose['source_tick']==frame*8 and pose['result_revision']==metrics['result_revision']
        checks.append({'frame':frame,'source_tick':pose['source_tick'],'projectile_primary_hit_pixels':row['primary_hit_pixels']})
    report={'ok':True,'case':a.case,'frame_count':len(frames),'simulation_duration_s':2,
            'source_tick_step':8,'selected_audits':checks,'visual_review':'pending',
            'intent':'fixed-camera source-mesh motion, static pillars/floor, before/at/after collision; no contact overlays',
            'rejection':'missing/occluded moving body, frozen sequence, framing loss, or incorrect exact source tick'}
    write(proof/'verification.json',report);print(json.dumps(report));return 0


if __name__=='__main__':raise SystemExit(main())

"""Native inspection freeze/orbit/resume qualification over a saved UI fixture."""
import argparse,copy,hashlib,json,math,os,random,shutil,subprocess,sys
from pathlib import Path
from fractions import Fraction

def main():
    p=argparse.ArgumentParser()
    for key in ['base','out','renderer','probe']:p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();a.out.mkdir(exist_ok=False);root=a.out/'fixture';shutil.copytree(a.base,root)
    repo=Path(__file__).resolve().parents[2];sys.path.insert(0,str(repo/'tools'));from sequence_video import assemble
    scene_path=root/'scene_runtime.json';scene=json.loads(scene_path.read_text());author=scene['extensions']['ray_tracing']['authoring'];move=author['inspection_moves'][0];target=move['target_position'];anchor=move['anchor']['position'];radius=math.hypot(anchor[0]-target[0],anchor[1]-target[1]);pins={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in root.rglob('*') if p.is_file()}
    program=a.out/'program';program.mkdir();config=program/'data/runtime';config.mkdir(parents=True);(config/'animation_config.json').write_text(json.dumps(dict(spaceMode=1,windowWidth=128,windowHeight=84,inputRoot=str(program),outputRoot=str(program/'frames'))));(config/'scene_config.json').write_text('{}')
    env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(program),XDG_CONFIG_HOME=str(a.out/'config'),PYTHONDONTWRITEBYTECODE='1',RAY_TRACING_RUNTIME_MESH_ASSET_PACK_CACHE_ROOT=str(a.out/'cache'))
    logs=a.out/'logs';logs.mkdir()
    def run(name,cmd,ok=True):
        r=subprocess.run(list(map(str,cmd)),cwd=program,env=env,capture_output=True,text=True,timeout=300);(logs/(name+'.log')).write_text(r.stdout+r.stderr);assert (r.returncode==0)==ok,(name,r.returncode,r.stderr[-1400:]);return [json.loads(s[4:]) for s in r.stdout.splitlines() if s.startswith('BR4 ')]
    order=list(range(102));random.Random(5).shuffle(order);order=[15]+order+[0,15,50,86,87,101]
    rows=run('native',[a.probe,scene_path,*order]);by={r['frame']:r for r in rows}
    frozen=by[15]['poses'];geometry=by[15]['triangles'];comparisons=0
    for row in rows:
        f=row['frame'];tick=f*10 if f<15 else 150 if f<87 else 150+(f-87)*10
        assert row['source_tick']==tick
        if 15<=f<87:
            assert row['poses']==frozen and row['triangles']==geometry
            assert row['inspection_id']=='contact_orbit' and Fraction(*row['inspection_time'])==Fraction(f-15,24)
            assert abs(math.hypot(row['camera'][0]-target[0],row['camera'][1]-target[1])-radius)<1e-11
            assert row['camera'][2]==anchor[2];comparisons+=len(frozen)
    assert by[15]['camera']==by[86]['camera']==anchor
    assert any(x['position']!=y['position'] for x,y in zip(by[87]['poses'],by[101]['poses']))
    # A partial orbit keeps the new viewpoint while the simulation resumes.
    partial=copy.deepcopy(scene);pa=partial['extensions']['ray_tracing']['authoring'];pa['inspection_moves'][0]['angle']=math.pi/2
    for shot in pa['camera_rig']['shots']:
        if shot['start_frame']>=87:shot['camera_id']=move['camera_id'];shot['clip_start']=dict(numerator=0,denominator=1)
    partial_path=root/'partial.json';partial_path.write_text(json.dumps(partial,indent=2))
    post=run('partial-resume',[a.probe,partial_path,15,50,86,87,101]);end=[target[0]-(anchor[1]-target[1]),target[1]+anchor[0]-target[0],anchor[2]]
    for row in post[-3:]:
        assert all(abs(a-b)<1e-11 for a,b in zip(row['camera'],end)) and Fraction(*row['inspection_time'])==Fraction(71,24)
    assert [r['source_tick'] for r in post[-3:]]==[150,150,290]
    mutations={}
    def mutate(name,edit):
        s=copy.deepcopy(scene);edit(s['extensions']['ray_tracing']['authoring']);mutations[name]=s
    mutate('stale-camera',lambda a:a['camera_rig']['cameras'][0]['position'].__setitem__(0,9))
    mutate('wrong-target',lambda a:a['inspection_moves'][0]['target_position'].__setitem__(0,9))
    mutate('wrong-tick',lambda a:a['inspection_moves'][0].update(source_tick=151))
    mutate('wrong-hold',lambda a:a['inspection_moves'][0].update(start_frame=16))
    mutate('wrong-span',lambda a:a['inspection_moves'][0].update(frame_count=71))
    mutate('infeasible',lambda a:a['inspection_moves'][0].update(max_speed=.0001))
    mutate('unknown-target',lambda a:a['inspection_moves'][0].update(target_id='missing'))
    for name,changed in mutations.items():
        path=root/(name+'.json');path.write_text(json.dumps(changed));run(name,[a.probe,path,15],False);run(name+'-preserved',[a.probe,scene_path,'--replacement',path])
    # The boundary camera is independently rederived on use, never trusted JSON.
    changed=copy.deepcopy(scene);changed['extensions']['ray_tracing']['authoring']['inspection_moves'][0]['anchor']['yaw']+=.1;path=root/'forged-anchor.json';path.write_text(json.dumps(changed));run('forged-anchor',[a.probe,path,15],False)
    request=json.loads((a.base.parent/'shots-request.json').read_text()) if (a.base.parent/'shots-request.json').exists() else json.loads((repo.parents[1]/'_private_workspace_artifacts/ray_tracing/sequence_stage2_20261030/proof/request.json').read_text())
    request['scene']=dict(runtime_scene_path=str(scene_path));request['render'].update(start_frame=12,frame_count=102,width=96,height=64,temporal_frames=1);out=a.out/'render';request['output']['root']=str(out);request['output']['video']['enabled']=False;request['progress']={k:str(out/Path(v).name) for k,v in request['progress'].items()}
    path=a.out/'request.json';path.write_text(json.dumps(request,indent=2));run('render',[a.renderer,'--request',path,'--render','--summary',a.out/'render.json','--summary-file-only'])
    assemble(scene_path,out/'frames',a.out/'inspection.mp4')
    receipts=[json.loads(p.read_text()) for p in sorted((out/'frames').glob('*.sequence.json'))]
    assert len(receipts)==102
    assert len({r['image_sha256'] for r in receipts[15:87]})>=70
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==v for p,v in pins.items())
    result=dict(ok=True,native_samples=len(rows)+len(post),exact_held_body_comparisons=comparisons,frozen_native_geometry=True,closed_endpoint_exact=True,partial_endpoint_held_after_resume=True,refused_cases=len(mutations)+1,native_frames=102,distinct_hold_images=len({r['image_sha256'] for r in receipts[15:87]}),duration='17/4',fps=24,exact_pts=True,source_unchanged=True)
    (a.out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()

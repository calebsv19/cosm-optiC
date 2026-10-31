"""Native event-view regression over a compact qualified six-body fixture.
No numerical solver is run and no installed application is changed.
"""
from pathlib import Path
import os,json,subprocess,shutil,sys,copy,math,random
import argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--base',type=Path,required=True,help='Compact portable Kinetic Machinery Spill fixture with the qualified tick-150 Hold')
parser.add_argument('--out',type=Path,required=True)
parser.add_argument('--probe',type=Path,required=True)
args=parser.parse_args()
base=args.base.resolve();R=args.out.resolve();R.mkdir(parents=True,exist_ok=False);S=R/'runtime';S.mkdir();root=R/'proof';root.mkdir();fixture=root/'fixture'
# Refuse an accidental broad project copy; the selected regression input is a
# compact portable fixture, not an arbitrary user's asset collection.
files=list(base.parent.rglob('*'))
if any(f.is_symlink() for f in files) or sum(f.stat().st_size for f in files if f.is_file())>64*1024*1024:
 raise ValueError('Use a compact portable fixture without symlinked inputs')
shutil.copytree(base.parent,fixture,ignore=lambda folder,names: [name for name in names if Path(folder)==base.parent and (Path(folder)/name).is_file() and name != base.name])
if base.name!='scene_runtime.json':shutil.copy2(fixture/base.name,fixture/'scene_runtime.json')
path=fixture/'scene_runtime.json';scene=json.loads(path.read_text());a=scene['extensions']['ray_tracing']['authoring'];a.pop('inspection_moves',None);rig=a['camera_rig'];rig['cameras']=rig['cameras'][:1];rig['shots']=[dict(id='baseline',camera_id='camera/A',start_frame=0,frame_count=102,clip_start=dict(numerator=0,denominator=1))];path.write_text(json.dumps(scene,indent=2)+'\n')
intent=dict(schema='ray_event_camera_intent_v1',id='bracket_event',hold=dict(start_frame=15,frame_count=72,source_tick=150),focus=dict(kind='body_com',targets=['copper_bracket']),pivot=dict(kind='body_com',targets=['copper_bracket']),framing=dict(fov_y=55,aspect=4/3,margin=.12,offset=[0,0],azimuth=.6,elevation=.45),orbit=dict(angle=math.pi*1.5,max_speed=30,acceleration=100,braking=100),lead=dict(kind='stationary',start_frame=0),resume=dict(position='endpoint',aim='track',end_frame=102))
intent_path=fixture/'intent.json';intent_path.write_text(json.dumps(intent,indent=2)+'\n')
program=S/'program';program.mkdir();runtime=program/'data/runtime';runtime.mkdir(parents=True);(runtime/'animation_config.json').write_text(json.dumps(dict(editorMode=1,spaceMode=1,windowWidth=1200,windowHeight=800)));(runtime/'scene_config.json').write_text(json.dumps(dict(window=dict(width=1200,height=800))))
env=dict(os.environ,RAY_TRACING_PROGRAM_ROOT=str(program),XDG_CONFIG_HOME=str(S/'config'),PYTHONDONTWRITEBYTECODE='1')
def run(label,args,ok=True):
 with (root/(label+'.log')).open('w') as f:p=subprocess.run([str(x) for x in args],cwd=program,env=env,stdout=f,stderr=subprocess.STDOUT)
 assert (p.returncode==0)==ok,(label,p.returncode,(root/(label+'.log')).read_text()[-2500:])
 return (root/(label+'.log')).read_text()
probe=args.probe.resolve();proposals=fixture/'proposals.json';run('propose',[probe,path,'--propose',intent_path,proposals]);candidates=json.loads(proposals.read_text())['candidates'];print('Generated three event candidates',flush=True)
usable=[c for c in candidates if c['usable']];assert usable,'No clear candidate';chosen=min(usable,key=lambda c:c['possible_occlusion_samples']);chosen_index=chosen['index']
selected=fixture/'selected.json';run('select',[probe,path,'--select',intent_path,chosen_index,chosen['proposal_sha256'],selected])
order=list(range(102));random.Random(19).shuffle(order);order += [0,15,86,87,101]
rows=[json.loads(l[6:]) for l in run('samples',[probe,selected,*order]).splitlines() if l.startswith('EVENT ')];by={r['frame']:r for r in rows};assert all(r==by[r['frame']] for r in rows)
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def projection(row,target):
 ray=[x-y for x,y in zip(target,row['position'])];z=dot(ray,row['forward']);t=math.tan(math.radians(row['fov_y'])/2);return dot(ray,row['right'])/(z*t*row['aspect']),dot(ray,row['up'])/(z*t)
for frame,row in by.items():
 target=chosen['focus'] if frame<87 else next(b['position'] for b in row['bodies'] if b['id']=='copper_bracket')
 assert max(abs(x) for x in projection(row,target))<1e-9
assert all(by[i]['position']==by[0]['position'] and by[i]['forward']==by[0]['forward'] for i in range(16))
assert next(b['position'] for b in by[0]['bodies'] if b['id']=='copper_bracket')!=chosen['focus']
assert by[86]['position']==by[87]['position'] and by[86]['forward']==by[87]['forward']
assert by[101]['position']==by[87]['position'] and by[101]['forward']!=by[87]['forward']
assert by[86]['position']!=by[0]['position']
assert all(by[i]['bodies']==by[15]['bodies'] for i in range(15,88))
# Closed orbit + return-entry uses an exact endpoint; event aim stays fixed after Resume.
closed=copy.deepcopy(intent);closed['id']='closed_event';closed['orbit']['angle']=2*math.pi;closed['resume'].update(position='entry',aim='event');cp=fixture/'closed-intent.json';cp.write_text(json.dumps(closed));props=fixture/'closed-proposals.json';run('closed-propose',[probe,path,'--propose',cp,props]);v=next(c for c in json.loads(props.read_text())['candidates'] if c['usable']);closed_path=fixture/'closed.json';run('closed-select',[probe,path,'--select',cp,v['index'],v['proposal_sha256'],closed_path]);closed_rows=[json.loads(l[6:]) for l in run('closed-samples',[probe,closed_path,0,15,86,87,101]).splitlines() if l.startswith('EVENT ')];assert all(r['position']==closed_rows[0]['position'] and r['forward']==closed_rows[0]['forward'] for r in closed_rows)
# Every focus form resolves a real recorded view. Pair COM is independently checked with recorded masses.
refs=[dict(kind='pair_com',targets=['copper_bracket','blue_channel']),dict(kind='pair_bounds',targets=['copper_bracket','blue_channel']),dict(kind='body_side',targets=['copper_bracket'],side='x+',radius=.3),dict(kind='world_point',point=chosen['focus'],radius=.4)]
# Pick an actual mesh source vertex, not a guessed coordinate.
obj=next(o for o in scene['objects'] if o['object_id']=='copper_bracket');visible=json.loads((fixture/obj['extensions']['line_drawing']['runtime_mesh_path']).read_text());v=visible['mesh']['vertices'][0];refs.append(dict(kind='body_point',targets=['copper_bracket'],point=[v[k] for k in ['x','y','z']],radius=.35))
for index,ref in enumerate(refs):
 request=copy.deepcopy(intent);request.update(id='focus_'+str(index),focus=ref,pivot=ref);request['framing']['offset']=[.15,-.1];ip=fixture/f'focus-{index}.json';ip.write_text(json.dumps(request));pp=fixture/f'focus-{index}-proposals.json';run('focus-'+str(index),[probe,path,'--propose',ip,pp]);options=[c for c in json.loads(pp.read_text())['candidates'] if 'position' in c];assert options
 if index==0:
  masses={b['object_id']:b['mass_kg'] for b in scene['extensions']['rigid_scene']['bindings']};positions={b['id']:b['position'] for b in by[15]['bodies']};m=sum(masses[t] for t in ref['targets']);expected=[sum(masses[t]*positions[t][k] for t in ref['targets'])/m for k in range(3)];assert max(abs(x-y) for x,y in zip(options[0]['focus'],expected))<1e-12
# Native admission must reject stale or forged plans atomically.
selected_scene=json.loads(selected.read_text());mutations={}
def mutate(name,fn):
 s=copy.deepcopy(selected_scene);fn(s['extensions']['ray_tracing']['authoring']);mutations[name]=s
mutate('forged-pose',lambda a:a['event_cameras'][0]['resolved']['position'].__setitem__(0,99))
mutate('stale-seed',lambda a:a['camera_rig']['cameras'][-1]['position'].__setitem__(0,99))
mutate('stale-hold',lambda a:a['event_cameras'][0]['intent']['hold'].update(source_tick=151))
mutate('stale-framing',lambda a:a['event_cameras'][0]['intent']['framing'].update(fov_y=70))
mutate('missing-target',lambda a:a['event_cameras'][0]['intent']['focus'].update(targets=['missing']))
mutate('bad-return',lambda a:a['event_cameras'][0]['intent']['resume'].update(position='entry'))
for name,s in mutations.items():
 bad=fixture/(name+'.json');bad.write_text(json.dumps(s));run(name,[probe,selected,'--replacement',bad])
run('stale-proposal',[probe,path,'--select',intent_path,chosen_index,'0'*64,fixture/'must-not-exist.json'],False)
assert not (fixture/'must-not-exist.json').exists()
(root/'result.json').write_text(json.dumps(dict(ok=True,native_samples=len(rows),centered_event_framing=True,stationary_future_event_lead=True,partial_endpoint_continuity=True,track_after_resume=True,closed_entry_exact=True,focus_variants=len(refs)+1,pair_com_mass_weighted=True,refusals=len(mutations)+1,candidate=chosen_index),indent=2)+'\n')
print('Native event camera qualification PASS',flush=True)

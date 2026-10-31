"""Qualify dolly lead-ins against the existing stationary event-camera oracle.
Uses a compact qualified recording; never runs or changes the physics solver.
"""
from pathlib import Path
import argparse, copy, json, math, os, random, subprocess, sys
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--base', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--probe', type=Path, required=True)
a = p.parse_args()
subprocess.run([sys.executable, '-B', str(Path(__file__).with_name('check_event_camera_runtime.py')), '--base', str(a.base), '--out', str(a.out), '--probe', str(a.probe)], check=True)
r = a.out.resolve(); root = r/'proof'; fixture = root/'fixture'; probe = a.probe.resolve()
program = r/'runtime/program'
env = dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(program), XDG_CONFIG_HOME=str(r/'runtime/config'))
def run(label, args, ok=True, extra=None):
    with (root/(label+'.log')).open('w') as f:
        x = subprocess.run([str(probe), *map(str,args)], cwd=program, env=dict(env, **(extra or {})), stdout=f, stderr=subprocess.STDOUT)
    log = (root/(label+'.log')).read_text()
    assert (x.returncode == 0) == ok, (label, x.returncode, log[-2500:])
    return [json.loads(l[6:]) for l in log.splitlines() if l.startswith('EVENT {')]
def write(name, value):
    f=fixture/(name+'.json'); f.write_text(json.dumps(value, indent=2)+'\n'); return f
scene=fixture/'scene_runtime.json'; intent=json.loads((fixture/'intent.json').read_text())
intent['id']='moving_event'; intent['lead'].update(kind='dolly',distance_scale=1.5,max_speed=30,acceleration=100,braking=100)
def select(label, value):
    ip=write(label+'-intent',value); pp=fixture/(label+'-proposals.json')
    run(label+'-propose',[scene,'--propose',ip,pp])
    candidates=json.loads(pp.read_text())['candidates']; c=candidates[2]; assert c['usable'],c
    dest=fixture/(label+'.json');run(label+'-select',[scene,'--select',ip,c['index'],c['proposal_sha256'],dest]);return dest,c
selected,c=select('dolly',intent)
order=list(range(102));random.Random(21).shuffle(order);order += [0,15,86,87,101]
rows=run('dolly-samples',[selected,*order]); by={x['frame']:x for x in rows}; assert all(x==by[x['frame']] for x in rows)
old=[json.loads(l[6:]) for l in (root/'samples.log').read_text().splitlines() if l.startswith('EVENT {')]; baseline={x['frame']:x for x in old}
keys=['position','forward','right','up','yaw','pitch','fov_y','aspect']
for f,x in by.items():
    assert x['tick']==baseline[f]['tick'] and x['bodies']==baseline[f]['bodies']
    if f>=15: assert all(x[k]==baseline[f][k] for k in keys),(f,x,baseline[f])
assert by[0]['position']==c['lead_position'] and by[15]['position']==c['position']
def dist(x,y):return math.sqrt(sum((v-w)**2 for v,w in zip(x,y)))
assert all(dist(by[i]['position'],c['position']) > dist(by[i+1]['position'],c['position']) for i in range(15))
assert abs(c['lead_duration']-.625)<1e-12
half=run('dolly-subframes',[selected,0,7,14,15,45,86,87],extra={'OPTIC_EVENT_HALF_FRAME':'1','OPTIC_EVENT_EVALUATE_ONLY':'1'})
assert len(half)==7
assert all(x['bodies']==by[15]['bodies'] for x in half if 15<=x['frame']<87)
# Current mesh render preparation refuses fractional presentation samples.
# Evaluate those samples independently and preserve the renderer refusal.
for frame in [0,15,87]:
    run('fractional-source-'+str(frame),[selected,frame],False,{'OPTIC_EVENT_HALF_FRAME':'1'})
    assert 'exact covered tick' in (root/('fractional-source-'+str(frame)+'.log')).read_text()
# Delayed start remains fixed before the selected lead interval.
delayed=copy.deepcopy(intent);delayed['id']='delayed_event';delayed['lead']['start_frame']=3
ds,dc=select('delayed',delayed);dr=run('delayed-samples',[ds,'--view','camera/event_delayed_event',0,1,2,3,4,15]);assert all(x['position']==dc['lead_position'] for x in dr[:4]);assert dr[4]['position']!=dc['lead_position'];assert dr[-1]['position']==dc['position']
# Detached evaluation and another selected camera must not advance/reset this lead.
view=run('dolly-view',[selected,'--view','camera/event_moving_event',0,7,14,15,86,87,101])
assert len(view)==7
for x in view:assert all(x[k]==by[x['frame']][k] for k in keys)
for name,change in [
 ('scale-one',dict(distance_scale=1)),('scale-too-large',dict(distance_scale=101)),
 ('unknown-field',dict(unrecognized=True)),('infeasible',dict(max_speed=1e-9)),('zero-span',dict(start_frame=15))]:
    bad=copy.deepcopy(intent);bad['lead'].update(change); ip=write(name,bad); pp=fixture/(name+'-must-not-exist.json')
    if name == 'infeasible':
        run(name,[scene,'--propose',ip,pp])
        assert all(not c['usable'] and 'reason' in c for c in json.loads(pp.read_text())['candidates'])
        no=fixture/'infeasible-selection-must-not-exist.json'
        run('infeasible-select',[scene,'--select',ip,2,'0'*64,no],False);assert not no.exists()
    else:
        run(name,[scene,'--propose',ip,pp],False);assert not pp.exists()
# Cuts select views without stopping or restarting presentation-driven lead motion.
cut=json.loads(selected.read_text());rig=cut['extensions']['ray_tracing']['authoring']['camera_rig']
rig['shots']=[dict(id='cut'+str(i),camera_id=cam,start_frame=start,frame_count=count,clip_start=dict(numerator=0,denominator=1)) for i,(start,count,cam) in enumerate([(0,5,'camera/A'),(5,5,'camera/event_moving_event'),(10,3,'camera/A'),(13,89,'camera/event_moving_event')])]
cutpath=write('cut-dolly',cut)
cutrows=run('cut-dolly',[cutpath,5,7,9,13,14,15,86,87,101])
for x in cutrows: assert all(x[k]==by[x['frame']][k] for k in keys)
# Resolved dolly metadata is re-derived, never trusted from a saved file.
saved=json.loads(selected.read_text())
for name,mutate in [
 ('forged-lead',lambda a:a['event_cameras'][0]['resolved']['lead_position'].__setitem__(0,99)),
 ('stale-lead',lambda a:a['event_cameras'][0]['intent']['lead'].update(distance_scale=1.6))]:
    bad=copy.deepcopy(saved);mutate(bad['extensions']['ray_tracing']['authoring']); bp=write(name,bad);run(name,[selected,'--replacement',bp])
run('dolly-stale-proposal',[scene,'--select',fixture/'dolly-intent.json',2,'0'*64,fixture/'stale-must-not-exist.json'],False)
assert not (fixture/'stale-must-not-exist.json').exists()
(root/'lead-result.json').write_text(json.dumps(dict(ok=True,native_samples=len(rows),subframes=len(half),delayed_start=True,detached_view=True,cuts_do_not_retime=True,body_and_source_tick_equivalence=True,hold_and_resume_camera_equivalence=True,refusals=11,chosen=2),indent=2)+'\n')
print('Native moving lead qualification PASS')

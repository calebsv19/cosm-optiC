"""Saved-profile sampler parsing, observational audit parity and trace replay."""
import argparse,copy,json,subprocess,sys
from pathlib import Path
W=Path(__file__).resolve().parents[2];sys.path.insert(0,str(W/'tools'));sys.path.insert(0,str(W/'tests/integration'))
from test_headless_spectral_surface_path import setup
from spectral_room_testbench import plane,v,material
from spectral_components import verify,read

def qualify(cli,root):
 root.mkdir(parents=True,exist_ok=False);(root/'config').symlink_to(W/'config',target_is_directory=True)
 q,scene=setup();p=q.pop('spectral')['surface_path_v1'];p.update(samples=64,max_depth=1,materials=[material('receiver',.8)])
 scene['objects']=[plane('receiver',(-100,0,0),(0,1,0),(0,0,1),(1,0,0),.001,.001)]
 p['light'].update(origin_m=[1,0,0],axis_u=[0,1,0],axis_v=[0,0,-1],width_m=2,height_m=2)
 q['inspection'].update(camera_position=v(.1,0,0),camera_look_at=v(1,0,0),camera_zoom=1000)
 scene['extensions']={'ray_tracing':{'spectral_transport_v1':{'surface_path_v1':p}}}
 def run(name,changes,valid=True):
  sc=copy.deepcopy(scene);sc['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1'].update(changes)
  req=copy.deepcopy(q);req.update(run_id=name,scene={'runtime_scene_path':name+'.scene.json'},output={'root':name,'overwrite':False})
  (root/(name+'.scene.json')).write_text(json.dumps(sc));(root/(name+'.request.json')).write_text(json.dumps(req))
  process=subprocess.run([str(cli),'--request',name+'.request.json','--render','--summary',name+'.summary.json','--summary-file-only'],cwd=root,capture_output=True,timeout=60)
  assert (process.returncode==0)==valid,(name,process.stderr)
  if not valid:assert not (root/name/'frames/frame_0000.bmp').exists();return
  report=json.loads((root/(name+'.summary.json')).read_text())['spectral_transport'];image=root/name/'frames/frame_0000.bmp';verify(report,image)
  return report,Path(str(image)+'.components.npy').read_bytes()
 baseline,raw=run('baseline',{})
 assert baseline['quality']['camera_sampler']=='hashed_v1' and not baseline['sample_audit']['enabled']
 legacy,b=run('explicit',{'camera_sampler':'hashed_v1'});assert raw==b
 audited,b=run('audited',{'sample_audit':{'x':0,'y':0,'limit':16}});assert raw==b
 audit=audited['sample_audit'];assert audit['observed_samples']==64 and len(audit['top_samples'])==16
 xyz=audited['film_components_xyz']['total'];assert all(abs(a-b)<1e-12 for a,b in zip(audit['mean_xyz'],xyz))
 top=audit['top_samples'][0];traced,b=run('traced',{'sample_audit':{'x':0,'y':0,'limit':16},'path_trace':{'x':0,'y':0,'sample':top['sample']}})
 assert raw==b and traced['path_trace']['events'][0]['vacuum_nm']==top['vacuum_nm']
 candidate,b=run('candidate',{'camera_sampler':'scrambled_path_v1','sample_audit':{'x':0,'y':0}})
 assert candidate['quality']['camera_sampler']=='scrambled_path_v1' and candidate['sample_audit']['observed_samples']==64
 assert b==raw # All primary rays see the same broad, unoccluded emitter.
 for i,bad in enumerate([True,17,'unknown','sobol_pairs_v1','hashed_v1\0hidden']):run('bad_sampler'+str(i),{'camera_sampler':bad},False)
 for i,bad in enumerate([{'x':0,'y':0,'limit':0},{'x':0,'y':0,'limit':17},{'x':0,'y':0,'limit':True},{'x':1,'y':0},{'x':-1,'y':0},{'x':0,'y':0,'unknown':1},False]):run('bad_audit'+str(i),{'sample_audit':bad},False)
 print('PASS saved-profile strict parsing, legacy/default parity, audit toggle parity, component mean and selected path replay')
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--cli',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();qualify(a.cli.resolve(),a.output.resolve())

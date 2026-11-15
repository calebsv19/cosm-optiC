"""Public V3 reconstruction selection, physical-ledger and angular checks."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[2]

def qualify(cli, root):
    (root/'config').symlink_to(REPO/'config')
    (root/'data/runtime/videos').mkdir(parents=True)
    q=json.loads((REPO/'tests/fixtures/spectral_volume/request.json').read_text())
    q['scene']['runtime_scene_path']=str(REPO/'tests/fixtures/spectral_prism/scene_runtime.json')
    q['render'].update(width=1,height=1)
    q['output'].update(root=str(root/'out'),overwrite=True)
    q['spectral'].update(glass_id='prism',glass='schott_n_bk7')
    q['spectral']['surface_map']['photons_per_source']=8192
    v=q['spectral']['participating_volume_v1']
    def run(valid=True):
        (root/'request.json').write_text(json.dumps(q))
        p=subprocess.run([cli,'--request',root/'request.json','--render','--summary',root/'summary.json','--summary-file-only'],cwd=root,capture_output=True)
        assert (p.returncode==0)==valid,(p.returncode,p.stderr.decode()[-400:])
        return json.loads((root/'summary.json').read_text()) if valid else None
    def physical(j):
        return {field:[{k:v for k,v in row.items() if not k.startswith('center_')} for row in j[field]['bands']] for field in ('spectral_volume','spectral_surface_map')}
    legacy=run();v['reconstruction']='box';box=run()
    assert physical(box)==physical(legacy)
    assert box['spectral_center_pixel_xyz']==legacy['spectral_center_pixel_xyz']
    settings=q['spectral']['surface_map']
    settings['gather']='fixed'
    fixed=run()
    assert fixed['spectral_center_pixel_xyz']==box['spectral_center_pixel_xyz']
    assert fixed['spectral_surface_map']['expanded_deposits']==0
    settings['gather']='adaptive_scattered'
    adaptive=run()
    assert adaptive['spectral_surface_map']['gather']=='adaptive_scattered'
    assert adaptive['spectral_surface_map']['expanded_deposits']>0
    assert adaptive['spectral_surface_map']['key']!=fixed['spectral_surface_map']['key']
    assert physical(adaptive)==physical(fixed)
    assert run()['spectral_center_pixel_xyz']==adaptive['spectral_center_pixel_xyz']
    # A coincident second plane must keep the footprint at the base radius.
    scene_path=Path(q['scene']['runtime_scene_path'])
    scene=json.loads(scene_path.read_text())
    plane=dict(next(o for o in scene['objects'] if o['object_id']=='screen'),object_id='nearby_screen')
    scene['objects'].append(plane)
    (root/'assets').symlink_to(scene_path.parent/'assets')
    (root/'overlap.json').write_text(json.dumps(scene))
    q['scene']['runtime_scene_path']=str(root/'overlap.json')
    overlap=run()
    assert overlap['spectral_surface_map']['deposits']>0
    assert overlap['spectral_surface_map']['expanded_deposits']==0
    q['scene']['runtime_scene_path']=str(scene_path)
    for bad in ('adaptive', 'adaptive_scattered\0ignored', None, 1):
        settings['gather']=bad
        run(False)
    del settings['gather']
    for mode in ('continuous','adaptive'):
        v.update(reconstruction=mode,phase_tolerance=.01)
        approx=run();v['phase_tolerance']=0;exact=run()
        assert physical(approx)==physical(exact)==physical(legacy)
        a=approx['spectral_center_pixel_xyz'];b=exact['spectral_center_pixel_xyz']
        for x,y in zip(a,b):
            assert abs(x-y)<=.01000001*abs(y)+1e-12,(mode,a,b)
        assert exact['spectral_reconstruction']['cached_node_queries']==0
        assert exact['spectral_reconstruction']['exact_node_queries']>0
        assert exact['spectral_reconstruction']['owned_bytes']<=256*1024**2
        print(mode,'phase bound and unchanged physical ledgers',approx['spectral_reconstruction'])
    # Finer evaluation spacing must leave transport and omitted/default output stable.
    original=run();v['reconstruction_cell_m']=v['cell_m'];equal=run()
    assert equal['spectral_center_pixel_xyz']==original['spectral_center_pixel_xyz']
    v['reconstruction_cell_m']=v['cell_m']*.5
    finer=run();assert physical(finer)==physical(original)
    assert finer['spectral_reconstruction']['links']==0
    assert finer['spectral_reconstruction']['owned_bytes']<=256*1024**2
    for bad in [0,-1,v['cell_m']*.49,v['cell_m']*1.01,'bad',None]:
        v['reconstruction_cell_m']=bad;run(False)
    del v['reconstruction_cell_m']
    for key,bad in [('reconstruction','unknown'),('reconstruction','adaptive\0bad'),('phase_tolerance',-.01),('phase_tolerance',.051)]:
        good=v[key];v[key]=bad;run(False);v[key]=good
    v['reconstruction']='box';run(False) # tolerance has no meaning in box mode
    del v['phase_tolerance'];v['reconstruction']='adaptive'
    print('PASS V3 reconstruction request, phase bound and physical ledgers')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--cli',type=Path,required=True)
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='optic-reconstruction-') as path:qualify(args.cli.resolve(),Path(path))

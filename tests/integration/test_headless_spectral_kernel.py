"""Native full-medium kernel contract: parser, transport identity and local refinement."""
import argparse,copy,json,subprocess,tempfile,array,struct,hashlib,sys
from pathlib import Path
REPO=Path(__file__).resolve().parents[2]
def qualify(cli,root):
    (root/'config').symlink_to(REPO/'config');(root/'data/runtime/videos').mkdir(parents=True)
    q=json.loads((REPO/'tests/fixtures/spectral_volume/request.json').read_text());q['scene']['runtime_scene_path']=str(REPO/'tests/fixtures/spectral_prism/scene_runtime.json');q['render'].update(width=4,height=4);q['output'].update(root=str(root/'out'),overwrite=True);q['spectral'].update(glass_id='prism',glass='schott_n_bk7');q['spectral']['surface_map']['photons_per_source']=2048
    v=q['spectral']['participating_volume_v1'];v['reconstruction']='adaptive'
    def run(valid=True):
        (root/'request.json').write_text(json.dumps(q));p=subprocess.run([str(cli),'--request',str(root/'request.json'),'--render','--summary',str(root/'summary.json'),'--summary-file-only'],cwd=root,capture_output=True)
        assert (p.returncode==0)==valid,(p.returncode,p.stderr[-300:])
        if valid:
            s=json.loads((root/'summary.json').read_text());assert s['frames_rendered']==1,s['diagnostics'];return s
    def ledger(s):return {k:[{a:b for a,b in row.items() if not a.startswith('center_')} for row in s[k]['bands']] for k in ['spectral_volume','spectral_surface_map']}
    baseline=run();v['reconstruction']='kernel';kernel=run();assert ledger(baseline)==ledger(kernel)
    v['higher_scatter_support_m']=v['cell_m']*4;v['local_refinement_v1']={'bounds_min_m':[-2,-2,-2],'bounds_max_m':[2,2,2],'step_m':v['cell_m']/4}
    local=run();assert ledger(local)==ledger(kernel);assert local['spectral_reconstruction']['links']==0;assert local['spectral_reconstruction']['peak_bytes']<=256*2**20
    settings=q['spectral']['surface_map'];settings['gather']='adaptive_scattered'
    radius=q['spectral']['receiver_radius_m'];settings['scattered_support_m']=radius*8;settings['scattered_neighbors']=16
    wide=run();assert ledger(wide)==ledger(kernel)
    for key,value in [('scattered_support_m',None),('scattered_support_m',radius/2),('scattered_support_m',radius*65),('scattered_neighbors',3),('scattered_neighbors',65),('scattered_neighbors','many')]:
        saved=copy.deepcopy(settings);settings[key]=value;run(False);settings.clear();settings.update(saved)
    settings['gather']='fixed';run(False);settings['gather']='adaptive_scattered';settings.pop('scattered_support_m');settings.pop('scattered_neighbors')
    good=copy.deepcopy(v)
    for field,value in [('higher_scatter_support_m',None),('higher_scatter_support_m',0),('higher_scatter_support_m',v['cell_m']/2),('higher_scatter_support_m',v['cell_m']*65),('local_refinement_v1',None),('local_refinement_v1',{'step_m':.001}),('reconstruction_cell_m',v['cell_m']/2)]:
        v.clear();v.update(copy.deepcopy(good));v[field]=value;run(False)
    for field,value in [('step_m',0),('step_m',v['cell_m']*2),('step_m','fine'),('bounds_max_m',[-3,-3,-3]),('unexpected',1)]:
        v.clear();v.update(copy.deepcopy(good));v['local_refinement_v1'][field]=value;run(False)
    v.clear();v.update(copy.deepcopy(good));v['reconstruction']='adaptive';run(False)
    # Portable regression for the actual failure: keep the large VF3D and camera31.
    nx,ny,nz=90,75,8
    def smooth(t):
        t=max(0,min(1,t));return t*t*(3-2*t)
    density=array.array('f',(smooth(min(x,nx-1-x)/5)*smooth(min(y,ny-1-y)/5)*smooth(min(z,nz-1-z)/2) for z in range(nz) for y in range(ny) for x in range(nx)))
    if sys.byteorder!='little':density.byteswap()
    frame=root/'full-apparatus.vf3d'
    frame.write_bytes(struct.pack('<5I4xdQd7fI3I4x',0x56463344,1,nx,ny,nz,0.,0,.1,-.5,-.7,-.08,.02,0,0,1,0,0,0,0)+density.tobytes()+bytes(nx*ny*nz*17))
    q['inspection']={'camera_position':dict(zip('xyz',[.86,.2899999983885148,.16000000000000003])),'camera_look_at':dict(zip('xyz',[1,.265,0])),'camera_zoom':2}
    q['spectral']={'glass_id':'prism','receiver_id':'screen','glass':'schott_n_bk7','receiver_radius_m':.0011,'exposure':.24,
        'surface_map':{'enabled':True,'photons_per_source':2048,'bins':24,'seed':19,'absorption_per_m':0,'gather':'adaptive_scattered','scattered_support_m':.032,'scattered_neighbors':16,'sources':[{'origin_m':[-.12855752193730788,-.1532088886237956,0],'direction':[.6427876096865394,.766044443118978,0],'aperture_m':.00085,'lines':[],'continuum':[[380,.0025],[780,.0025]]}]},
        'participating_volume_v1':{'kind':'vf3d_density','source_path':str(frame),'frame_index':0,'sigma_a_per_m':[[380,.01],[780,.01]],'sigma_s_per_m':[[380,.58],[780,.4]],'g':.35,'cell_m':.002,'reconstruction':'kernel','higher_scatter_support_m':.016,'local_refinement_v1':{'bounds_min_m':[.8,.19,-.012],'bounds_max_m':[1.03,.31,.012],'step_m':.00025}}}
    full=run();assert full['spectral_volume']['source_sha256']==hashlib.sha256(frame.read_bytes()).hexdigest()
    for key in ['camera_position','camera_look_at']:assert all(abs(full['inspection'][key][a]-q['inspection'][key][a])<1e-9 for a in 'xyz')
    assert full['spectral_reconstruction']['links']==0
    v=q['spectral']['participating_volume_v1'];v['reconstruction']='adaptive';v.pop('higher_scatter_support_m');v.pop('local_refinement_v1');q['spectral']['surface_map'].pop('scattered_support_m');q['spectral']['surface_map'].pop('scattered_neighbors')
    assert ledger(run())==ledger(full)
    print('PASS full-apparatus VF3D and actual camera31 retained across reconstruction changes')
    print('PASS kernel public request, unchanged transport, local field, strict invalid/type/range/mode refusal')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--cli',type=Path,required=True);a=p.parse_args()
    with tempfile.TemporaryDirectory(prefix='optic-kernel-api-') as d:qualify(a.cli.resolve(),Path(d))

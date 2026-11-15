"""Native V3 convergence, legacy parity, jitter, and camera-reset regressions."""
import argparse
import copy
import hashlib
import json
import subprocess
from pathlib import Path

REPO=Path(__file__).resolve().parents[2]

def qualify(cli,root):
    root.mkdir(parents=True,exist_ok=False)
    (root/'config').symlink_to(REPO/'config')
    (root/'data/runtime/videos').mkdir(parents=True)
    q=json.loads((REPO/'tests/fixtures/spectral_surface_map/request.json').read_text())
    q['scene']['runtime_scene_path']=str(REPO/'tests/fixtures/spectral_prism/scene_runtime.json')
    q['render'].update(width=48,height=32)
    q['spectral']['surface_map'].update(photons_per_source=2048,gather='adaptive_scattered')
    q['output']=dict(root=str(root/'frames'),overwrite=True)
    def run(name,request,valid=True):
        path=root/(name+'.request.json');summary=root/(name+'.json');path.write_text(json.dumps(request))
        with (root/(name+'.log')).open('w') as log:
            p=subprocess.run([cli,'--render','--request',path,'--summary',summary,'--summary-file-only'],cwd=root,stdout=log,stderr=log,timeout=240)
        assert (p.returncode==0)==valid,(name,p.returncode)
        if not valid:return
        image=(root/'frames/frames/frame_0000.bmp').read_bytes();(root/(name+'.bmp')).write_bytes(image)
        return json.loads(summary.read_text()),hashlib.sha256(image).hexdigest()
    legacy,old_hash=run('legacy',q)
    q['spectral']['sampling']=dict(mode='single')
    assert run('single',q)[1]==old_hash
    q['spectral']['sampling']=dict(mode='converge',max_passes=16,visible_error=255,edge_error=255,local_error=255,energy_error=1)
    first,first_hash=run('converged',q)
    s=first['spectral_sampling'];assert s['status']=='converged' and s['passes']==16 and s['consecutive']==2
    assert s['full_passes']==16 and s['full_checkpoints']==2 and s['edge_pixels']>0
    assert s['min_pixel_samples']==s['max_pixel_samples']==16
    assert s['camera_samples']==16*48*32
    full=copy.deepcopy(q);full['spectral']['sampling']['camera_coverage']='full'
    reference,reference_hash=run('full-reference',full)
    assert reference_hash==first_hash and reference['spectral_sampling']['camera_coverage']=='full'
    assert first['spectral_execution']['temporal_passes']==16
    again,h=run('repeated',q);assert h==first_hash and again['spectral_sampling']==s
    strict=copy.deepcopy(q);strict['spectral']['sampling'].update(visible_error=1e-12,edge_error=1e-12,energy_error=1e-12)
    limited,_=run('limited',strict);assert limited['spectral_sampling']['status']=='pass_limit'
    assert limited['spectral_sampling']['passes']==16
    progressive=copy.deepcopy(full)
    progressive['spectral']['sampling'].update(camera_sequence='progressive',camera_seed=77)
    pr,ph=run('progressive',progressive)
    assert pr['spectral_sampling']['camera_seed']==77 and pr['spectral_sampling']['camera_sequence']=='progressive'
    assert run('progressive-repeat',progressive)[1]==ph
    explicit=copy.deepcopy(progressive)
    explicit['spectral']['sampling']['camera_samples_per_map']=1
    assert run('explicit-one',explicit)[1]==ph
    for bundle in (4,8):
        bundled=copy.deepcopy(progressive)
        bundled['spectral']['sampling']['camera_samples_per_map']=bundle
        report,bh=run('bundle-'+str(bundle),bundled)
        bs=report['spectral_sampling']
        assert bs['passes']==16 and bs['full_checkpoints']==2
        assert bs['min_pixel_maps']==bs['max_pixel_maps']==16
        assert bs['min_pixel_samples']==bs['max_pixel_samples']==16*bundle
        assert bs['camera_samples']==16*bundle*48*32
        assert bs['camera_samples_per_map']==bundle
        assert run('bundle-repeat-'+str(bundle),bundled)[1]==bh
        for key in ('launched','expanded_deposits','deposits','direct_arrivals','missed','depth_limited'):
            assert report['spectral_surface_map'][key]==pr['spectral_surface_map'][key]
    progressive['spectral']['sampling']['camera_seed']=78
    changed,ch=run('camera-seed',progressive)
    assert ch!=ph
    # Camera quadrature must not change photon transport accounting.
    for key in ('launched','expanded_deposits','deposits','direct_arrivals','missed','depth_limited'):
        assert pr['spectral_surface_map'][key]==changed['spectral_surface_map'][key]
    for bad in [dict(mode='converge',camera_seed=None),dict(mode='converge',camera_sequence=None),dict(mode='converge',camera_seed=-1),dict(mode='converge',camera_seed=4294967296),dict(mode='converge',camera_seed=True),dict(mode='converge',camera_sequence='progressive\0bad'),dict(mode='converge',camera_sequence='wrong'),dict(mode='converge',local_error=0),dict(mode='converge',camera_coverage='full\0extra'),dict(mode='converge',camera_coverage='unknown'),dict(mode='converge',camera_coverage=True),dict(mode='unknown'),dict(mode='converge\0extra'),dict(mode='converge',min_passes=8),dict(mode='converge',max_passes=17),dict(mode='converge',energy_error=0),dict(mode='converge',extra=True),dict(mode='single',max_passes=16),False,None]:
        malformed=copy.deepcopy(q);malformed['spectral']['sampling']=bad;run('invalid',malformed,False)
    moved=copy.deepcopy(q)
    moved['inspection']['camera_position']['y']+=.012
    moved_report,moved_hash=run('moved',moved)
    assert moved_hash!=first_hash and moved_report['spectral_sampling']['passes']==16
    assert run('returned',q)[1]==first_hash
    print('PASS native V3 single parity, balanced convergence, pass-limit status, replay, camera reset and malformed settings')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--cli',type=Path,required=True);p.add_argument('--output-root',type=Path,required=True);a=p.parse_args();qualify(a.cli.resolve(),a.output_root.resolve())

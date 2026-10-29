#!/usr/bin/env python3
"""Native/Python/shared 16/32 admission, 33 refusal and declared-budget agreement."""
import argparse,copy,hashlib,json,subprocess,sys
from pathlib import Path


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for n in ['native','fixture','ball-tools','profile','profile-sanitizer','out']:
        ap.add_argument('--'+n,type=Path,required=True)
    a=ap.parse_args();root=a.out.resolve();root.mkdir(parents=True,exist_ok=False)
    sys.path.insert(0,str(a.ball_tools.resolve()))
    from rigid_result_contract import validate,MAX_RIGID_BODIES,MAX_RIGID_CONSTRAINTS
    capabilities=json.loads(subprocess.check_output([str(a.native),'capabilities']))
    assert capabilities['body_capacity']==MAX_RIGID_BODIES==32
    assert capabilities['pair_capacity']==capabilities['constraint_capacity']==MAX_RIGID_CONSTRAINTS==496
    original=json.loads((a.fixture/'scene_authoring.json').read_text())
    def scene_with(count):
        scene=copy.deepcopy(original)
        while len(scene['objects'])<count:
            i=len(scene['objects']);o=copy.deepcopy(scene['objects'][0]);b=copy.deepcopy(scene['extensions']['rigid_scene']['bindings'][0])
            o['object_id']=b['object_id']='capacity_'+str(i);b['body_id']=500+i
            scene['objects'].append(o);scene['extensions']['rigid_scene']['bindings'].append(b)
        for i,o in enumerate(scene['objects']):o['transform']['position']={'x':i*20.,'y':0.,'z':4.}
        return scene
    for count in [16,32]:
        scene=scene_with(count);path=root/f'input{count}.json';path.write_text(json.dumps(scene))
        cache=json.loads(subprocess.check_output([str(a.native),'record',str(path),str(a.fixture/'asset_set.json'),'8']))
        validate(cache);(root/f'capacity{count}-cache.json').write_text(json.dumps(cache))
    over_scene=root/'input33.json';over_scene.write_text(json.dumps(scene_with(33)))
    p=subprocess.run([str(a.native),'record',str(over_scene),str(a.fixture/'asset_set.json'),'8'],capture_output=True,text=True)
    assert p.returncode==2 and json.loads(p.stdout)['error']=='body_capacity',(p.returncode,p.stdout,p.stderr)
    over=copy.deepcopy(cache);b=copy.deepcopy(over['physical_input']['bindings'][-1]);b['object_id']='capacity_over';b['body_id']=999
    over['physical_input']['bindings'].append(b);canonical=json.dumps(over['physical_input'],separators=(',',':'))
    over['physical_input_canonical']=canonical;over['physical_input_sha256']=hashlib.sha256(canonical.encode()).hexdigest()
    (root/'capacity33-cache.json').write_text(json.dumps(over))
    try:validate(over)
    except ValueError as e:assert str(e)=='body capacity'
    else:raise AssertionError('Python accepted33')
    for mode,exe in [('normal',a.profile),('san',a.profile_sanitizer)]:
        for count,expected in [(16,0),(32,0),(33,3)]:
            p=subprocess.run([str(exe),str(root/f'capacity{count}-cache.json')],capture_output=True)
            assert p.returncode==expected,(mode,count,p.returncode,p.stderr)
    (root/'verification.json').write_text(json.dumps({'ok':True,'capabilities':capabilities,
        'native_python_shared16_and32_admitted':True,'native_python_shared33_refused':True,
        'shared_normal_and_sanitizer':True},indent=2)+'\n')


if __name__=='__main__':main()

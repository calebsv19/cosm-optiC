"""Repeated matrices retain early MCP bindings beyond the old 32-scene ceiling."""
import copy,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from optical_comparison_mcp import MCP,run,resume,verify,digest,publish,wait_render

def qualify(client,scene,out):
    scene=Path(scene);out=Path(out);original=scene.read_bytes();attachment=client.state()['attachment']
    request=dict(schema='optic_optical_comparison_v1',comparison_id='capacity',object_id='surface',axes=[dict(parameter='ior',values=[1.1,1.8]),dict(parameter='roughness',values=[.02,.65])],settings=dict(transmission=.95,absorption_distance=2.,thin_walled=False,interface_tint=0xFFFFFF,absorption_color=0xBBDDEE),render=dict(width=96,height=64,temporal_frames=2,frame=0,integrator='disney_v2',sample_sequence=321))
    expected={};early=None;paths=[]
    try:
        for index in range(12):
            integrator='direct_light' if index%2 else 'disney_v2'
            recipe=dict(request,comparison_id='capacity_%02d'%index,render=dict(request['render'],integrator=integrator,temporal_frames=1 if integrator=='direct_light' else 2))
            root=out/recipe['comparison_id'];result=run(client,recipe,root,60);paths.append(root.name)
            hashes=[c['image_sha256'] for c in result['cells']]
            if integrator in expected:assert hashes==expected[integrator]
            else:expected[integrator]=hashes
            assert verify(client,root,60)['verified']
            if early is None:
                early_path=root/result['cells'][0]['scene_relative']
                client.mutate('scene_open',scene_path=str(early_path))
                early=MCP(early_path);early_pid=early.process.pid
                client.mutate('scene_open',scene_path=str(scene))
                early_descriptor=json.loads(Path(str(early_path)+'.optic-agent.json').read_text())
            assert early.process.pid==early_pid and early.process.poll() is None
            assert early.state()['scene_path']==str(scene)
            assert json.loads(Path(str(early_path)+'.optic-agent.json').read_text())==early_descriptor
        # Interrupt after one sealed cell, beyond the previous endpoint ceiling.
        root=out/'capacity-resume';actual=client.mutate
        def interrupted(tool,**args):
            if tool=='render_start_seeded' and args.get('request_id')=='capacity_resume_01':raise RuntimeError('injected after first cell')
            return actual(tool,**args)
        client.mutate=interrupted
        try:run(client,dict(request,comparison_id='capacity_resume'),root,60)
        except RuntimeError as exc:assert 'injected' in str(exc)
        else:raise AssertionError('interruption not reached')
        finally:client.mutate=actual
        prior=json.loads((root/'matrix.json').read_text());assert len(prior['cells'])==1
        first=copy.deepcopy(prior['cells'][0]);sealed=[root/first['scene_relative'],root/first['image_relative']];times=[p.stat().st_mtime_ns for p in sealed]
        recovered=resume(client,root,60);assert recovered['cells'][0]==first and [p.stat().st_mtime_ns for p in sealed]==times
        assert [c['image_sha256'] for c in recovered['cells']]==expected['disney_v2'];assert verify(client,root,60)['verified'];paths.append(root.name)
        state=client.state();assert not state['dirty'] and state['scene_path']==str(scene) and state['attachment']==attachment and scene.read_bytes()==original
        assert early.state()['scene_path']==str(scene) and early.process.poll() is None
        descriptors=list(out.rglob('*.optic-agent.json'));assert len(descriptors)>=65,len(descriptors)
        assert all(json.loads(p.read_text())['socket']==early_descriptor['socket'] for p in descriptors)
        result=dict(dynamic_scene_attachments=True,scene_endpoints=len(descriptors),repeated_matrices=12,completed_cells=52,direct_light_and_disney_repeats_exact=True,all_saved_cells_reopened_rendered_exact=True,early_mcp_process_and_binding_preserved=True,partial_resume_beyond_32_preserves_completed_cell=True,original_legacy_bytes_and_attachment_preserved=True,original_sha256=digest(scene),matrix_paths=paths)
        publish(out/'capacity-acceptance.json',result);return result
    finally:
        if early:early.close()

if __name__=='__main__':
    scene=Path(sys.argv[1]).resolve()
    with MCP(scene) as client:
        ui=json.loads((scene.parent/'matrix-ui-render.json').read_text())
        client.mutate('render_start_seeded',request_id='capacity-ui-parity',width=96,height=64,temporal_frames=2,frame=0,integrator='disney_v2',sample_sequence=321)
        assert wait_render(client,'capacity-ui-parity',60)['image_sha256']==ui['image_sha256']
        result=qualify(client,scene,scene.parent)
    result['shared_ui_mcp_seeded_pixels_exact']=True
    publish(scene.parent/'matrix-acceptance.json',result)
    print(json.dumps(result,indent=2))

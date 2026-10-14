"""Real native MCP matrix workflows and failure recovery."""
import copy,hashlib,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from optical_comparison_mcp import MCP,plan,run,verify,validate_request,wait_render,digest
scene=Path(sys.argv[1]).resolve();out=scene.parent;original=scene.read_bytes()
request=dict(schema='optic_optical_comparison_v1',comparison_id='ior_frost',object_id='surface',axes=[dict(parameter='ior',values=[1.1,1.8]),dict(parameter='roughness',values=[.02,.65])],settings=dict(transmission=.95,absorption_distance=2.,thin_walled=False,interface_tint=0xFFFFFF,absorption_color=0xBBDDEE),render=dict(width=96,height=64,temporal_frames=2,frame=0,integrator='disney_v2',sample_sequence=321))
with MCP(scene) as client:
    state=client.state();attachment=state['attachment'];assert attachment
    ui=json.loads((out/'matrix-ui-render.json').read_text());client.mutate('render_start_seeded',request_id='mcp-seeded',**request['render']);same=wait_render(client,'mcp-seeded',60);assert same['image_sha256']==ui['image_sha256']
    before=client.state();prepared=plan(client,request);assert prepared['cells']==4 and client.state()==before and scene.read_bytes()==original
    for change in [dict(axes=[request['axes'][0]]),dict(render=dict(request['render'],sample_sequence=0)),dict(axes=[dict(parameter='ior',values=[True]),request['axes'][1]])]:
        try:validate_request(dict(request,**change))
        except ValueError:pass
        else:raise AssertionError('invalid request accepted')
    a=run(client,request,out/'matrix-a',60);assert a['original_restored'] and client.state()['attachment']==attachment and scene.read_bytes()==original
    for cell in a['cells']:
        graph=cell['response']['retained']['surface_graph'];assert graph['outputs']['roughness']=='optic_comparison_roughness'
        assert next(n for n in graph['nodes'] if n['id']=='optic_comparison_roughness')['value']==cell['values']['roughness']
        assert graph['producer']=={'preserve':'optics'}
    repeat=run(client,dict(request,comparison_id='ior_frost_repeat'),out/'matrix-repeat',60)
    assert [c['image_sha256'] for c in a['cells']]==[c['image_sha256'] for c in repeat['cells']]
    assert a['plan']['recipe_sha256']==repeat['plan']['recipe_sha256']
    v=verify(client,out/'matrix-a',60);assert v['verified'] and v['original_restored']
    second=dict(request,comparison_id='trans_absorb',axes=[dict(parameter='transmission',values=[.2,.9]),dict(parameter='absorption_distance',values=[.25,4.])],settings=dict(ior=1.5,thin_walled=False,interface_tint=0xFFFFFF,absorption_color=0xBBDDEE))
    b=run(client,second,out/'matrix-transmission-absorption',60);verify(client,out/'matrix-transmission-absorption',60)
    for matrix in (a,b):
        hashes=[c['image_sha256'] for c in matrix['cells']]
        assert hashes[0]!=hashes[1] and hashes[0]!=hashes[2] and hashes[2]!=hashes[3] and hashes[1]!=hashes[3], 'each axis must affect both rows/columns'
    # Closed-schema seeded commands reject invalid seed types/ranges before rendering.
    for seed in (0,True,4294967296):
        try:client.mutate('render_start_seeded',request_id='bad-seed',**dict(request['render'],sample_sequence=seed))
        except RuntimeError:pass
        else:raise AssertionError('invalid native seed accepted')
    direct=dict(request,comparison_id='direct',axes=[dict(parameter='ior',values=[1.5]),dict(parameter='roughness',values=[.02,.65])],render=dict(request['render'],integrator='direct_light',temporal_frames=1))
    d=run(client,direct,out/'matrix-direct',60);verify(client,out/'matrix-direct',60)
    # An existing output is never overwritten/replayed.
    try:run(client,request,out/'matrix-a',60)
    except ValueError:pass
    else:raise AssertionError('existing output accepted')
    original_mutate=client.mutate
    def fail(tool,**args):
        if tool=='render_start_seeded' and args.get('request_id')=='failure_01':raise RuntimeError('injected interruption before second render')
        return original_mutate(tool,**args)
    client.mutate=fail
    try:run(client,dict(request,comparison_id='failure'),out/'matrix-interrupted',60)
    except RuntimeError:pass
    else:raise AssertionError('interruption accepted')
    finally:client.mutate=original_mutate
    failed=json.loads((out/'matrix-interrupted/matrix.json').read_text());assert failed['state']=='failed' and failed['original_restored'] and len(failed['cells'])==1
    assert client.state()['scene_path']==str(scene) and not client.state()['dirty'] and client.state()['attachment']==attachment and scene.read_bytes()==original
    # Contact sheet is self-contained; provenance binds every source/result.
    assert (out/'matrix-a/matrix.svg').read_text().count('data:image/png;base64,')==4
    receipt=dict(all_four_axes=True,each_axis_changes_each_row_column=True,direct_light_saved_reopened_exact=True,invalid_seed_refused=True,shared_native_ui_mcp_seeded_pixels=True,repeated_disney_cells_exact=True,saved_reopened_disney_cells_exact=True,graph_uniform_roughness=True,original_legacy_source_and_attachment_preserved=True,partial_failure_restored=True,existing_output_refused=True,contact_sheet_labeled_self_contained=True,original_sha256=digest(scene),matrix_paths=['matrix-a','matrix-repeat','matrix-transmission-absorption','matrix-direct'])
    (out/'matrix-acceptance.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('Optical matrices MCP PASS')

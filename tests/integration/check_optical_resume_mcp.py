"""Real native crash/reconnect recovery; never touches the installed editor."""
import copy,json,os,signal,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from optical_comparison_mcp import MCP,run,resume,verify,digest,wait_render,publish
import optical_comparison_recovery as recovery
scene=Path(sys.argv[1]).resolve();out=scene.parent;original=scene.read_bytes();phase=os.environ.get('OPTIC_RESUME_PHASE','kill')
request=dict(schema='optic_optical_comparison_v1',comparison_id='editor_death',object_id='surface',axes=[dict(parameter='ior',values=[1.1,1.8]),dict(parameter='roughness',values=[.02,.65])],settings=dict(transmission=.95,absorption_distance=2.,thin_walled=False,interface_tint=0xFFFFFF,absorption_color=0xBBDDEE),render=dict(width=96,height=64,temporal_frames=2,frame=0,integrator='disney_v2',sample_sequence=321))
if phase=='kill':
    with MCP(scene) as client:
        real_wait=recovery.wait_render
        def kill_editor(client,job_id,timeout):
            if job_id=='editor_death_01':
                status=client.call('render_status',{'job_id':job_id})['render'];assert status['scene_path'].startswith(str(out/'editor-death/project'));assert Path(os.environ['OPTIC_TEST_NATIVE_BINARY']).resolve().is_file()
                publish(out/'editor-kill.json',dict(pid=status['editor_pid'],first_cell=json.loads((out/'editor-death/matrix.json').read_text())['cells'][0],original_sha256=digest(scene)))
                os.kill(status['editor_pid'],signal.SIGKILL);raise RuntimeError('injected native editor death')
            return real_wait(client,job_id,timeout)
        recovery.wait_render=kill_editor
        try:run(client,request,out/'editor-death',60)
        except RuntimeError:pass
        else:raise AssertionError('native death injection failed')
    sys.exit(0)
def core_cases(client):
    attachment=client.state()['attachment'];assert attachment
    root=out/'editor-death';prior=json.loads((root/'matrix.json').read_text());first=copy.deepcopy(prior['cells'][0]);first_paths=[root/first['scene_relative'],root/first['image_relative']];times=[p.stat().st_mtime_ns for p in first_paths]
    result=resume(client,root,60);assert result['cells'][0]==first and [p.stat().st_mtime_ns for p in first_paths]==times and result['resume_count']==1 and result['original_restored'];assert result['retired_attempts'] and result['cells'][1]['render']['job_id']=='editor_death_01_r1';verify(client,root,60)
    # Same scene values/sampling reproduce the uninterrupted matrix exactly.
    # Completed resume is an idempotent, read-only operation.
    before=client.state();bytes_before=(root/'matrix.json').read_bytes();again=resume(client,root,60);assert again==result and client.state()==before and (root/'matrix.json').read_bytes()==bytes_before
    # Complete render but interrupt before its cell is committed to the journal.
    real_wait=recovery.wait_render
    def completed_then_die(client,job_id,timeout):
        value=real_wait(client,job_id,timeout)
        if job_id=='adopt_01':raise RuntimeError('completed native result not yet journaled')
        return value
    recovery.wait_render=completed_then_die
    try:run(client,dict(request,comparison_id='adopt'),out/'adopt',60)
    except RuntimeError:pass
    finally:recovery.wait_render=real_wait
    pending=json.loads((out/'adopt/matrix.json').read_text())['pending'];image=out/'adopt/project/.optic-renders/adopt_01/frame.bmp';mtime=image.stat().st_mtime_ns
    adopted=resume(client,out/'adopt',60);assert adopted['cells'][1]['render']['job_id']=='adopt_01' and image.stat().st_mtime_ns==mtime and not adopted.get('retired_attempts');verify(client,out/'adopt',60)
    # Tamper refusal happens before scene/history mutation.
    saved=(root/first['scene_relative']).read_bytes();(root/first['scene_relative']).write_bytes(saved+b' ');state=client.state()
    try:resume(client,root,60)
    except ValueError:pass
    else:raise AssertionError('changed completed cell accepted')
    assert client.state()==state;(root/first['scene_relative']).write_bytes(saved)
    manifest=json.loads((root/'matrix.json').read_text());manifest['request']['render']['sample_sequence']+=1;publish(root/'matrix.json',manifest)
    try:resume(client,root,60)
    except ValueError:pass
    else:raise AssertionError('changed recipe accepted')
    publish(root/'matrix.json',result)
    # Crash after cell Save As but before sealing it: keep the orphan and regenerate.
    actual_publish=recovery.publish
    def saved_orphan(path,value):
        p=value.get('pending')
        if value.get('request',{}).get('comparison_id')=='orphan' and p and p['phase']=='saved':raise RuntimeError('crash after Save As before journal seal')
        return actual_publish(path,value)
    recovery.publish=saved_orphan
    try:run(client,dict(request,comparison_id='orphan'),out/'orphan',60)
    except RuntimeError:pass
    finally:recovery.publish=actual_publish
    # The handled error's final publication records the saved state; simulate the
    # last durable pre-seal checkpoint left by a killed runner.
    partial=json.loads((out/'orphan/matrix.json').read_text());partial['pending']['phase']='authoring';partial['pending'].pop('scene_sha256',None);partial['pending'].pop('response',None);publish(out/'orphan/matrix.json',partial)
    orphan=out/'orphan/project/cell-00.json';orphan_hash=digest(orphan);orphan_mtime=orphan.stat().st_mtime_ns
    recovered=resume(client,out/'orphan',60);assert recovered['cells'][0]['scene_relative']=='project/cell-00-recovery-1.json' and digest(orphan)==orphan_hash and orphan.stat().st_mtime_ns==orphan_mtime
    # Kill only the runner process while its native render remains alive.
    import subprocess
    runner_request=dict(request,comparison_id='runner_death',render=dict(request['render'],temporal_frames=16));publish(out/'runner-request.json',runner_request)
    child=subprocess.Popen([sys.executable,'-B',str(ROOT/'tools/optical_comparison_mcp.py'),'--scene',str(scene),'--request',str(out/'runner-request.json'),'--output-root',str(out/'runner-death')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:
        deadline=time.monotonic()+90
        while time.monotonic()<deadline:
            journal=out/'runner-death/matrix.json';job=out/'runner-death/project/.optic-renders/runner_death_01/job.json'
            if journal.exists() and job.exists():
                data=json.loads(journal.read_text());status=json.loads(job.read_text())
                if len(data['cells'])==1 and status['state']=='rendering':break
            assert child.poll() is None;time.sleep(.01)
        else:raise AssertionError('runner interruption window not reached')
        frozen=copy.deepcopy(data['cells'][0]);os.kill(child.pid,signal.SIGKILL);child.wait(timeout=10)
        recovered_runner=resume(client,out/'runner-death',60);assert recovered_runner['cells'][0]==frozen and recovered_runner['cells'][1]['render']['job_id']=='runner_death_01';verify(client,out/'runner-death',60)
    finally:
        if child.poll() is None:child.terminate();child.wait(timeout=10)
    assert client.state()['scene_path']==str(scene) and not client.state()['dirty'] and scene.read_bytes()==original and client.state()['attachment']==attachment
    publish(out/'core-recovery.json',dict(editor_and_runner_recovery=True,original_preserved=True))

def auxiliary_cases(client):
    attachment=client.state()['attachment'];root=out/'editor-death';result=json.loads((root/'matrix.json').read_text());first=result['cells'][0]
    control=run(client,dict(request,comparison_id='uninterrupted'),out/'control',60);assert [c['image_sha256'] for c in result['cells']]==[c['image_sha256'] for c in control['cells']]
    # A crash publishing the sheet is recoverable without rerendering cells.
    actual_sheet=recovery.contact_sheet
    def sheet_then_fail(root,receipt,filename='matrix.svg'):
        value=actual_sheet(root,receipt,filename)
        if receipt['request']['comparison_id']=='sheet_crash':raise RuntimeError('sheet published before journal commit')
        return value
    recovery.contact_sheet=sheet_then_fail
    try:run(client,dict(request,comparison_id='sheet_crash'),out/'sheet-crash',60)
    except RuntimeError:pass
    finally:recovery.contact_sheet=actual_sheet
    partial=json.loads((out/'sheet-crash/matrix.json').read_text());frozen=copy.deepcopy(partial['cells']);sheet_hash=digest(out/'sheet-crash/matrix.svg');recovered_sheet=resume(client,out/'sheet-crash',60);assert recovered_sheet['cells']==frozen and recovered_sheet['contact_sheet']=='matrix-recovery-1.svg' and digest(out/'sheet-crash/matrix.svg')==sheet_hash
    # Lock, dependency, baseline and engine mismatch refuse before editing.
    for file in (root/result['baseline_relative'],root/next(iter(result['dependencies']))):
        saved=file.read_bytes();file.write_bytes(saved+b' ');state=client.state()
        try:resume(client,root,60)
        except ValueError:pass
        else:raise AssertionError('changed seal accepted')
        assert client.state()==state;file.write_bytes(saved)
    changed=copy.deepcopy(result);changed['renderer_sha256']='0'*64;publish(root/'matrix.json',changed);state=client.state()
    try:resume(client,root,60)
    except ValueError:pass
    else:raise AssertionError('changed engine accepted')
    assert client.state()==state;publish(root/'matrix.json',result)
    with recovery.locked(root):
        try:resume(client,root,60)
        except ValueError:pass
        else:raise AssertionError('concurrent runner lock accepted')
    # Direct Light recovery follows the same journal and save/reopen owner.
    direct=dict(request,comparison_id='direct_resume',render=dict(request['render'],integrator='direct_light',temporal_frames=1))
    real_mutate=client.mutate
    def cancel(client_tool,**args):
        value=real_mutate(client_tool,**args)
        if client_tool=='render_start_seeded' and args['request_id']=='direct_resume_01':
            real_mutate('render_cancel',job_id=args['request_id']);raise RuntimeError('injected cancellation')
        return value
    client.mutate=cancel
    try:run(client,direct,out/'direct',60)
    except RuntimeError:pass
    finally:client.mutate=real_mutate
    resumed_direct=resume(client,out/'direct',60);assert resumed_direct['original_restored'];verify(client,out/'direct',60)
    assert client.state()['attachment']==attachment and client.state()['scene_path']==str(scene) and not client.state()['dirty'] and scene.read_bytes()==original
    publish(out/'resume-acceptance.json',dict(native_editor_death_recovered=True,runner_death_active_render_adopted=True,sheet_publication_recovered=True,dependency_baseline_build_lock_refusals=True,completed_cells_not_rewritten=True,terminal_attempt_new_id=True,completed_unjournaled_render_adopted=True,uninterrupted_pixels_exact=True,completed_resume_idempotent=True,saved_orphan_preserved=True,direct_and_disney_save_reopen_exact=True,tampered_source_recipe_refused=True,original_legacy_bytes_and_attachment_preserved=True))

with MCP(scene) as client:
    if phase=='recover':core_cases(client)
    elif phase=='aux':auxiliary_cases(client)
    else:raise ValueError('unknown native recovery phase')
print('Optical partial resume MCP PASS')

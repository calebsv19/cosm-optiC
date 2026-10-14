"""Explicit build transition after exact replay of every sealed completed cell."""
import copy,json,time
from pathlib import Path
from optical_comparison_mcp import canonical,digest,publish,wait_render
from optical_comparison_recovery import locked,check_seals,contained,engine

def qualify(client,root,timeout=300):
    root=Path(root).resolve(strict=True)
    with locked(root):
        journal=root/'matrix.json';before=journal.read_bytes();receipt=json.loads(before)
        check_seals(root,receipt)
        old=receipt['renderer_sha256'];new=engine(client)
        if old==new:raise ValueError('renderer is unchanged; ordinary resume applies')
        if receipt['state']!='failed' or not receipt['original_restored'] or not receipt['cells']:
            raise ValueError('qualification requires a restored paused matrix with completed cells')
        original=client.state();render=client.call('render_read')['render']
        if original['dirty'] or render['active'] or original['scene_path']!=receipt['plan']['original_scene']:
            raise ValueError('clean idle original comparison scene required')
        prefix='transition_'+str(time.time_ns());results=[];active=None
        try:
            for i,cell in enumerate(receipt['cells']):
                path=contained(root,cell['scene_relative']);client.mutate('scene_open',scene_path=str(path))
                response=client.call('material_response_read',{'object_id':receipt['request']['object_id']})['response']
                if response!=cell['response']:raise ValueError('reopened completed response changed')
                active=f'{prefix}_{i:02d}'
                client.mutate('render_start_seeded',request_id=active,**receipt['request']['render'])
                result=wait_render(client,active,timeout)
                image=Path(result['image_path']).resolve(strict=True)
                snapshot=image.parent/'source_snapshot.json'
                if (not image.is_relative_to(root/'project/.optic-renders') or
                    image.parent.name!=active or image.name!='frame.bmp' or result['job_id']!=active or
                    not result['artifact_integrity'] or digest(image)!=cell['image_sha256'] or
                    result['settings']!=receipt['request']['render'] or
                    result['scene_path']!=str(path) or result['sample_sequence']!=receipt['request']['render']['sample_sequence'] or
                    canonical(json.loads(snapshot.read_text()))!=canonical(json.loads(path.read_text())) or
                    digest(snapshot)!=result['source_sha256'] or result['image_sha256']!=cell['image_sha256']):
                    raise ValueError('completed-cell source/settings/image parity failed; old journal retained')
                results.append(dict(cell=i,scene_sha256=cell['scene_sha256'],
                    image_sha256=cell['image_sha256'],qualified_render=result))
                active=None
        finally:
            render=client.call('render_read')['render']
            if active and render['active']:
                state=client.state()
                if render['latest_job_id']!=active or Path(state['scene_path']).parent!=root/'project':
                    raise RuntimeError('unrelated active render; transition restore refused')
                client.mutate('render_cancel',job_id=active);deadline=time.monotonic()+120
                while client.call('render_read')['render']['active'] and time.monotonic()<deadline:time.sleep(.1)
                if client.call('render_read')['render']['active']:raise RuntimeError('transition cancellation still pending')
            state=client.state()
            if state['dirty']:raise RuntimeError('transition scene became dirty; restore refused')
            if state['scene_path']!=original['scene_path']:
                if Path(state['scene_path']).parent!=root/'project':raise RuntimeError('editor left transition-owned scenes')
                client.mutate('scene_open',scene_path=original['scene_path'])
        check_seals(root,receipt)
        if journal.read_bytes()!=before or engine(client)!=new:raise ValueError('journal or renderer changed during qualification')
        proof=dict(schema='optic_verified_renderer_transition_v1',old_renderer_sha256=old,
                   new_renderer_sha256=new,request_sha256=receipt['request_sha256'],
                   completed_cells=results,original_restored=True)
        proof_name=prefix+'.json';publish(root/proof_name,proof)
        backup_name=prefix+'-previous-matrix.json';publish(root/backup_name,receipt)
        receipt.setdefault('renderer_transitions',[]).append(dict(
            proof_relative=proof_name,proof_sha256=digest(root/proof_name),
            previous_journal_relative=backup_name,previous_journal_sha256=digest(root/backup_name),
            old_renderer_sha256=old,new_renderer_sha256=new))
        receipt['renderer_sha256']=new;publish(journal,receipt)
        return proof

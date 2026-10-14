"""Sealed matrix cell journal and process-interruption recovery via shared MCP."""
import contextlib,copy,fcntl,hashlib,json,os,subprocess,time
from pathlib import Path
from optical_comparison_mcp import canonical,digest,publish,validate_request,plan,rough_graph,wait_render
from optical_comparison_artifacts import contact_sheet
SCHEMA='optic_optical_comparison_result_v2'

def grid(request):
    axes=request['axes']
    return [(r,c,{axes[0]['parameter']:x,axes[1]['parameter']:y}) for r,y in enumerate(axes[1]['values']) for c,x in enumerate(axes[0]['values'])]

def contained(root,relative,exists=True):
    p=Path(relative)
    if p.is_absolute() or '..' in p.parts:raise ValueError('matrix path escape')
    path=(root/p).resolve(strict=exists)
    if not path.is_relative_to(root):raise ValueError('matrix dependency escapes root')
    return path

@contextlib.contextmanager
def locked(root):
    fd=os.open(root/'.matrix.lock',os.O_RDWR|os.O_CREAT|os.O_NOFOLLOW,0o600)
    try:
        try:fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:raise ValueError('another matrix runner owns this output root')
        yield
    finally:os.close(fd)

def engine(client):
    state=client.state();descriptor=Path(state['scene_path']+'.optic-agent.json');pid=json.loads(descriptor.read_text())['pid']
    binary=Path(subprocess.check_output(['ps','-ww','-p',str(pid),'-o','comm='],text=True).strip()).resolve(strict=True)
    if not binary.is_file():raise ValueError('native renderer executable unavailable')
    return digest(binary)

def seal_dependencies(root,baseline):
    project=root/'project';result={}
    for p in project.rglob('*'):
        if not p.is_file() or p==baseline or p.name.endswith('.optic-agent.json') or p.name=='.'+baseline.name+'.managed.lock' or '.optic-renders' in p.parts:continue
        if p.is_symlink():raise ValueError('comparison dependencies must be copied regular files')
        result[str(p.relative_to(root))]=digest(p)
    return result

def check_seals(root,receipt):
    if receipt.get('schema')!=SCHEMA:raise ValueError('resume requires a v2 sealed journal; older results remain verifiable')
    request=validate_request(receipt['request'])
    if hashlib.sha256(canonical(request).encode()).hexdigest()!=receipt['request_sha256']:raise ValueError('comparison recipe changed')
    if digest(Path(receipt['plan']['original_scene']))!=receipt['plan']['original_sha256']:raise ValueError('original source changed')
    if not receipt.get('baseline_sha256'):raise ValueError('baseline was not sealed; preserve this attempt and start a fresh matrix')
    if digest(contained(root,receipt['baseline_relative']))!=receipt['baseline_sha256']:raise ValueError('baseline changed')
    for relative,sha in receipt['dependencies'].items():
        if digest(contained(root,relative))!=sha:raise ValueError('comparison dependency changed: '+relative)
    expected=grid(request);cells=receipt['cells']
    if len(cells)>len(expected) or [(c['row'],c['column'],c['values']) for c in cells]!=expected[:len(cells)]:raise ValueError('completed cells are not a valid grid prefix')
    for cell in cells:
        if digest(contained(root,cell['scene_relative']))!=cell['scene_sha256'] or digest(contained(root,cell['image_relative']))!=cell['image_sha256']:raise ValueError('completed cell changed')
    if receipt.get('contact_sheet_sha256') and digest(contained(root,receipt['contact_sheet']))!=receipt['contact_sheet_sha256']:raise ValueError('contact sheet changed')
    transitions=receipt.get('renderer_transitions',[])
    for i,transition in enumerate(transitions):
        proof_path=contained(root,transition['proof_relative']);previous=contained(root,transition['previous_journal_relative'])
        if digest(proof_path)!=transition['proof_sha256'] or digest(previous)!=transition['previous_journal_sha256']:
            raise ValueError('renderer transition proof changed')
        proof=json.loads(proof_path.read_text());prior=json.loads(previous.read_text())
        if (proof.get('schema')!='optic_verified_renderer_transition_v1' or not proof.get('original_restored') or
            proof.get('old_renderer_sha256')!=transition['old_renderer_sha256'] or
            proof.get('new_renderer_sha256')!=transition['new_renderer_sha256'] or
            proof.get('request_sha256')!=receipt['request_sha256'] or
            prior.get('renderer_sha256')!=transition['old_renderer_sha256'] or
            prior.get('request_sha256')!=receipt['request_sha256']):
            raise ValueError('renderer transition identity differs')
        qualified=proof['completed_cells']
        if not qualified or len(qualified)!=len(prior['cells']) or len(qualified)>len(cells):
            raise ValueError('renderer transition lacks complete prior-cell proof')
        for index,row in enumerate(qualified):
            result=row['qualified_render'];cell=cells[index];path=contained(root,cell['scene_relative'])
            image=Path(result['image_path']).resolve(strict=True);snapshot=image.parent/'source_snapshot.json'
            if (row['cell']!=index or row['scene_sha256']!=cell['scene_sha256'] or
                row['image_sha256']!=cell['image_sha256'] or not image.is_relative_to(root/'project/.optic-renders') or
                result['state']!='completed' or not result['artifact_integrity'] or result['settings']!=request['render'] or
                result['scene_path']!=str(path) or result['sample_sequence']!=request['render']['sample_sequence'] or
                digest(image)!=cell['image_sha256'] or digest(snapshot)!=result['source_sha256'] or
                canonical(json.loads(snapshot.read_text()))!=canonical(json.loads(path.read_text()))):
                raise ValueError('renderer transition cell parity changed')
        if i and transitions[i-1]['new_renderer_sha256']!=transition['old_renderer_sha256']:
            raise ValueError('renderer transition chain differs')
    if transitions and transitions[-1]['new_renderer_sha256']!=receipt['renderer_sha256']:
        raise ValueError('renderer transition does not qualify current renderer')
    pending=receipt.get('pending')
    if pending:
        if len(cells)>=len(expected) or (pending['row'],pending['column'],pending['values'])!=expected[len(cells)]:raise ValueError('pending cell does not follow the completed prefix')
        path=contained(root,pending['scene_relative'],exists=False)
        if path.parent!=root/'project':raise ValueError('pending scene is outside the comparison project')
        if pending['phase'] not in ('authoring','saved','render_requested'):raise ValueError('unsupported pending phase')
        if pending['phase']!='authoring' and digest(path)!=pending['scene_sha256']:raise ValueError('pending saved scene changed')
    return request

def preflight(client,root,receipt):
    check_seals(root,receipt)
    if engine(client)!=receipt['renderer_sha256']:raise ValueError('native renderer build changed; exact resume refused')
    state=client.state();render=client.call('render_read')['render'];pending=receipt.get('pending')
    if not render.get('fixed_sampling_supported'):raise ValueError('fixed native sampling required')
    own=Path(state['scene_path']).parent==root/'project'
    if state['scene_path']!=receipt['plan']['original_scene'] and not own:raise ValueError('working scene does not belong to this comparison; resume refused')
    if state['dirty'] and not own:raise ValueError('dirty scene outside the comparison; resume refused')
    if render['active'] and (not pending or pending.get('job_id')!=render['latest_job_id'] or state['scene_path']!=str(contained(root,pending['scene_relative']))):raise ValueError('unrelated native render is active; resume refused')
    return state,render

def restore(client,root,receipt):
    pending=receipt.get('pending');render=client.call('render_read')['render']
    if render['active']:
        if not pending or render['latest_job_id']!=pending.get('job_id') or client.state()['scene_path']!=str(contained(root,pending['scene_relative'])):raise RuntimeError('unrelated active render; restore refused')
        client.mutate('render_cancel',job_id=pending['job_id']);deadline=time.monotonic()+30
        while client.call('render_read')['render']['active'] and time.monotonic()<deadline:time.sleep(.05)
        if client.call('render_read')['render']['active']:raise RuntimeError('owned render cancellation still pending')
    state=client.state();original=receipt['plan']['original_scene']
    if state['scene_path']!=original:
        if Path(state['scene_path']).parent!=root/'project':raise RuntimeError('editor left comparison-owned scene; restore refused')
        if state['dirty']:client.mutate('scene_save_as',scene_path=str(root/'project'/f'interrupted-{time.time_ns()}.json'))
        client.mutate('scene_open',scene_path=original)
    receipt['original_restored']=digest(original)==receipt['plan']['original_sha256'] and not client.state()['dirty']
    if not receipt['original_restored']:raise RuntimeError('original restoration failed')

def result_cell(client,root,receipt,pending):
    result=client.call('render_results',{'job_id':pending['job_id']})['render'];scene=contained(root,pending['scene_relative']);job=root/'project/.optic-renders'/pending['job_id']
    if not result['artifact_integrity'] or result['state']!='completed' or result['settings']!=receipt['request']['render'] or result['sample_sequence']!=receipt['request']['render']['sample_sequence'] or result['job_id']!=pending['job_id'] or result['scene_path']!=str(scene):raise RuntimeError('recovered render identity/settings differ')
    image=contained(root,str((job/'frame.bmp').relative_to(root)))
    snapshot=contained(root,str((job/'source_snapshot.json').relative_to(root)))
    if result['image_path']!=str(image) or digest(image)!=result['image_sha256'] or digest(snapshot)!=result['source_sha256'] or canonical(json.loads(snapshot.read_text()))!=canonical(json.loads(scene.read_text())):raise RuntimeError('render source/image does not match the sealed saved cell')
    if digest(scene)!=pending['scene_sha256']:raise RuntimeError('saved cell changed during render')
    return {key:copy.deepcopy(pending[key]) for key in ('row','column','values','scene_relative','scene_sha256','response')}|dict(image_relative=str(image.relative_to(root)),image_sha256=result['image_sha256'],render=result)

def fresh_cell(client,root,receipt,index,save):
    r,c,values=grid(receipt['request'])[index];candidate=root/'project'/f'cell-{index:02d}.json';recovery=0
    while candidate.exists() or candidate.is_symlink():
        recovery+=1;candidate=root/'project'/f'cell-{index:02d}-recovery-{recovery}.json'
    pending=dict(row=r,column=c,values=values,scene_relative=str(candidate.relative_to(root)),phase='authoring',attempt=0)
    receipt['pending']=pending;save()
    state=client.state()
    if state['dirty']:
        if Path(state['scene_path']).parent!=root/'project':raise ValueError('dirty scene outside comparison')
        client.mutate('scene_save_as',scene_path=str(root/'project'/f'interrupted-{time.time_ns()}.json'))
    client.mutate('scene_open',scene_path=str(contained(root,receipt['baseline_relative'])))
    client.mutate('material_response_set',object_id=receipt['request']['object_id'],settings={('base_roughness' if key=='roughness' else key):value for key,value in values.items()})
    if 'roughness' in values:
        row=client.call('material_read',{'object_id':receipt['request']['object_id']})['material']['retained']
        if row.get('surface_graph'):client.mutate('material_set_graph',object_id=receipt['request']['object_id'],graph=rough_graph(row['surface_graph'],values['roughness']))
    client.mutate('scene_save_as',scene_path=str(candidate));pending.update(phase='saved',scene_sha256=digest(candidate),response=client.call('material_response_read',{'object_id':receipt['request']['object_id']})['response']);save();return pending

def finish_cell(client,root,receipt,pending,save,timeout):
    scene=contained(root,pending['scene_relative']);state=client.state();render=client.call('render_read')['render']
    if not render['active']:
        if state['dirty']:
            if Path(state['scene_path']).parent!=root/'project':raise ValueError('dirty scene outside comparison')
            client.mutate('scene_save_as',scene_path=str(root/'project'/f'interrupted-{time.time_ns()}.json'))
        client.mutate('scene_open',scene_path=str(scene))
    if client.call('material_response_read',{'object_id':receipt['request']['object_id']})['response']!=pending['response']:raise RuntimeError('saved pending response differs')
    job_id=pending.get('job_id');job=root/'project/.optic-renders'/job_id if job_id else None
    if job and (job.exists() or job.is_symlink()):
        contained(root,str(job.relative_to(root)))
        if (job/'job.json').exists():
            status=client.call('render_status',{'job_id':job_id})['render']['state']
            if status in ('preparing','rendering','canceling'):wait_render(client,job_id,timeout);status='completed'
            if status=='completed':return result_cell(client,root,receipt,pending)
            if status not in ('canceled','failed','interrupted'):raise RuntimeError('unknown existing render state')
        receipt.setdefault('retired_attempts',[]).append(dict(cell=len(receipt['cells']),job_id=job_id,reason='terminal or incomplete native attempt; preserved'))
        pending['attempt']+=1
    # Never reuse a reserved render directory, including uncertain/incomplete starts.
    index=len(receipt['cells'])
    while True:
        suffix='' if pending['attempt']==0 else '_r'+str(pending['attempt'])
        job_id=f"{receipt['request']['comparison_id']}_{index:02d}{suffix}"
        if not (root/'project/.optic-renders'/job_id).exists():break
        pending['attempt']+=1
    pending.update(job_id=job_id,phase='render_requested');save()
    client.mutate('render_start_seeded',request_id=job_id,**receipt['request']['render']);wait_render(client,job_id,timeout)
    return result_cell(client,root,receipt,pending)

def execute(client,root,receipt,timeout):
    def save():publish(root/'matrix.json',receipt)
    receipt.update(state='rendering',original_restored=False);receipt.pop('error',None);receipt.pop('restore_error',None);save()
    try:
        while len(receipt['cells'])<len(grid(receipt['request'])):
            pending=receipt.get('pending')
            if not pending or pending['phase']=='authoring':pending=fresh_cell(client,root,receipt,len(receipt['cells']),save)
            cell=finish_cell(client,root,receipt,pending,save,timeout);receipt['cells'].append(cell);receipt['pending']=None;save();check_seals(root,receipt)
        filename=receipt.get('contact_sheet','matrix.svg');sheet=root/filename
        if not receipt.get('contact_sheet_sha256'):
            recovery=0
            while sheet.exists() or sheet.is_symlink():
                recovery+=1;filename=f'matrix-recovery-{recovery}.svg';sheet=root/filename
            contact_sheet(root,receipt,filename)
        receipt.update(contact_sheet=filename,contact_sheet_sha256=digest(sheet),state='completed')
    except BaseException as error:
        receipt.update(state='failed',error=str(error));raise
    finally:
        try:restore(client,root,receipt)
        except BaseException as error:receipt.update(state='failed',restore_error=str(error))
        save()
    if receipt['state']!='completed':raise RuntimeError(receipt.get('restore_error','matrix failed'))
    return receipt

def start(client,request,root,timeout):
    request=validate_request(request);prepared=plan(client,request);renderer=engine(client);root=Path(root).absolute()
    if root.exists() or root.is_symlink():raise ValueError('comparison output root already exists; use --resume or preserve it')
    root=root.parent.resolve(strict=True)/root.name;root.mkdir();project=root/'project';project.mkdir()
    with locked(root):
        client.transcript=root/'mcp-transcript.jsonl';receipt=dict(schema=SCHEMA,state='preparing',request=request,request_sha256=hashlib.sha256(canonical(request).encode()).hexdigest(),renderer_sha256=renderer,plan=prepared,cells=[],pending=None,resume_count=0,original_restored=False);publish(root/'matrix.json',receipt)
        try:
            baseline=project/'baseline.json';client.mutate('scene_save_as',scene_path=str(baseline));client.mutate('material_response_set',object_id=request['object_id'],settings=dict(request['settings'],base_preset='glass'));client.mutate('scene_save')
            receipt.update(baseline_sha256=digest(baseline),baseline_relative='project/baseline.json',dependencies=seal_dependencies(root,baseline));publish(root/'matrix.json',receipt)
        except BaseException as error:
            receipt.update(state='failed',error=str(error))
            try:restore(client,root,receipt)
            except BaseException as restore_error:receipt['restore_error']=str(restore_error)
            publish(root/'matrix.json',receipt);raise
        return execute(client,root,receipt,timeout)

def resume(client,root,timeout=300):
    root=Path(root).resolve(strict=True)
    with locked(root):
        receipt=json.loads((root/'matrix.json').read_text());preflight(client,root,receipt)
        if receipt['state']=='completed':
            if digest(contained(root,receipt['contact_sheet']))!=receipt['contact_sheet_sha256']:raise ValueError('completed contact sheet changed')
            return receipt
        if receipt['state'] not in ('rendering','failed'):raise ValueError('matrix is not resumable')
        client.transcript=root/'mcp-transcript.jsonl';receipt['resume_count']+=1
        return execute(client,root,receipt,timeout)

#!/usr/bin/env python3
"""Repeatable optical matrices using only live MCP authoring/render commands."""
from __future__ import annotations
import argparse,copy,hashlib,json,math,os,re,select,subprocess,sys,time
from pathlib import Path
from optical_comparison_artifacts import contact_sheet
PARAMETERS={'ior':(1,2.5),'transmission':(0,1),'absorption_distance':(.25,8),'roughness':(0,1)}
SETTINGS={'base_preset','ior','transmission','absorption_distance','thin_walled','base_roughness','interface_tint','absorption_color'}

def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def canonical(value):return json.dumps(value,sort_keys=True,separators=(',',':'),allow_nan=False)
def publish(path,value):
    path=Path(path);temporary=path.with_suffix('.pending');data=json.dumps(value,indent=2,allow_nan=False)+'\n'
    with temporary.open('w') as f:f.write(data);f.flush();os.fsync(f.fileno())
    os.replace(temporary,path)

def validate_request(request):
    if not isinstance(request,dict) or set(request)!={'schema','comparison_id','object_id','axes','settings','render'} or request['schema']!='optic_optical_comparison_v1':raise ValueError('unsupported comparison request')
    if not isinstance(request['comparison_id'],str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,40}',request['comparison_id']):raise ValueError('comparison_id must use 1..40 letters/digits/underscore/dash')
    if not isinstance(request['object_id'],str) or not request['object_id']:raise ValueError('object_id required')
    axes=request['axes']
    if not isinstance(axes,list) or len(axes)!=2:raise ValueError('two axes required; horizontal first, vertical second')
    for axis in axes:
        if not isinstance(axis,dict) or set(axis)!={'parameter','values'} or axis['parameter'] not in PARAMETERS:raise ValueError('unknown optical axis')
        values=axis['values'];low,high=PARAMETERS[axis['parameter']]
        if not isinstance(values,list) or not 1<=len(values)<=8:raise ValueError('axis needs 1..8 values')
        if any(type(v) not in (int,float) or not math.isfinite(v) or not low<=v<=high for v in values):raise ValueError('axis value outside native range')
        if len(set(values))!=len(values):raise ValueError('duplicate axis values')
    if axes[0]['parameter']==axes[1]['parameter'] or len(axes[0]['values'])*len(axes[1]['values'])>16:raise ValueError('distinct axes and at most 16 cells required')
    settings=request['settings']
    if not isinstance(settings,dict) or set(settings)-SETTINGS:raise ValueError('unknown held optical setting')
    for key,value in settings.items():
        if key=='base_preset':
            if value!='glass':raise ValueError('only glass comparison preset supported')
        elif key=='thin_walled':
            if type(value) is not bool:raise ValueError('thin_walled must be boolean')
        elif key in ('interface_tint','absorption_color'):
            if value is not None and (type(value) is not int or not 0<=value<=0xFFFFFF):raise ValueError('color must be RGB24 integer or null')
        else:
            low,high=PARAMETERS['roughness' if key=='base_roughness' else key]
            if type(value) not in (int,float) or not math.isfinite(value) or not low<=value<=high:raise ValueError('held setting outside native range')
    render=request['render']
    if not isinstance(render,dict) or set(render)!={'width','height','temporal_frames','frame','integrator','sample_sequence'}:raise ValueError('complete render settings required')
    for key,low,high in [('width',16,512),('height',16,512),('temporal_frames',1,32),('frame',0,1000000000),('sample_sequence',1,4294967295)]:
        if type(render[key]) is not int or not low<=render[key]<=high:raise ValueError('invalid '+key)
    if render['integrator'] not in ('direct_light','disney_v2') or render['integrator']=='direct_light' and render['temporal_frames']!=1:raise ValueError('invalid comparison integrator/passes')
    return copy.deepcopy(request)

class MCP:
    def __init__(self,scene,bridge=None,transcript=None):
        self.scene=Path(scene).resolve(strict=True);self.seq=0;self.buffer=b'';self.transcript=transcript
        bridge=Path(bridge) if bridge else Path(__file__).with_name('scene_editor_mcp.py')
        self.process=subprocess.Popen([sys.executable,'-B',str(bridge),'--scene',str(self.scene)],stdin=subprocess.PIPE,stdout=subprocess.PIPE)
        try:
            self.rpc('initialize',dict(protocolVersion='2025-06-18',capabilities={},clientInfo=dict(name='optic-optical-comparison',version='1')))
            self.process.stdin.write(b'{"jsonrpc":"2.0","method":"notifications/initialized"}\n');self.process.stdin.flush()
            self.catalog=self.raw('editor_catalog',{});self.call('scene_connect',{'scene_path':str(self.scene)})
        except BaseException:
            self.close()
            raise
    def rpc(self,method,params):
        self.seq+=1;self.process.stdin.write(json.dumps(dict(jsonrpc='2.0',id=self.seq,method=method,params=params),allow_nan=False).encode()+b'\n');self.process.stdin.flush();deadline=time.monotonic()+40
        while time.monotonic()<deadline:
            if b'\n' not in self.buffer:
                if not select.select([self.process.stdout],[],[],max(0,deadline-time.monotonic()))[0]:break
                data=os.read(self.process.stdout.fileno(),8192)
                if not data:raise RuntimeError('MCP bridge closed')
                self.buffer+=data
                if len(self.buffer)>262144:raise RuntimeError('MCP response exceeds capacity')
                continue
            line,self.buffer=self.buffer.split(b'\n',1);value=json.loads(line)
            if value.get('id')==self.seq:
                if 'error' in value:raise RuntimeError(str(value['error']))
                return value['result']
        raise TimeoutError('MCP reply')
    def raw(self,tool,args):
        result=self.rpc('tools/call',dict(name=tool,arguments=args))
        if result['isError']:raise RuntimeError(result['content'][0]['text'])
        value=json.loads(result['content'][0]['text'])
        if not value.get('ok'):raise RuntimeError(str(value))
        return value
    def call(self,tool,args=None):
        args=args or {};value=self.raw('editor_command',dict(catalog_id=self.catalog['catalog_id'],tool=tool,arguments=args))
        if value.get('scene_path'):self.scene=Path(value['scene_path'])
        if self.transcript:
            with Path(self.transcript).open('a') as f:f.write(canonical(dict(tool=tool,arguments=args,result=value))+'\n')
        return value
    def state(self):return self.call('scene_read')
    def mutate(self,tool,**args):return self.call(tool,dict(expected_revision=self.state()['revision'],**args))
    def close(self):
        if self.process.poll() is None:
            self.process.stdin.close()
            try:self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:self.process.terminate();self.process.wait(timeout=5)
    def __enter__(self):return self
    def __exit__(self,*exc):self.close()

def rough_graph(graph,value):
    graph=copy.deepcopy(graph);node_id='optic_comparison_roughness'
    if any(n.get('id')==node_id for n in graph['nodes']):raise ValueError('reserved comparison roughness node already exists')
    if len(graph['nodes'])>=32:raise ValueError('graph has no capacity for uniform comparison roughness')
    graph['nodes'].append(dict(id=node_id,kind='scalar',value=value));graph['outputs']['roughness']=node_id
    for region in graph.get('regions',[]):
        if 'roughness' in region.get('outputs',{}):region['outputs']['roughness']=node_id
    return graph

def plan(client,request):
    state=client.state()
    if state['dirty']:raise ValueError('save the working document before a comparison')
    render=client.call('render_read')['render']
    if render['active'] or not render.get('fixed_sampling_supported'):raise ValueError('idle source-matched editor with fixed sampling required')
    objects=client.call('object_list')['objects'];obj=next((o for o in objects if o['object_id']==request['object_id']),None)
    if not obj or obj['locked'] or not obj['visible'] or not obj['has_runtime_instance']:raise ValueError('visible unlocked native object required')
    response=client.call('material_response_read',{'object_id':request['object_id']})['response'];material=client.call('material_read',{'object_id':request['object_id']})['material'];row=material['retained'] or {};graph=row.get('surface_graph')
    rough=any(a['parameter']=='roughness' for a in request['axes'])
    if rough:
        if graph:rough_graph(graph,0)
        elif material['mapping'] is not None or any(key in row for key in ('material_graph','materialGraph','material_texture_stack','materialTextureStack','surface_material_binding','authored_texture','procedural_texture')):raise ValueError('roughness sweep requires a simple base or typed surface graph; layered/mapped legacy response is not uniform')
    signature=copy.deepcopy(request);signature.pop('comparison_id')
    return dict(original_scene=str(client.scene),original_sha256=digest(client.scene),original_state=state,target=response,roughness_route='uniform graph output and declared roughness regions' if rough and graph else 'base roughness',recipe_sha256=hashlib.sha256(canonical(signature).encode()).hexdigest(),cells=len(request['axes'][0]['values'])*len(request['axes'][1]['values']))

def wait_render(client,id,timeout):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        status=client.call('render_status',{'job_id':id})['render']
        if status['state'] not in ('preparing','rendering','canceling'):
            if status['state']!='completed':raise RuntimeError('render '+id+' '+status['state'])
            result=client.call('render_results',{'job_id':id})['render']
            if not result['artifact_integrity'] or digest(result['image_path'])!=result['image_sha256']:raise RuntimeError('render artifact verification failed')
            return result
        time.sleep(.05)
    raise TimeoutError('render '+id)

def run(client,request,root,timeout=300):
    from optical_comparison_recovery import start
    return start(client,request,root,timeout)

def resume(client,root,timeout=300):
    from optical_comparison_recovery import resume as recover
    return recover(client,root,timeout)

def verify(client,root,timeout=300):
    root=Path(root).resolve(strict=True);receipt=json.loads((root/'matrix.json').read_text());validate_request(receipt['request']);original=client.state();original_hash=digest(original['scene_path'])
    if original['dirty'] or receipt['state']!='completed':raise ValueError('saved working scene and completed matrix required')
    axes=receipt['request']['axes'];expected=[(r,c,{axes[0]['parameter']:x,axes[1]['parameter']:y}) for r,y in enumerate(axes[1]['values']) for c,x in enumerate(axes[0]['values'])]
    if [(cell['row'],cell['column'],cell['values']) for cell in receipt['cells']]!=expected:raise ValueError('matrix cells do not match the complete requested grid')
    def contained(relative):
        path=Path(relative)
        if path.is_absolute() or '..' in path.parts:raise ValueError('matrix path escape')
        resolved=(root/path).resolve(strict=True)
        if not resolved.is_relative_to(root):raise ValueError('matrix dependency escapes root')
        return resolved
    if digest(contained(receipt['baseline_relative']))!=receipt['baseline_sha256'] or digest(contained(receipt['contact_sheet']))!=receipt['contact_sheet_sha256']:raise ValueError('matrix baseline/contact sheet changed')
    # Exact repeat uses a new render namespace; never overwrites original jobs.
    prefix='verify_'+str(time.time_ns());results=[];active=None
    try:
        for i,cell in enumerate(receipt['cells']):
            path=contained(cell['scene_relative'])
            if digest(path)!=cell['scene_sha256'] or digest(contained(cell['image_relative']))!=cell['image_sha256']:raise ValueError('cell source/image changed')
            client.mutate('scene_open',scene_path=str(path));read=client.call('material_response_read',{'object_id':receipt['request']['object_id']})['response']
            if read!=cell['response']:raise RuntimeError('reopened response differs')
            id=f'{prefix}_{i:02d}';active=id;client.mutate('render_start_seeded',request_id=id,**receipt['request']['render']);result=wait_render(client,id,timeout)
            if result['image_sha256']!=cell['image_sha256']:raise RuntimeError('reopened render differs')
            results.append(dict(cell=i,job_id=id,image_sha256=result['image_sha256']));active=None
    finally:
        render=client.call('render_read')['render']
        if active and render['active'] and render['latest_job_id']==active:
            client.mutate('render_cancel',job_id=active);deadline=time.monotonic()+30
            while client.call('render_read')['render']['active'] and time.monotonic()<deadline:time.sleep(.05)
        state=client.state()
        if state['scene_path']!=original['scene_path']:
            if Path(state['scene_path']).parent!=root/'project':raise RuntimeError('editor left comparison-owned scene; restore refused')
            if state['dirty']:client.mutate('scene_save_as',scene_path=str(root/'project'/('verify-interrupted-'+str(time.time_ns())+'.json')))
            client.mutate('scene_open',scene_path=original['scene_path'])
        if digest(original['scene_path'])!=original_hash:raise RuntimeError('working scene bytes changed')
    result=dict(verified=True,cells=results,original_restored=client.state()['scene_path']==original['scene_path']);publish(root/('verification-'+str(time.time_ns())+'.json'),result);return result

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--scene',type=Path,required=True);p.add_argument('--bridge',type=Path);p.add_argument('--request',type=Path);p.add_argument('--output-root',type=Path);p.add_argument('--verify',type=Path);p.add_argument('--resume',type=Path);p.add_argument('--qualify-renderer',type=Path);p.add_argument('--plan',action='store_true');p.add_argument('--timeout',type=float,default=300);args=p.parse_args()
    if not math.isfinite(args.timeout) or args.timeout<=0:p.error('positive finite timeout required')
    with MCP(args.scene,args.bridge) as client:
        if args.qualify_renderer and (args.resume or args.verify or args.request or args.output_root or args.plan):p.error('--qualify-renderer is exclusive')
        if args.resume and (args.verify or args.request or args.output_root or args.plan):p.error('--resume is exclusive with new-run/verify options')
        if args.qualify_renderer:
            from optical_comparison_renderer_transition import qualify
            result=qualify(client,args.qualify_renderer,args.timeout)
        elif args.resume:result=resume(client,args.resume,args.timeout)
        elif args.verify:result=verify(client,args.verify,args.timeout)
        else:
            if not args.request:p.error('--request required')
            request=validate_request(json.loads(args.request.read_text()))
            if args.plan:result=plan(client,request)
            else:
                if not args.output_root:p.error('--output-root required')
                result=run(client,request,args.output_root,args.timeout)
        print(json.dumps(result,indent=2,allow_nan=False))
if __name__=='__main__':main()

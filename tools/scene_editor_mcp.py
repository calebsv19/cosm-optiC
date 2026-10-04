#!/usr/bin/env python3
"""MCP stdio bridge to the open optiC scene. No offline scene overwrite path."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import socket
import stat
import sys

def number(low,high):return {'type':'number','minimum':low,'maximum':high}
def triple(low,high):return {'type':'array','minItems':3,'maxItems':3,'items':number(low,high)}
ATTACHMENT_SCHEMA={
    'type':'object','required':['schema_version','source_path'],
    'properties':{
        'schema_version':{'const':'ray_atmosphere_attachment_v1'},
        'id':{'type':'string','minLength':1,'maxLength':63},
        'source_path':{'type':'string','description':'Contained project-relative VF3D, pack, cache manifest or bundle path; use atmosphere_import for external inputs.'},
        'source_kind':{'enum':['vf3d','pack','manifest','scene_bundle']},
        **{key:{'type':'boolean'} for key in ('enabled','visible','affects_lighting')},
        'appearance':{'type':'object','properties':{
            'density_scale':number(0,1000),'density_gamma':number(.01,100),
            'absorption_gain':number(0,1000),'scatter_gain':number(0,1000),
            'step_scale':number(.01,100),'tint':triple(0,100)}},
        'placement':{'type':'object','description':'Volume only, relative to cache origin; water sidecars retain their authored coordinates. Volume caustic caches require identity placement in this slice.',
            'properties':{'translation':triple(-1e9,1e9),'rotation_degrees':triple(-36000,36000),'scale':triple(1e-6,1e6)}},
        'playback':{'type':'object','description':'Actual sorted cache IDs. Animated: floor(source_start + (scene_frame - timeline_start) * source_fps / scene_fps). Single-file inputs are static.',
            'properties':{'animated':{'type':'boolean'},'rate_numerator':{'type':'integer','minimum':1,'maximum':1000000},
                'rate_denominator':{'type':'integer','minimum':1,'maximum':1000000},
                'timeline_start':{'type':'integer','minimum':-1000000000,'maximum':1000000000},
                'source_start':{'type':'integer','minimum':0,'maximum':1000000000},'missing':{'enum':['hold','error']}}}
    }
}

METHODS={
    'scene_connect':('scene.connect','Connect to an existing live optiC document by its absolute scene path. Refuses closed documents; does not open or overwrite files.',{'scene_path':{'type':'string'}}),
    'scene_read':('scene.read','Read scene revision, dirty state, playhead and retained atmosphere.',{}),
    'atmosphere_set':('atmosphere.set','Set the complete retained atmosphere using the shared editor validator.',{'attachment':ATTACHMENT_SCHEMA}),
    'atmosphere_import':('atmosphere.import','Import VF3D, pack, manifest or scene bundle with contained dependencies.',{'source':{'type':'string'}}),
    'atmosphere_generate':('atmosphere.generate','Create a static preset without running PhysicsSim. Presets: room_haze_v1, low_floor_fog_v1, cloud_bank_v1, mist_patch_v1, plume_column_v1.',{'preset':{'type':'string'}}),
    'atmosphere_remove':('atmosphere.remove','Remove the retained atmosphere; use scene_undo to restore it.',{}),
    'scene_undo':('scene.undo','Undo one shared editor command.',{}),
    'scene_redo':('scene.redo','Redo one shared editor command.',{}),
    'scene_save':('scene.save','Save using the same authoring save operation as File Save.',{}),
    'scene_seek':('scene.seek','Seek the shared timeline to an absolute frame.',{'frame':{'type':'integer'}}),
}


def tool_list():
    result=[]
    for name,(method,description,fields) in METHODS.items():
        properties=dict(fields)
        if method not in ('scene.read','scene.connect'):properties['expected_revision']={'type':'integer','minimum':0}
        result.append({'name':name,'description':description,'inputSchema':{
            'type':'object','properties':properties,'required':list(properties),'additionalProperties':False}})
    return result


def command(scene: Path, method: str, params: dict) -> dict:
    scene=scene.resolve(strict=True)
    descriptor=Path(str(scene)+'.optic-agent.json')
    if descriptor.is_symlink():raise ValueError('agent endpoint descriptor cannot be a symlink')
    metadata=descriptor.stat()
    if metadata.st_uid!=os.getuid() or not stat.S_ISREG(metadata.st_mode):
        raise ValueError('agent endpoint has a different owner')
    endpoint=json.loads(descriptor.read_text())
    if endpoint.get('schema_version')!='optic_agent_control_v1' or Path(endpoint['scene_path']).resolve()!=scene:
        raise ValueError('agent endpoint does not belong to this scene')
    path=Path(endpoint['socket']);metadata=path.stat()
    if path.is_symlink() or not stat.S_ISSOCK(metadata.st_mode) or metadata.st_uid!=os.getuid():
        raise ValueError('invalid local editor socket')
    with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as client:
        client.settimeout(30)
        client.connect(str(path))
        data=json.dumps({'method':method,'params':params}).encode()+b'\n'
        if len(data)>65535:raise ValueError('command exceeds editor capacity')
        client.sendall(data);response=b''
        while b'\n' not in response:
            part=client.recv(8192)
            if not part:raise ValueError('editor closed before replying')
            response+=part
            if len(response)>65536:raise ValueError('editor response exceeds capacity')
        return json.loads(response.split(b'\n',1)[0])


def serve(scene: Path | None):
    for line in sys.stdin:
        request=None
        try:
            if len(line)>131072:raise ValueError('MCP message exceeds capacity')
            request=json.loads(line)
            if not isinstance(request,dict):raise ValueError('MCP request must be an object')
            if 'id' not in request:continue
            method=request.get('method');params=request.get('params',{})
            if method=='initialize':
                version=params.get('protocolVersion','2024-11-05')
                if version not in ('2024-11-05','2025-03-26','2025-06-18'):version='2025-06-18'
                result={'protocolVersion':version,'capabilities':{'tools':{'listChanged':False}},
                        'serverInfo':{'name':'optic-scene-editor','version':'1.0.0'},
                        'instructions':'Open the scene in optiC, then use scene_connect with its absolute path. Read scene revision before each mutation. All operations share editor Undo and File Save; no solver execution.'}
            elif method=='ping':result={}
            elif method=='tools/list':result={'tools':tool_list()}
            elif method=='tools/call':
                name=params.get('name');arguments=params.get('arguments',{})
                if name not in METHODS or not isinstance(arguments,dict):raise ValueError('unknown tool or invalid arguments')
                schema=next(t['inputSchema'] for t in tool_list() if t['name']==name)
                if set(arguments)!=set(schema['required']):raise ValueError('tool arguments do not match the input schema')
                if name=='scene_connect':
                    candidate=Path(arguments['scene_path'])
                    if not candidate.is_absolute():raise ValueError('scene_connect requires an absolute path')
                    value=command(candidate,'scene.read',{})
                    if value.get('ok'):scene=candidate.resolve(strict=True)
                else:
                    if scene is None:raise ValueError('Use scene_connect with the path of a live optiC document first')
                    value=command(scene,METHODS[name][0],arguments)
                result={'content':[{'type':'text','text':json.dumps(value)}],'isError':not value.get('ok',False)}
            else:
                print(json.dumps({'jsonrpc':'2.0','id':request['id'],'error':{'code':-32601,'message':'Method not found'}}),flush=True)
                continue
            print(json.dumps({'jsonrpc':'2.0','id':request['id'],'result':result}),flush=True)
        except (ValueError,OSError,KeyError,TypeError) as error:
            if isinstance(request,dict) and 'id' in request:
                # Tool execution failures are tool results, not transport errors.
                if request.get('method')=='tools/call':
                    response={'result':{'content':[{'type':'text','text':str(error)}],'isError':True}}
                else:response={'error':{'code':-32602,'message':str(error)}}
                print(json.dumps({'jsonrpc':'2.0','id':request['id'],**response}),flush=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scene',type=Path,help='Optional initial live document; otherwise use scene_connect')
    serve(parser.parse_args().scene)

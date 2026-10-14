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

from scene_editor_mcp_reload import Catalog, requests
from scene_editor_mcp_compat import CATALOG_TOOL, COMMAND_TOOL, fixed_tools, read_catalog, unwrap, validate

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
        data=json.dumps({'method':method,'params':params,'scene_path':str(scene)}).encode()+b'\n'
        if len(data)>65535:raise ValueError('command exceeds editor capacity')
        client.sendall(data);response=b''
        while b'\n' not in response:
            part=client.recv(8192)
            if not part:raise ValueError('editor closed before replying')
            response+=part
            if len(response)>65536:raise ValueError('editor response exceeds capacity')
        return json.loads(response.split(b'\n',1)[0])


def serve(scene: Path | None):
    catalog=Catalog(Path(__file__).with_name("scene_editor_mcp_catalog.py"))
    initialized=False
    pending_catalog_change=False
    for line in requests(sys.stdin):
        changed=catalog.refresh()
        pending_catalog_change=pending_catalog_change or changed
        if pending_catalog_change and initialized:
            print(json.dumps({"jsonrpc":"2.0","method":"notifications/tools/list_changed"}),flush=True)
            pending_catalog_change=False
        if line is None:continue
        request=None
        try:
            if len(line)>131072:raise ValueError('MCP message exceeds capacity')
            request=json.loads(line)
            if not isinstance(request,dict):raise ValueError('MCP request must be an object')
            if 'id' not in request:
                if request.get('method')=='notifications/initialized':initialized=True
                continue
            method=request.get('method');params=request.get('params',{})
            if method=='initialize':
                version=params.get('protocolVersion','2024-11-05')
                if version not in ('2024-11-05','2025-03-26','2025-06-18'):version='2025-06-18'
                result={'protocolVersion':version,'capabilities':{'tools':{'listChanged':True}},
                        'serverInfo':{'name':'optic-scene-editor','version':'1.3.0'},
                        'instructions':'Open the scene in optiC, then use scene_connect with its absolute path. For cached catalogs call editor_catalog, then editor_command with the current catalog_id, listed tool and exact arguments. Read scene revision before each mutation. Edits share editor Undo and File Save. Open/Create require saved edits; Save As copies contained dependencies when changing directories. Render tools run the existing local native renderer and verify results.'}
            elif method=='ping':result={}
            elif method=='tools/list':result={'tools':catalog.tools+fixed_tools()}
            elif method=='tools/call':
                name=params.get('name');arguments=params.get('arguments',{})
                if name==CATALOG_TOOL:
                    validate(fixed_tools()[0]['inputSchema'],arguments)
                    value=read_catalog(catalog,scene)
                    result={'content':[{'type':'text','text':json.dumps(value)}],'isError':False}
                    print(json.dumps({'jsonrpc':'2.0','id':request['id'],'result':result}),flush=True)
                    continue
                if name==COMMAND_TOOL:name,arguments=unwrap(catalog,arguments)
                if name not in catalog.methods or not isinstance(arguments,dict):raise ValueError('unknown tool or invalid arguments')
                schema=next(t['inputSchema'] for t in catalog.tools if t['name']==name)
                validate(schema,arguments)
                if name=='scene_connect':
                    candidate=Path(arguments['scene_path'])
                    if not candidate.is_absolute():raise ValueError('scene_connect requires an absolute path')
                    value=command(candidate,'scene.read',{})
                    if value.get('ok') and Path(value.get('scene_path','')).resolve()!=candidate.resolve(strict=True):raise ValueError('Scene belongs to this editor session but is no longer the active document')
                    if value.get('ok'):scene=candidate.resolve(strict=True)
                else:
                    if scene is None:raise ValueError('Use scene_connect with the path of a live optiC document first')
                    value=command(scene,catalog.methods[name][0],arguments)
                if value.get('ok') and value.get('scene_path'):scene=Path(value['scene_path']).resolve(strict=True)
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

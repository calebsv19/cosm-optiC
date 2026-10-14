"""Exercise catalog replacement over one stdio connection, optionally a live editor."""
import argparse
import hashlib
import json
from pathlib import Path
import queue
import shutil
import socket
import subprocess
import sys
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from scene_editor_mcp_catalog import tool_list
BASE_TOOLS = len(tool_list()) + 2


def run(live_scene=None):
    with tempfile.TemporaryDirectory(prefix='optic-mcp-') as scratch:
        root = Path(scratch)
        for name in ('scene_editor_mcp.py', 'scene_editor_mcp_catalog.py', 'scene_editor_mcp_reload.py', 'scene_editor_mcp_compat.py'):
            shutil.copyfile(ROOT / 'tools' / name, root / name)
        scene = live_scene or root / 'scene.json'
        owner = None
        if live_scene is None:
            scene.write_text('{}')
            owner = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            endpoint = root / 'editor.sock'
            owner.bind(str(endpoint)); owner.listen()
            Path(str(scene) + '.optic-agent.json').write_text(json.dumps(dict(
                schema_version='optic_agent_control_v1', scene_path=str(scene), socket=str(endpoint))))

            def reply():
                while True:
                    try:
                        client, _ = owner.accept()
                    except OSError:
                        return
                    with client:
                        request = b''
                        while b'\n' not in request:
                            request += client.recv(8192)
                        assert json.loads(request)['method'] == 'scene.read'
                        client.sendall(json.dumps(dict(ok=True, revision=7, dirty=False, scene_path=str(scene))).encode() + b'\n')

            threading.Thread(target=reply, daemon=True).start()
        digest_before = hashlib.sha256(scene.read_bytes()).hexdigest()
        with (root / 'stderr.log').open('w+') as errors:
            server = subprocess.Popen([sys.executable, '-B', str(root / 'scene_editor_mcp.py')],
                                      stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=errors, text=True)
            inbox = queue.Queue()

            def receive():
                for line in server.stdout:
                    inbox.put(json.loads(line))

            threading.Thread(target=receive, daemon=True).start()
            sequence = 0

            def send(method, params=None, notification=False):
                nonlocal sequence
                sequence += 1
                request = dict(jsonrpc='2.0', method=method, params=params or {})
                if not notification:
                    request['id'] = sequence
                server.stdin.write(json.dumps(request) + '\n'); server.stdin.flush()
                return sequence

            def rpc(method, params=None):
                identity = send(method, params)
                value = inbox.get(timeout=5)
                assert value.get('id') == identity, value
                assert 'error' not in value, value
                return value['result']

            def call(name, arguments=None):
                result = rpc('tools/call', dict(name=name, arguments=arguments or {}))
                assert not result['isError'], result
                return json.loads(result['content'][0]['text'])

            def notification():
                value = inbox.get(timeout=5)
                assert value == dict(jsonrpc='2.0', method='notifications/tools/list_changed'), value

            def replace(source):
                stage = root / 'catalog.next'
                stage.write_text(source)
                stage.replace(root / 'scene_editor_mcp_catalog.py')

            try:
                identity = rpc('initialize', dict(protocolVersion='2025-06-18', capabilities={},
                                                  clientInfo=dict(name='reload-proof', version='1')))
                assert identity['capabilities']['tools']['listChanged'] is True
                source = (root / 'scene_editor_mcp_catalog.py').read_text()
                replace(source + "\nMETHODS['preinit_read']=('scene.read','Initialization ordering proof',{})\n")
                try:
                    raise AssertionError(inbox.get(timeout=1.1))
                except queue.Empty:
                    pass
                send('notifications/initialized', notification=True)
                notification()
                assert len(rpc('tools/list')['tools']) == BASE_TOOLS+1
                replace(source); notification()
                baseline = rpc('tools/list')['tools']; assert len(baseline) == BASE_TOOLS
                state = call('scene_connect', dict(scene_path=str(scene)))
                assert state == call('scene_read')
                initial_pid = server.pid
                manifest = call('editor_catalog')
                assert len(manifest['tools']) == BASE_TOOLS-2 and manifest['scene_path'] == str(scene.resolve())
                assert call('editor_command', dict(catalog_id=manifest['catalog_id'], tool='scene_read', arguments={})) == state
                added = source + "\nMETHODS['scene_read_probe']=('scene.read','Reload proof read',{})\n"
                # Idle updates must notify without waiting for another request.
                replace(added); notification()
                expanded = rpc('tools/list')['tools']; assert len(expanded) == BASE_TOOLS+1
                assert call('scene_read_probe') == state == call('scene_read')
                stale = rpc('tools/call', dict(name='editor_command', arguments=dict(catalog_id=manifest['catalog_id'], tool='scene_read_probe', arguments={})))
                assert stale['isError']
                manifest = call('editor_catalog')
                assert call('editor_command', dict(catalog_id=manifest['catalog_id'], tool='scene_read_probe', arguments={})) == state
                assert server.pid == initial_pid and server.poll() is None
                # Broken source and broken schemas must leave the last good mapping usable.
                for broken in ('this is not valid Python !!!',
                               added + "\ndef tool_list(): return [{'name':'bad','inputSchema':{}}]\n"):
                    replace(broken)
                    try:
                        unexpected = inbox.get(timeout=1.1)
                        raise AssertionError(unexpected)
                    except queue.Empty:
                        pass
                    assert rpc('tools/list')['tools'] == expanded
                    assert call('scene_read_probe') == state
                    assert call('editor_command', dict(catalog_id=manifest['catalog_id'], tool='scene_read_probe', arguments={})) == state
                # Updating a schema is a list change too; restore removes the added tool.
                revised = added.replace("'Reload proof read',{}", "'Reload proof revised description',{'probe':{'type':'string'}}")
                replace(revised); notification()
                revised_tool = next(t for t in rpc('tools/list')['tools'] if t['name']=='scene_read_probe')
                assert revised_tool['inputSchema']['required'] == ['probe']
                assert rpc('tools/call', dict(name='scene_read_probe', arguments={}))['isError']
                assert call('scene_read_probe', dict(probe='read-only')) == state
                manifest = call('editor_catalog')
                for tool, arguments in (('editor_command', {}), ('missing', {}), ('scene_read_probe', {'probe': 42})):
                    rejected = rpc('tools/call', dict(name='editor_command', arguments=dict(catalog_id=manifest['catalog_id'], tool=tool, arguments=arguments)))
                    assert rejected['isError'], rejected
                assert call('editor_command', dict(catalog_id=manifest['catalog_id'], tool='scene_read_probe', arguments=dict(probe='read-only'))) == state
                replace(source); notification()
                assert rpc('tools/list')['tools'] == baseline
                assert call('scene_read') == state
                # Same bytes under a new inode must not trigger a spurious notification.
                replace(source)
                try:
                    raise AssertionError(inbox.get(timeout=1.1))
                except queue.Empty:
                    pass
                unknown = rpc('tools/call', dict(name='scene_read_probe', arguments={}))
                assert unknown['isError']
                assert hashlib.sha256(scene.read_bytes()).hexdigest() == digest_before
                server.stdin.close(); server.wait(timeout=5); assert server.returncode == 0
                errors.seek(0); diagnostics = errors.read()
                assert diagnostics.count('catalog update rejected') == 2, diagnostics
                return dict(passed=True, initial_tools=BASE_TOOLS, expanded_tools=BASE_TOOLS+1, same_process=True,
                            attachment_preserved=True, invalid_update_fallback=True,
                            idle_notifications=True, initialization_order=True, schema_update=True, removal=True,
                            compatibility_dispatch=True, stale_catalog_refusal=True, current_schema_refusal=True,
                            eof_clean=True, scene_sha256=digest_before, live_editor=live_scene is not None)
            finally:
                if server.poll() is None:
                    server.terminate(); server.wait(timeout=5)
                if owner:
                    owner.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--live-scene', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = run(args.live_scene.resolve(strict=True) if args.live_scene else None)
    if args.output:
        args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))

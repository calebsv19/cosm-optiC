"""Qualify the real Codex MCP client without resetting any existing connection."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import tomllib

ROOT = Path(__file__).resolve().parents[2]
PROBE = 'optic_catalog_verification'
sys.path.insert(0, str(ROOT / 'tools'))
from scene_editor_mcp_catalog import tool_list
BASE_TOOLS = len(tool_list()) + 2

# Transparent task-owned transport capture. Only this copied bridge is updated.
WRAPPER = '''import json,subprocess,sys,threading
from pathlib import Path
root=Path(__file__).parent
log=(root/'wire.jsonl').open('a',buffering=1)
lock=threading.Lock()
def record(direction,value):
    with lock:log.write(json.dumps(dict(direction=direction,value=value))+'\\n')
child=subprocess.Popen([sys.executable,'-B',str(root/'scene_editor_mcp.py')],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
record('process',dict(pid=child.pid))
def read():
    for line in child.stdout:
        record('server',json.loads(line));sys.stdout.write(line);sys.stdout.flush()
threading.Thread(target=read,daemon=True).start()
try:
    for line in sys.stdin:
        record('client',json.loads(line));child.stdin.write(line);child.stdin.flush()
finally:
    child.stdin.close()
    try:child.wait(timeout=5)
    except subprocess.TimeoutExpired:child.terminate();child.wait(timeout=5)
'''


def processes():
    result = subprocess.check_output(['ps', '-axo', 'pid=,comm='], text=True)
    return {int(line.split(maxsplit=1)[0]): line.split(maxsplit=1)[1]
            for line in result.splitlines() if line.strip()}


def verify(binary, scene, output):
    output.mkdir(parents=True, exist_ok=False)
    before = processes()
    config_path = Path.home() / '.codex/config.toml'
    config_bytes = config_path.read_bytes()
    config = tomllib.loads(config_bytes.decode())
    scene_hash = hashlib.sha256(scene.read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory(prefix='optic-codex-') as temp:
        scratch = Path(temp)
        for name in ('scene_editor_mcp.py', 'scene_editor_mcp_catalog.py', 'scene_editor_mcp_reload.py', 'scene_editor_mcp_compat.py'):
            shutil.copyfile(ROOT / 'tools' / name, scratch / name)
        (scratch / 'capture.py').write_text(WRAPPER)
        args = [str(binary), 'app-server', '--listen', 'stdio://']
        for name in config.get('mcp_servers', {}):
            args += ['-c', f'mcp_servers.{name}.enabled=false']
        # Suppress plugin/application connection discovery in this child only.
        args += ['-c', 'features.plugins=false', '-c', 'features.apps=false']
        server_config = '{command=' + json.dumps(sys.executable) + ',args=[' + json.dumps(str(scratch / 'capture.py')) + ']}'
        args += ['-c', f'mcp_servers.{PROBE}={server_config}']
        args += ['-c', 'mcp_optional_startup_grace_ms=0']
        with (output / 'app-server.log').open('w') as log:
            child = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=log, text=True, cwd=ROOT)
            inbox = queue.Queue()
            notifications = []
            transcript = []

            def read():
                for line in child.stdout:
                    inbox.put(json.loads(line))

            threading.Thread(target=read, daemon=True).start()
            sequence = 0

            def rpc(method, params=None, timeout=30):
                nonlocal sequence
                sequence += 1
                request = dict(id=sequence, method=method, params=params or {})
                transcript.append(dict(direction='request', value=request))
                child.stdin.write(json.dumps(request) + '\n'); child.stdin.flush()
                deadline = time.monotonic() + timeout
                while time.monotonic() < deadline:
                    value = inbox.get(timeout=max(.1, deadline-time.monotonic()))
                    if value.get('id') == sequence:
                        transcript.append(dict(direction='response', value=value))
                        assert 'error' not in value, value
                        return value['result']
                    notifications.append(value)
                raise TimeoutError(method)

            def notify(method):
                child.stdin.write(json.dumps(dict(method=method)) + '\n'); child.stdin.flush()

            thread_id = None
            try:
                identity = rpc('initialize', dict(clientInfo=dict(name='optic_catalog_verification', version='1'),
                                                  capabilities=dict(experimentalApi=True)))
                notify('initialized')
                started = rpc('thread/start', dict(cwd=str(ROOT), ephemeral=True,
                                                   approvalPolicy='never', sandbox='read-only'))
                assert started['thread']['ephemeral'] is True
                thread_id = started['thread']['id']

                def inventory(selected_thread=None):
                    result = rpc('mcpServerStatus/list', dict(threadId=selected_thread or thread_id, serverName=PROBE,
                                                            detail='toolsAndAuthOnly', limit=100))
                    transcript.append(dict(direction='inventory', value=result))
                    servers = result['data']
                    assert len(servers) == 1 and servers[0]['name'] == PROBE, servers
                    return servers[0]['tools']

                def names(tools):
                    if isinstance(tools, dict):
                        return {t['name'] for t in tools.values()}
                    return {t['name'] for t in tools}

                def call(tool, arguments=None, selected_thread=None):
                    result = rpc('mcpServer/tool/call', dict(threadId=selected_thread or thread_id, server=PROBE,
                                                           tool=tool, arguments=arguments or {}))
                    assert not result.get('isError', False), result
                    return json.loads(result['content'][0]['text'])

                baseline = inventory(); assert len(names(baseline)) == BASE_TOOLS, names(baseline)
                state = call('scene_connect', dict(scene_path=str(scene)))
                assert state == call('scene_read')
                initial_catalog = call('editor_catalog')
                assert len(initial_catalog['tools']) == BASE_TOOLS-2
                assert call('editor_command', dict(catalog_id=initial_catalog['catalog_id'], tool='material_read', arguments=dict(object_id='surface'))) == call('material_read', dict(object_id='surface'))
                catalog = scratch / 'scene_editor_mcp_catalog.py'
                original = catalog.read_text()

                def replace(source):
                    next_file = scratch / 'catalog.next'
                    next_file.write_text(source); next_file.replace(catalog)

                added = original + "\nMETHODS['scene_read_catalog_probe']=('scene.read','Read-only catalog discovery proof',{})\n"
                replace(added)
                # One bounded discovery read after allowing the idle notification to arrive.
                time.sleep(5)
                expanded = inventory()
                discovered = 'scene_read_catalog_probe' in names(expanded)
                current_catalog = call('editor_catalog')
                assert 'scene_read_catalog_probe' in names(current_catalog['tools'])
                assert call('editor_command', dict(catalog_id=current_catalog['catalog_id'], tool='scene_read_catalog_probe', arguments={})) == state
                def refuse(tool, arguments, token):
                    result = rpc('mcpServer/tool/call', dict(threadId=thread_id, server=PROBE,
                                                           tool='editor_command', arguments=dict(catalog_id=token, tool=tool, arguments=arguments)))
                    assert result['isError'], result

                # A well-typed mutation still reaches the native revision guard.
                refuse('material_set_layer_value', dict(object_id='recipient', layer_id='base',
                       group='', property='opacity', value=.57, expected_revision=state['revision']+1), current_catalog['catalog_id'])
                assert call('scene_read') == state
                refuse('scene_read_catalog_probe', {}, initial_catalog['catalog_id'])
                revised = added.replace("'Read-only catalog discovery proof',{}", "'Read-only catalog discovery proof',{'probe':{'type':'integer','minimum':0,'maximum':2}}")
                replace(revised)
                revised_catalog = call('editor_catalog')
                assert revised_catalog['catalog_id'] != current_catalog['catalog_id']
                for bad in ({}, {'probe': True}, {'probe': 3}, {'probe': 1, 'extra': 0}):
                    refuse('scene_read_catalog_probe', bad, revised_catalog['catalog_id'])
                refuse('editor_command', {}, revised_catalog['catalog_id'])
                refuse('not_in_catalog', {}, revised_catalog['catalog_id'])
                assert call('editor_command', dict(catalog_id=revised_catalog['catalog_id'], tool='scene_read_catalog_probe', arguments=dict(probe=1))) == state
                # Last-good catalog remains usable after an invalid on-disk update.
                replace('invalid Python !!!')
                assert call('editor_catalog')['catalog_id'] == revised_catalog['catalog_id']
                assert call('editor_command', dict(catalog_id=revised_catalog['catalog_id'], tool='scene_read_catalog_probe', arguments=dict(probe=1))) == state
                replace(added)
                # A fresh ephemeral connection tests startup discovery independently
                # of notification handling. No existing session is refreshed.
                second = rpc('thread/start', dict(cwd=str(ROOT), ephemeral=True,
                                                  approvalPolicy='never', sandbox='read-only'))
                assert second['thread']['ephemeral'] is True
                second_id = second['thread']['id']
                second_inventory = inventory(second_id)
                assert 'scene_read_catalog_probe' in names(second_inventory)
                assert call('scene_connect', dict(scene_path=str(scene)), second_id) == state
                assert call('scene_read_catalog_probe', selected_thread=second_id) == state
                if discovered:
                    assert call('scene_read_catalog_probe') == state
                replace(original); time.sleep(1)
                third = rpc('thread/start', dict(cwd=str(ROOT), ephemeral=True,
                                                 approvalPolicy='never', sandbox='read-only'))
                assert third['thread']['ephemeral'] is True
                restored = inventory(third['thread']['id']); assert names(restored) == names(baseline)
                restored_catalog = call('editor_catalog')
                refuse('scene_read_catalog_probe', {}, restored_catalog['catalog_id'])
                assert call('scene_read') == state
                wire = [json.loads(line) for line in (scratch / 'wire.jsonl').read_text().splitlines()]
                bridge_pids = [row['value']['pid'] for row in wire if row['direction'] == 'process']
                assert len(bridge_pids) == 3, bridge_pids
                change_notifications = sum(row['direction']=='server' and row['value'].get('method')=='notifications/tools/list_changed' for row in wire)
                assert change_notifications >= 2
                assert hashlib.sha256(scene.read_bytes()).hexdigest() == scene_hash
                assert config_path.read_bytes() == config_bytes
                after = processes()
                preserved = {pid: command for pid, command in before.items()
                             if 'codex' in command.lower() or 'node' in command.lower() or 'python' in command.lower()}
                missing = [pid for pid, command in preserved.items() if after.get(pid) != command]
                assert not missing, missing
                receipt = dict(verified_checkpoint=True, live_notification_discovery=discovered,
                               new_connection_discovery=True, app_server_identity=identity, ephemeral=True,
                               initial_tools=BASE_TOOLS, updated_tools=BASE_TOOLS+1, restored_tools=BASE_TOOLS,
                               new_tool_call=True, cached_connection_compatibility=True,
                               compatibility_catalog_discovery=True, current_schema_validation=True,
                               stale_catalog_refusal=True, last_good_compatibility=True,
                               removed_command_refusal=True, native_revision_guard=True,
                               bridge_pids=bridge_pids, change_notifications=change_notifications,
                               scene_attachment_preserved=True, scene_sha256=scene_hash,
                               persistent_config_unchanged=True, preserved_process_ids=list(preserved),
                               desktop_binary=str(binary), desktop_binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest())
                (output / 'result.json').write_text(json.dumps(receipt, indent=2)+'\n')
                return receipt
            finally:
                if (scratch / 'wire.jsonl').exists():
                    shutil.copyfile(scratch / 'wire.jsonl', output / 'mcp-wire.jsonl')
                (output / 'app-server-transcript.json').write_text(json.dumps(dict(rpc=transcript,notifications=notifications),indent=2)+'\n')
                child.stdin.close()
                try:
                    child.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    child.terminate(); child.wait(timeout=5)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--codex-binary', type=Path, required=True)
    parser.add_argument('--scene', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.codex_binary.resolve(strict=True), args.scene.resolve(strict=True), args.output.resolve())))

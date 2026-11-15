"""Actual stdio MCP bridge against the native V3 editor under test."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

repo = Path(__file__).resolve().parents[2]
scene = Path(sys.argv[1]).resolve()
with subprocess.Popen([sys.executable, '-B', str(repo / 'tools/scene_editor_mcp.py'),
        '--scene', str(scene)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True) as bridge:
    sequence = 0

    def rpc(method, params=None):
        global sequence
        sequence += 1
        bridge.stdin.write(json.dumps(dict(jsonrpc='2.0', id=sequence,
            method=method, params=params or {})) + '\n')
        bridge.stdin.flush()
        while True:
            response = json.loads(bridge.stdout.readline())
            if response.get('id') == sequence:
                break
        assert 'error' not in response, response
        return response['result']

    def call(name, arguments=None):
        result = rpc('tools/call', dict(name=name, arguments=arguments or {}))
        assert not result['isError'], result
        return json.loads(result['content'][0]['text'])

    rpc('initialize', dict(protocolVersion='2025-06-18', capabilities={},
        clientInfo=dict(name='v3-transport-proof', version='1')))
    tools = rpc('tools/list')['tools']
    assert {'render_start_v3', 'render_profile_read', 'render_profile_set'} <= {t['name'] for t in tools}
    original = call('render_profile_read')['spectral_profile']['profile']
    disk = scene.read_bytes()
    changed = copy.deepcopy(original)
    changed['surface_path_v1']['seed'] = 21
    revision = call('scene_read')['revision']
    call('render_profile_set', dict(profile=changed, expected_revision=revision))
    revision = call('scene_read')['revision']
    call('render_start_v3', dict(request_id='v3-mcp', width=32, height=24,
        integrator='disney_v3_spectral', temporal_frames=1, frame=0,
        camera_samples_override=16, expected_revision=revision))
    deadline = time.monotonic() + 45
    while time.monotonic() < deadline:
        status = call('render_status', dict(job_id='v3-mcp'))['render']
        if status['state'] not in ('preparing', 'rendering', 'canceling'):
            break
        time.sleep(.01)
    result = call('render_results', dict(job_id='v3-mcp'))['render']
    assert result['state'] == 'completed' and result['artifact_integrity']
    assert result['spectral_transport']['quality']['seed'] == 21
    report = Path(result['spectral_transport_path'])
    assert hashlib.sha256(report.read_bytes()).hexdigest() == result['spectral_transport_sha256']
    call('render_profile_set', dict(profile=original, expected_revision=call('scene_read')['revision']))
    assert scene.read_bytes() == disk
    (scene.parent / 'mcp-transport-result.json').write_text(json.dumps(result, indent=2))
    bridge.stdin.close()
    bridge.wait(timeout=5)
    assert bridge.returncode == 0
print('PASS actual stdio V3 profile/read/set, render-start, status/results and sidecar hash')

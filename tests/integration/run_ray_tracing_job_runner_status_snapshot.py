#!/usr/bin/env python3
"""A generic renderer status must not hide immutable worker metadata."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runner', type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='ray-status-snapshot-') as tmp:
        jobs = Path(tmp) / 'jobs'; job = jobs / 'status_snapshot'; job.mkdir(parents=True)
        generic = {'schema_version': 'ray_tracing_detached_job_status_v1',
                   'job_id': job.name, 'state': 'completed', 'stage': 'completed',
                   'frames_completed': 1, 'effective_frame_count': 1}
        (job/'job_status.json').write_text(json.dumps(generic))
        binding = {'schema_version': 'ray_tracing_worker_client_state_v1',
                   'execution_mode': 'worker_protocol', 'worker_protocol_version': 1,
                   'immutable_request_sha256': 'a'*64, 'renderer_build_sha256': 'b'*64}
        (job/'worker_client_state.json').write_text(json.dumps(binding))
        for mode, version in [('worker_protocol', 1), ('direct_fallback', 0)]:
            binding.update(execution_mode=mode, worker_protocol_version=version)
            (job/'worker_client_state.json').write_text(json.dumps(binding))
            # Model the renderer's later generic replacement deterministically.
            (job/'job_status.json').write_text(json.dumps(generic))
            result = subprocess.run([str(args.runner.resolve()), 'status', '--job-id', job.name,
                                     '--jobs-root', str(jobs)], capture_output=True, text=True)
            assert result.returncode == 0, result.stderr
            status = json.loads(result.stdout)
            assert status['state'] == 'completed'
            for key in ('execution_mode', 'worker_protocol_version', 'immutable_request_sha256', 'renderer_build_sha256'):
                assert status.get(key) == binding[key], (key, status)
            assert json.loads((job/'job_status.json').read_text()) == generic
        print('status emits merged worker metadata for protocol and direct snapshots')


if __name__ == '__main__': main()

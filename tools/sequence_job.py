"""Use the existing native job owner; retain its identity across wait timeouts."""
import json
import os
from pathlib import Path
import subprocess


def submit_and_wait(runner, request, folder, timeout=300, env=None):
    folder=Path(folder)
    jobs=folder/'jobs';jobs.mkdir()
    receipt=folder/'native-job.json'
    def save(value):
        temporary=receipt.with_suffix('.tmp')
        with temporary.open('w') as stream:
            json.dump(value,stream,indent=2);stream.write('\n');stream.flush();os.fsync(stream.fileno())
        os.replace(temporary,receipt)
    binding=dict(schema='ray_sequence_native_job_v1',runner=str(Path(runner).resolve()),
                 jobs_root=str(jobs.resolve()),request=str(Path(request).resolve()),
                 state='submitting',automatic_resubmission=False)
    save(binding)
    try:
        submitted=subprocess.run([str(runner),'submit','--request',str(request),
                                  '--jobs-root',str(jobs)],capture_output=True,text=True,
                                 check=True,timeout=30,env=env)
        binding.update(job_id=json.loads(submitted.stdout)['job_id'],state='submitted')
        save(binding) # Native job ID is durable before beginning the bounded wait.
    except BaseException as exc:
        binding.update(state='submission_unverified',error=str(exc));save(binding)
        raise
    command=[str(runner),'wait','--job-id',binding['job_id'],'--jobs-root',str(jobs),
             '--timeout-seconds',str(timeout)]
    binding['continuation_argv']=command
    save(binding)
    try:
        completed=subprocess.run(command,capture_output=True,text=True,timeout=timeout+15,env=env)
        binding.update(wait_exit_code=completed.returncode,wait_stdout=completed.stdout,
                       diagnostics=completed.stderr)
        save(binding) # Preserve native diagnostics even when stdout is not valid JSON.
        binding['native_status']=json.loads(completed.stdout)
        status=binding['native_status']
        if completed.returncode or status.get('state')!='completed' or status.get('completion_verified') is not True:
            binding['state']='wait_timeout' if completed.returncode==124 else 'native_completion_unverified'
            save(binding)
            raise ValueError(f"Native job {binding['job_id']} has no verified completion; inspect {receipt} and continue the same job")
        binding['state']='verified_completed';save(binding)
        return binding
    except BaseException as exc:
        if binding['state']=='submitted':
            binding.update(state='wait_unverified',error=str(exc));save(binding)
        raise

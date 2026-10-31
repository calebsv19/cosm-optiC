"""Off-process timeout, malformed-result and false-completion reporting cases."""
from pathlib import Path
import json
import os
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from sequence_job import submit_and_wait

with tempfile.TemporaryDirectory(prefix='optic-native-reporting-') as temp:
 root=Path(temp)
 runner=root/'fake-native-owner'
 runner.write_text('''#!/usr/bin/env python3
import json,os,sys
from pathlib import Path
if sys.argv[1]=='submit':
 with Path(os.environ['SUBMIT_COUNT']).open('a') as f:f.write('submit\\n')
 print(json.dumps(dict(job_id='native-original-id',status='submitted')))
else:
 print(os.environ['WAIT_RESULT'])
 print('native diagnostic retained',file=sys.stderr)
 raise SystemExit(int(os.environ['WAIT_EXIT']))
''');runner.chmod(0o700)
 request=root/'request.json';request.write_text('{}\n')
 for label,code,result in [('timeout',124,json.dumps(dict(state='running',completion_verified=False))),('malformed',1,'not-json'),('false-completion',0,json.dumps(dict(state='completed')))]:
  folder=root/label;folder.mkdir();count=folder/'submit-count'
  env=dict(os.environ,SUBMIT_COUNT=str(count),WAIT_EXIT=str(code),WAIT_RESULT=result)
  try:submit_and_wait(runner,request,folder,1,env)
  except (ValueError,json.JSONDecodeError):pass
  else:raise AssertionError(label+' became success')
  receipt=json.loads((folder/'native-job.json').read_text())
  assert receipt['job_id']=='native-original-id' and receipt['automatic_resubmission'] is False
  assert receipt['wait_exit_code']==code and receipt['wait_stdout'].strip()==result
  assert 'native diagnostic retained' in receipt['diagnostics']
  assert 'native-original-id' in receipt['continuation_argv']
  try:submit_and_wait(runner,request,folder,1,env)
  except FileExistsError:pass
  else:raise AssertionError('occupied delivery resubmitted')
  assert count.read_text()=='submit\n'
 print('Native job reporting PASS: timeout/malformed/false completion, diagnostics preserved, same ID, no duplicate submission')

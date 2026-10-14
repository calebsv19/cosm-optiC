"""Guard tests for explicit build qualification; native pixel parity is separate."""
import argparse,copy,json,sys
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import optical_comparison_renderer_transition as transition
import optical_comparison_recovery as recovery
from optical_comparison_mcp import canonical,digest,publish
import hashlib
p=argparse.ArgumentParser();p.add_argument('--output-root',type=Path,required=True);args=p.parse_args();out=args.output_root.resolve();out.mkdir()

def fixture(name):
 root=out/name;project=root/'project';project.mkdir(parents=True);original=root/'original.json';baseline=project/'baseline.json';cell=project/'cell-00.json'
 for path in (original,baseline,cell):publish(path,dict(schema_family='codework_scene',schema_variant='scene_runtime_v1',objects=[],producer={'retained':True}))
 image=project/'old.bmp';image.write_bytes(b'original-image')
 request=dict(schema='optic_optical_comparison_v1',comparison_id='guard',object_id='orb',axes=[dict(parameter='ior',values=[1.,1.5]),dict(parameter='roughness',values=[0.,1.])],settings={},render=dict(width=16,height=16,temporal_frames=1,frame=0,integrator='disney_v2',sample_sequence=901))
 receipt=dict(schema=recovery.SCHEMA,state='failed',original_restored=True,renderer_sha256='a'*64,request=request,request_sha256=hashlib.sha256(canonical(request).encode()).hexdigest(),plan=dict(original_scene=str(original),original_sha256=digest(original)),baseline_relative='project/baseline.json',baseline_sha256=digest(baseline),dependencies={},pending=None,cells=[dict(row=0,column=0,values=dict(ior=1.,roughness=0.),scene_relative='project/cell-00.json',scene_sha256=digest(cell),image_relative='project/old.bmp',image_sha256=digest(image),response=dict(ior=1.))])
 publish(root/'matrix.json',receipt);return root,receipt

class Client:
 def __init__(self,root,receipt,bad=False):self.root=root;self.receipt=receipt;self.scene=receipt['plan']['original_scene'];self.bad=bad
 def state(self):return dict(scene_path=self.scene,dirty=False)
 def call(self,name,args=None):
  if name=='render_read':return dict(render=dict(active=False,fixed_sampling_supported=True))
  if name=='material_response_read':return dict(response=self.receipt['cells'][0]['response'])
  raise AssertionError(name)
 def mutate(self,name,**args):
  if name=='scene_open':self.scene=args['scene_path'];return
  assert name=='render_start_seeded'
  job=self.root/'project/.optic-renders'/args['request_id'];job.mkdir(parents=True);image=job/'frame.bmp';image.write_bytes(b'different-image' if self.bad else b'original-image');snapshot=job/'source_snapshot.json';snapshot.write_bytes(Path(self.scene).read_bytes())
  self.result=dict(job_id=args['request_id'],state='completed',artifact_integrity=True,settings=self.receipt['request']['render'],scene_path=self.scene,sample_sequence=901,image_path=str(image),image_sha256=digest(image),source_sha256=digest(snapshot))

root,r=fixture('success');client=Client(root,r)
with patch.object(transition,'engine',return_value='b'*64),patch.object(transition,'wait_render',side_effect=lambda c,*_:c.result):
 proof=transition.qualify(client,root)
assert proof['original_restored'] and client.scene==r['plan']['original_scene']
updated=json.loads((root/'matrix.json').read_text());assert updated['renderer_sha256']=='b'*64 and updated['cells']==r['cells'];recovery.check_seals(root,updated)
# Generic changed-build resume still refuses an unqualified journal.
other,receipt=fixture('unqualified')
with patch.object(recovery,'engine',return_value='b'*64):
 try:recovery.preflight(Client(other,receipt),other,receipt);assert False
 except ValueError as e:assert 'build changed' in str(e)
# A mismatched image restores the document and leaves the exact journal bytes intact.
bad,receipt=fixture('pixel-mismatch');before=(bad/'matrix.json').read_bytes();client=Client(bad,receipt,True)
with patch.object(transition,'engine',return_value='b'*64),patch.object(transition,'wait_render',side_effect=lambda c,*_:c.result):
 try:transition.qualify(client,bad);assert False
 except ValueError as e:assert 'parity failed' in str(e)
assert (bad/'matrix.json').read_bytes()==before and client.scene==receipt['plan']['original_scene']
# Changed or missing proof artifacts are not accepted by resume seal checking.
path=root/updated['renderer_transitions'][0]['proof_relative'];path.write_text('{}')
try:recovery.check_seals(root,updated);assert False
except ValueError as e:assert 'proof changed' in str(e)
publish(out/'acceptance.json',dict(passed=True,completed_cells_preserved=True,unqualified_build_refused=True,pixel_mismatch_restores_without_journal_change=True,proof_tamper_refused=True))
print('Verified renderer transition guards PASS')

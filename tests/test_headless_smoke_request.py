"""Typed physical-smoke request rejection gates; a public local preflight only."""
import copy,json,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];WORKER=ROOT/'build/toolchains/clang/arm64/tools/cli/ray_tracing_render_headless'
class SmokeRequestTests(unittest.TestCase):
 def test_strict_physical_profile(self):
  original=json.loads((ROOT/'tests/fixtures/agent_render_preflight_request.json').read_text())
  for k,v in original['scene'].items():
   if k.endswith('_path') and not Path(v).is_absolute():original['scene'][k]=str((ROOT/'tests/fixtures'/v).resolve())
  original['volume']={'enabled':False,'affects_lighting':True,'sampling_layout':'cell_centered'}
  original['render']['integrator_3d']='direct_light';original['inspection']={'volume_smoke_optics':{'schema':'physical_smoke_v1','reference_concentration_kg_m3':.0002,'mass_extinction_m2_kg':1250,'scattering_albedo':.65,'anisotropy':.35,'sky_illumination':True}}
  cases=[('valid',original,True)]
  bounded=copy.deepcopy(original);bounded['inspection']['volume_smoke_optics']['sky_error_budget']=1e-5;cases.append(('bounded_sky',bounded,True))
  for name,key,value in [('albedo','scattering_albedo',1.01),('g','anisotropy',1),('numeric_string','mass_extinction_m2_kg','1250'),('boolean_number','reference_concentration_kg_m3',True),('sky_number','sky_illumination',1),('unknown_field','extra',1),('budget_limit','sky_error_budget',.1),('budget_boolean','sky_error_budget',True)]:
   bad=copy.deepcopy(original);bad['inspection']['volume_smoke_optics'][key]=value;cases.append((name,bad,False))
  for value in ('node',True):
   bad=copy.deepcopy(original);bad['volume']['sampling_layout']=value;cases.append(('bad_layout',bad,False))
  bad=copy.deepcopy(original);bad['inspection']['volume_density_scale']=1;cases.append(('mixed_legacy',bad,False))
  bad=copy.deepcopy(original);bad['render']['integrator_3d']='disney_v2';cases.append(('unsupported_integrator',bad,False))
  with tempfile.TemporaryDirectory() as tmp:
   for name,req,accepted in cases:
    req['output']={'root':str(Path(tmp)/'render'),'overwrite':True};path=Path(tmp)/'request.json';path.write_text(json.dumps(req));result=subprocess.run([str(WORKER),'--request',str(path),'--preflight','--summary',str(Path(tmp)/'summary.json')],capture_output=True,text=True)
    self.assertEqual(result.returncode==0,accepted,(name,result.stderr))
if __name__=='__main__':unittest.main()

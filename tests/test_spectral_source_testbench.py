"""Authoring and safety checks; native optical proof is a separate CLI run."""
from pathlib import Path
import copy,json,subprocess,sys,tempfile
REPO=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(REPO/'tools'))
from spectral_source_testbench import load_base,request_for

def main():
    base=load_base(REPO/'tests/fixtures/spectral_surface_map/request.json')
    original=copy.deepcopy(base)
    with tempfile.TemporaryDirectory(prefix='spectral-source-contract-') as tmp:
        root=Path(tmp)
        for width in (0,10):
            q=request_for(base,450,width,1,19,16,'full',(240,172),root/'image','test')
            assert base==original
            assert q['inspection']==base['inspection'] and q['scene']==base['scene']
            a=copy.deepcopy(q['spectral']);b=copy.deepcopy(base['spectral'])
            a.pop('sampling');b.pop('sampling',None)
            for v in (a,b):v['surface_map'].pop('seed');v['surface_map'].pop('sources')
            assert a==b,'Reconstruction/medium/optics drift'
            lamp=q['spectral']['surface_map']['sources'][0]
            if width:assert lamp['spectrum']['visible_watts']==1 and 'lines' not in lamp
            else:assert lamp['lines']==[[450,1]] and lamp['continuum']==[] and 'spectrum' not in lamp
        controlled=request_for(base,450,0,1,19,16,'full',(240,172),root/'controlled','controlled','progressive',0)
        assert controlled['spectral']['sampling']['camera_seed']==0
        assert controlled['spectral']['sampling']['camera_sequence']=='progressive'
        assert controlled['spectral']['surface_map']['seed']==19
        cmd=[sys.executable,str(REPO/'tools/spectral_source_testbench.py'),'--output-root',str(root/'prepared')]
        result=subprocess.run(cmd,capture_output=True,text=True);assert result.returncode==0,result.stderr
        rows=json.loads((root/'prepared/results.json').read_text());assert len(rows)==6
        assert all(r['status']=='prepared' and not (root/'prepared'/r['name']/'output').exists() for r in rows)
        before=(root/'prepared/results.json').read_bytes()
        assert subprocess.run(cmd,capture_output=True).returncode!=0
        assert (root/'prepared/results.json').read_bytes()==before
        for args in [['--camera-seed','-1'],['--camera-seed','4294967296'],['--camera-sequence','unknown'],['--wavelengths','365'],['--wavelengths','nan'],['--seeds','19','19'],['--watts','-1'],['--passes','1000'],['--size','1440','1032'],['--size','240','172'],['--fwhm-nm','.001']]:
            rejected=root/'rejected'
            result=subprocess.run([*cmd[:-1],str(rejected),*args],capture_output=True)
            assert result.returncode!=0 and not rejected.exists(),args
    print('PASS source authoring preserves camera/reconstruction, line/Gaussian distinction, six prepared cells, no overwrite, visible-range and cost limits')
if __name__=='__main__':main()

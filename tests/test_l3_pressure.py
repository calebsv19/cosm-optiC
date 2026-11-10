"""Contained L3 allocation, progress and owned child-process proofs."""
from pathlib import Path
import subprocess,os,tempfile
r=Path(__file__).resolve().parents[1];w=r.parents[1]
incs=[r/'include',r/'src',r/'src/tools',r/'tests',r/'third_party/codework_shared/shape/external/cjson',w/'fisiCs/include',Path('/opt/homebrew/include'),Path('/opt/homebrew/include/SDL2')]+list((r/'third_party/codework_shared').glob('core/*/include'))+list((w/'shared/core').glob('*/include'))
incs+=list((r/'third_party/codework_shared').glob('kit/*/include'))+list((w/'shared/kit').glob('*/include'))
with tempfile.TemporaryDirectory(prefix='optic-l3-pressure-') as temp:
 for san in [False,True]:
  for test in ['async_job_pressure_test.c','async_progress_pressure_test.c','rigid_job_lifetime_test.c','catalog_admission_test.c','direct_light_seed_boundary_test.c','picker_child_lifetime_test.c','import_child_lifetime_test.c','l3_import_receipt_fault_test.c']:
   cmd=['/usr/bin/clang','-std=c11','-g','-Wl,-dead_strip','-Wall','-Wextra','-Werror',*(['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if san else []),*[f'-I{p}' for p in incs],str(r/'tests'/test),'-L/opt/homebrew/lib','-ljson-c','-lSDL2','-o',str(Path(temp)/'probe')]
   subprocess.run(cmd,check=True);subprocess.run([str(Path(temp)/'probe')],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),check=True)
   print('PASS',test,'sanitized' if san else 'strict',flush=True)

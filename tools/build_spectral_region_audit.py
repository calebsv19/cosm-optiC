"""Build a test-only exact-sensor region binary from a trusted local CLI build log.
This relinks existing objects; it never edits production transport source.
"""
import argparse,hashlib,json,shlex,subprocess
from pathlib import Path

def build(repo,build_log,output):
 output.mkdir(parents=True,exist_ok=False)
 lines=build_log.read_text().splitlines()
 compile_line=next(x for x in lines if ' -c src/render/runtime_spectral_surface_path.c ' in x)
 original=shlex.split(compile_line)
 base=original+['-DRuntimeSpectralSurfacePath_Render=RuntimeSpectralSurfacePath_RenderFullAuditBaseline']
 base[base.index('-o')+1]=str(output/'region-base.o')
 wrapper=original.copy();wrapper[wrapper.index('-c')+1]='tests/spectral_region_audit.c'
 wrapper[wrapper.index('-o')+1]=str(output/'region-wrapper.o')
 link_line=next(x for x in reversed(lines) if ' -o ' in x and 'tools/cli/ray_tracing_render_headless ' in x)
 link=shlex.split(link_line)
 i=next(i for i,x in enumerate(link) if x.endswith('/render/runtime_spectral_surface_path.o'))
 link[i:i+1]=[str(output/'region-base.o'),str(output/'region-wrapper.o')]
 binary=output/'ray_tracing_region_audit';link[link.index('-o')+1]=str(binary)
 for command in (base,wrapper,link):subprocess.run(command,cwd=repo,check=True)
 receipt={'mode':'test_only_full_sensor_regions','production_source_edited':False,
          'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
          'harness_sha256':hashlib.sha256((repo/'tests/spectral_region_audit.c').read_bytes()).hexdigest(),
          'build_log_sha256':hashlib.sha256(build_log.read_bytes()).hexdigest(),'commands':[base,wrapper,link]}
 (output/'build-receipt.json').write_text(json.dumps(receipt,indent=2))
 print(binary)

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[1])
 p.add_argument('--build-log',type=Path,required=True,help='Trusted fresh normal CLI build log; referenced objects must still exist')
 p.add_argument('--output',type=Path,required=True,help='New task-owned build directory')
 a=p.parse_args();build(a.repo.resolve(),a.build_log.resolve(),a.output.resolve())

"""Validate the test-only region harness against native pixels or a full film."""
from pathlib import Path
import hashlib,math,struct
from spectral_components import read,display,LABELS
from optical_comparison_artifacts import bmp_rgba

def verify_regions(report,image,regions):
 meta=report['linear_components'];path=Path(image).parent/meta['file']
 assert meta['written'] and path.stat().st_size==meta['bytes']
 assert hashlib.sha256(path.read_bytes()).hexdigest()==meta['sha256']
 w,h,data=read(path);assert (w,h)==(8,8*len(regions))
 width,height,rgba=bmp_rgba(Path(image));assert (width,height)==(256,192)
 count=0;sums=[[0.]*3 for _ in LABELS];residual=0
 for x,y,rw,rh in regions.values():
  for row in range(y,y+rh):
   for col in range(x,x+rw):
    d=data[count*33:(count+1)*33];count+=1
    for c in range(3):
     total=((d[3+c]+d[6+c])+d[9+c])+(d[12+c]+d[15+c])
     assert struct.pack('<d',total)==struct.pack('<d',d[c])
     error=abs(sum(d[k*3+c] for k in range(7,11))+d[18+c]-d[9+c]);residual=max(residual,error)
     assert error<3e-12*max(1,abs(d[9+c]))
    expected=display(d[:3],report['quality']['exposure'],report['quality']['display_transform']=='spectral_soft_v1')
    offset=(row*width+col)*4;assert expected==rgba[offset:offset+4]
    for k in range(11):
     for c in range(3):sums[k][c]+=d[k*3+c]
 ledger=report['film_components_xyz'];assert ledger['committed_pixels']==count
 for k in range(1,7):
  key='volume_caustic_subset' if k==6 else LABELS[k]
  if key in ledger:assert all(math.isclose(a,b,rel_tol=1e-12,abs_tol=1e-12) for a,b in zip(ledger[key],sums[k]))
 for k,g in enumerate(report['scattering_orders']['groups']):
  assert all(math.isclose(a,b,rel_tol=1e-12,abs_tol=1e-12) for a,b in zip(g['xyz'],sums[k+7]))
 return dict(capture_mode='test_harness_exact_full_sensor_regions',exact_recombined_pixels=count,
             exact_display_pixels=count,max_volume_subset_absolute_residual=residual,component_xyz=sums)

def compare_full(compact,full,regions):
 w,h,a=read(compact);fw,fh,b=read(full);assert (fw,fh)==(256,192)
 expected=[]
 for x,y,rw,rh in regions.values():
  for row in range(y,y+rh):expected.extend(b[(row*fw+x)*33:(row*fw+x+rw)*33])
 assert list(a)==expected
 return dict(exact_values=len(expected),exact_pixels=len(expected)//33,full_film=[fw,fh],packed_film=[w,h])

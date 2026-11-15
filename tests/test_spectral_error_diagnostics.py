"""Independent geometry/reference sanity checks for the bounded spectral audit."""
import unittest,sys
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import spectral_mixed_reference as f
class References(unittest.TestCase):
 def test_chords(self):
  self.assertAlmostEqual(float(f.chord([-2,0,0],[2,0,0],[-1]*3,[1]*3)),2)
  self.assertEqual(float(f.chord([-2,3,0],[2,3,0],[-1]*3,[1]*3)),0)
  self.assertAlmostEqual(float(f.chord([0,0,0],[2,2,2],[-1]*3,[1]*3)),3**.5)
 def test_box_closed_outward(self):
  m=f.box_mesh((.65,-.5,-.5),(.67,1.2,.5));v=np.array([list(p.values()) for p in m['mesh']['vertices']]);center=v.mean(axis=0);edges={}
  for t in m['mesh']['triangles']:
   a,b,c=[t[k] for k in ('a','b','c')];self.assertGreater(np.cross(v[b]-v[a],v[c]-v[a])@(v[[a,b,c]].mean(axis=0)-center),0)
   for i,j in [(a,b),(b,c),(c,a)]:edges[(i,j)]=edges.get((i,j),0)+1
  self.assertTrue(all(edges.get((j,i))==n==1 for (i,j),n in edges.items()))
 def test_quadrature_refines(self):
  for name,fn in f.REFERENCES.items():
   a=fn(550,n=16,k=6);b=fn(550,n=32,k=12)
   self.assertLess(abs(a/b-1),.01,name)
class Statistics(unittest.TestCase):
 def test_additive_crossed_variance(self):
  from spectral_error_analysis import variance_budget
  m=np.array([-3.,-1,1,3]);c=np.array([-6.,-2,2,6]);v=variance_budget(m[:,None]+c[None,:])
  self.assertAlmostEqual(float(v[0]),float(m.var(ddof=1)))
  self.assertAlmostEqual(float(v[1]),float(c.var(ddof=1)))
  self.assertAlmostEqual(float(v[2]),0)
 def test_covariance_retains_cancellation(self):
  from spectral_error_analysis import covariance_summary
  a=np.zeros((4,1,11,3));q=np.array([-3.,-1,1,3])
  a[:,0,1,1]=10+q;a[:,0,2,1]=10-.8*q;a[:,0,0,1]=20+.2*q
  result=covariance_summary(a)
  self.assertAlmostEqual(result['total_Y_variance'],float((.2*q).var(ddof=1)))
  self.assertLess(result['cross_covariance'],0)
class CameraAcceptance(unittest.TestCase):
 def test_rejects_biased_low_variance_candidate(self):
  from spectral_camera_analysis import compare
  a=np.zeros((4,2,11,3));q=np.array([-3.,-1,1,3])[:,None]
  a[:,:,0,1]=a[:,:,1,1]=100+q
  b=a.copy();b[:,:,0,1]=b[:,:,1,1]=120+.5*q
  result=compare(a,b,'beam')
  self.assertAlmostEqual(result['Y_variance_ratio'],.25)
  self.assertTrue(result['variance_pass']);self.assertFalse(result['mean_pass'])
 def test_keeps_mandatory_beam_gate(self):
  from spectral_camera_analysis import compare
  a=np.zeros((4,2,11,3));q=np.array([-3.,-1,1,3])[:,None]
  a[:,:,0,1]=a[:,:,1,1]=100+q
  b=a.copy();b[:,:,0,1]=b[:,:,1,1]=100+1.04*q
  self.assertFalse(compare(a,b,'beam')['variance_pass'])
  self.assertTrue(compare(a,b,'haze')['variance_pass'])
if __name__=='__main__':unittest.main()

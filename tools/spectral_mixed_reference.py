"""Independent mixed-path references and ordinary V3 fixtures.
Only geometry/observer data are shared; quadrature never calls renderer transport.
"""
import copy,json,math,subprocess,sys,time,shutil,hashlib
from pathlib import Path
import numpy as np
from spectral_room_testbench import plane,material,scene_root,v,REPO
from spectral_response_fixture import cie
from spectral_components import verify,read

SEEDS=(19,104729,1237,65537,16807,7919,373,99991,524287,8191,65521,32749,1009,42043,131071,31)
WAVES=(450,550,650)
RHO={450:.05,550:.5,650:.8}
def curve(value):return [[380,value],[780,value]]
def medium(lo,hi,sa,ss,cell=.04):
 return {'bounds_min_m':lo,'bounds_max_m':hi,'sigma_a_per_m':curve(sa),'sigma_s_per_m':curve(ss),'g':0,'caustic_cell_m':cell}
def profile(scene):return scene['extensions']['ray_tracing']['spectral_transport_v1']['surface_path_v1']
def request(at,look):
 return {'schema_version':'ray_tracing_agent_render_request_v2','render':{'integrator_3d':'disney_v3_spectral','width':1,'height':1},
 'inspection':{'camera_position':v(*at),'camera_look_at':v(*look),'camera_zoom':1000}}
def attach(scene,p):scene['extensions']={'ray_tracing':{'spectral_transport_v1':{'quality_profile':'surface_path_v1','exposure':1000,'surface_path_v1':p}}}
def box_mesh(lo,hi):
 vertices=[v(x,y,z) for x in (lo[0],hi[0]) for y in (lo[1],hi[1]) for z in (lo[2],hi[2])]
 faces=[(0,1,3,2),(4,6,7,5),(0,4,5,1),(2,3,7,6),(0,2,6,4),(1,5,7,3)]
 tri=[]
 for a,b,c,d in faces:
  tri.extend([dict(a=a,b=b,c=c,surface_group_id='slab'),dict(a=a,b=c,c=d,surface_group_id='slab')])
 return dict(schema_family='codework_geometry',schema_variant='mesh_asset_runtime_v1',schema_version=1,asset_id='audit_slab',source_asset_id='analytic_closed_slab',asset_type='solid_mesh',compile_meta={'profile':'runtime_default'},local_bounds={'min':v(*lo),'max':v(*hi)},mesh=dict(vertex_count=8,triangle_count=12,vertices=vertices,triangles=tri),surface_groups=[dict(group_id='slab',semantic='slab',triangle_span={'start':0,'count':12})],topology_flags={'closed_volume':True,'manifold_expected':True},extensions={})
def glass_object():
 return {'object_id':'glass','object_type':'mesh_asset_instance','dimensional_mode':'full_3d','space_mode_intent':'3d','transform':{'position':v(0,0,0),'rotation':v(0,0,0),'scale':v(1,1,1)},'geometry_ref':{'kind':'mesh_asset','id':'audit_slab','variant':'runtime_default'},'extensions':{'line_drawing':{'runtime_mesh_path':'slab.runtime.json'}},'flags':{'visible':True,'locked':False,'selectable':True}}
def glass_material():return dict(object_id='glass',model='smooth_glass',glass='constant',ior=1.0001)
def caustics(seed,radius=.03):return dict(passes=16,photons_per_pass=16384,seed=seed,bins=32,photon_launch='glass_guided',camera_fresnel='split_first',radius_m=radius)
def fixture(kind,nm,seed=19,black=False,scale=1,cell=.04,radius=.03):
 rho=0 if black else RHO[nm];mesh=None
 if kind=='surface_fog':
  scene=scene_root([plane('panel',(0,0,0),(1,0,0),(0,1,0),(0,0,1),.6,.6)])
  p=dict(samples=8192,max_depth=1,seed=seed,materials=[material('panel',rho)],
    light=dict(type='area',origin_m=[.6,0,1],axis_u=[1,0,0],axis_v=[0,-1,0],width_m=.4,height_m=.4,spectrum={'lines':[[nm,scale]]}),
    medium=medium([-.2,-.2,.3],[.2,.2,.5],{450:.05,550:.15,650:.3}[nm],.4))
  p['medium'].pop('caustic_cell_m');q=request((-.8,0,.4),(0,0,.4))
 elif kind=='glass_surface':
  scene=scene_root([plane('receiver',(0,0,0),(0,1,0),(0,0,1),(1,0,0),.8,.8),plane('panel',(1,0,0),(0,1,0),(0,0,-1),(-1,0,0),.6,.6),glass_object()])
  mesh=box_mesh((.65,-.5,-.5),(.67,1.2,.5))
  p=dict(samples=8192,max_depth=4,seed=seed,materials=[material('receiver',.8),material('panel',rho),glass_material()],caustics=caustics(seed,radius),
   light=dict(type='area',origin_m=[.4,1,0],axis_u=[0,1,0],axis_v=[0,0,1],width_m=.12,height_m=.12,spectrum={'lines':[[nm,scale]]}))
  q=request((.35,0,0),(0,0,0))
 elif kind=='glass_fog':
  scene=scene_root([plane('black',(0,0,0),(1,0,0),(0,1,0),(0,0,1),.03,.03),glass_object()])
  mesh=box_mesh((-.25,-.25,.4),(.25,.25,.42))
  p=dict(samples=2048,max_depth=6,seed=seed,materials=[material('black',0),glass_material()],caustics=caustics(seed,radius),
    light=dict(type='area',origin_m=[0,0,.88],axis_u=[1,0,0],axis_v=[0,-1,0],width_m=.06,height_m=.06,spectrum={'lines':[[nm,scale]]}),
    medium=medium([-.12,-.12,.02],[.12,.12,.10],{450:.05,550:.15,650:.3}[nm],.025,cell))
  q=request((0,-.8,.06),(0,0,.06))
 else:raise ValueError(kind)
 p['probes']=[{'name':'target','x':0,'y':0}];attach(scene,p);return q,scene,mesh

def chord(a,b,lo,hi):
 """Euclidean segment length inside an axis-aligned box, vectorized independently."""
 a=np.asarray(a);b=np.asarray(b);d=b-a
 low=np.zeros(d.shape[:-1]);high=np.ones(d.shape[:-1]);valid=np.ones(d.shape[:-1],dtype=bool)
 for k in range(3):
  active=np.abs(d[...,k])>1e-14
  den=np.where(active,d[...,k],1.)
  t0=(lo[k]-a[...,k])/den;t1=(hi[k]-a[...,k])/den
  low=np.maximum(low,np.where(active,np.minimum(t0,t1),-np.inf));high=np.minimum(high,np.where(active,np.maximum(t0,t1),np.inf))
  valid &= active | ((a[...,k]>=lo[k]) & (a[...,k]<=hi[k]))
 return np.maximum(0,high-low)*np.linalg.norm(d,axis=-1)*valid

def surface_fog_reference(nm,n=40,k=12,steps=80):
 axis=(np.arange(n)+.5)*.6/n-.3
 x,y=np.meshgrid(axis,axis,indexing='ij');panel=np.stack((x.ravel(),y.ravel(),np.zeros(n*n)),axis=-1)
 la=(np.arange(k)+.5)*.4/k-.2;lx,ly=np.meshgrid(la+.6,la,indexing='ij');light=np.stack((lx.ravel(),ly.ravel(),np.ones(k*k)),axis=-1)
 lo=(-.2,-.2,.3);hi=(.2,.2,.5);st={450:.05,550:.15,650:.3}[nm]+.4
 # Unit emitter radiance, panel irradiance, both parallel normals.
 d=light[None,:,:]-panel[:,None,:];r2=np.sum(d*d,axis=-1)
 E=np.sum(np.exp(-st*chord(panel[:,None,:],light[None,:,:],lo,hi))/r2**2,axis=1)*(.4/k)**2
 s=(np.arange(steps)+.5)*.4/steps-.2;points=np.stack((s,np.zeros(steps),np.full(steps,.4)),axis=-1)
 d=panel[None,:,:]-points[:,None,:];r=np.linalg.norm(d,axis=-1)
 L=(E[None,:]*.4/r**3*np.exp(-st*chord(points[:,None,:],panel[None,:,:],lo,hi))).sum(axis=1)*(.6/n)**2*RHO[nm]/math.pi
 return float(np.sum(np.exp(-st*(s+.2))*.4/(4*math.pi)*L)*.4/steps)

def glass_surface_reference(nm,n=80,k=16):
 axis=(np.arange(n)+.5)*.6/n-.3;y,z=np.meshgrid(axis,axis,indexing='ij');y=y.ravel();z=z.ravel()
 light=(np.arange(k)+.5)*.12/k-.06;ly,lz=np.meshgrid(light+1,light,indexing='ij')
 r2=.6**2+(y[:,None]-ly.ravel())**2+(z[:,None]-lz.ravel())**2
 E=np.sum(.6**2/r2**2,axis=1)*(.12/k)**2
 return float(np.sum(E/(math.pi**2*(1+y*y+z*z)**2))*(.6/n)**2*.8*RHO[nm])

def glass_fog_reference(nm,n=160,k=24):
 y=(np.arange(n)+.5)*.24/n-.12;u=(np.arange(k)+.5)*.06/k-.03
 x,z=np.meshgrid(u,u,indexing='ij');x=x.ravel();z=z.ravel()
 r=np.sqrt(.82**2+x[None,:]**2+(z[None,:]-y[:,None])**2)
 st={450:.05,550:.15,650:.3}[nm]+.025
 tr=np.exp(-st*(y[:,None]+.12+.04*r/.82))
 return float(np.sum(tr*.025/(4*math.pi)*.82/r**3)*.24/n*(.06/k)**2)
REFERENCES={'surface_fog':surface_fog_reference,'glass_surface':glass_surface_reference,'glass_fog':glass_fog_reference}

def run(cli,root,name,q,scene,mesh):
 case=root/name
 if (case/'result.json').exists():
  old=json.loads((case/'result.json').read_text())
  assert json.loads((case/'scene.json').read_text())==scene, 'saved scene identity changed'
  oldq=json.loads((case/'request.json').read_text());newq=copy.deepcopy(q)
  newq.update(scene={'runtime_scene_path':'scene.json'},output={'root':'render','overwrite':False},run_id=name)
  assert oldq==newq, 'saved request identity changed'
  if mesh:assert json.loads((case/'slab.runtime.json').read_text())==mesh
  binary=hashlib.sha256(Path(cli).read_bytes()).hexdigest()
  assert old.get('binary_sha256',binary)==binary, 'saved binary identity changed'
  if 'binary_sha256' not in old:
   old['binary_sha256']=binary;old['identity_verified_after_pilot']=True
   (case/'result.json').write_text(json.dumps(old,indent=2))
  return old
 if case.exists():raise RuntimeError('inspect incomplete identity '+str(case))
 case.mkdir(parents=True);(case/'config').symlink_to(REPO/'config',target_is_directory=True)
 if mesh:(case/'slab.runtime.json').write_text(json.dumps(mesh))
 q=copy.deepcopy(q);q['scene']={'runtime_scene_path':'scene.json'};q['output']={'root':'render','overwrite':False};q['run_id']=name
 for file,value in [('scene.json',scene),('request.json',q)]:(case/file).write_text(json.dumps(value,indent=2))
 start=time.monotonic()
 with (case/'render.log').open('w') as log:
  p=subprocess.run([str(cli),'--request','request.json','--render','--summary','summary.json','--summary-file-only'],cwd=case,stdout=log,stderr=subprocess.STDOUT,timeout=180)
 if p.returncode:raise RuntimeError(f'{name} failed: '+(case/'summary.json').read_text()[-2400:])
 report=json.loads((case/'summary.json').read_text())['spectral_transport'];verified=verify(report,case/'render/frames/frame_0000.bmp')
 _,_,data=read(case/'render/frames/frame_0000.bmp.components.npy')
 out={'binary_sha256':hashlib.sha256(Path(cli).read_bytes()).hexdigest(),'seconds':time.monotonic()-start,'components':np.array(data).reshape(11,3).tolist(),'report':report,'verified':verified}
 (case/'result.json').write_text(json.dumps(out,indent=2));return out

def projected(xyz,nm):
 color=np.array(cie(nm));xyz=np.array(xyz);scalar=float(xyz@color/(color@color))
 assert np.max(np.abs(xyz-scalar*color))<1e-10*max(1,float(np.max(np.abs(xyz))))
 return scalar

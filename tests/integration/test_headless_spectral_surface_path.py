#!/usr/bin/env python3
"""Native opaque surface/medium transport against independent integral and limits."""
import sys, argparse, tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
import spectral_room_testbench as bench
from spectral_room_testbench import *
def oracle(n=80,k=16):
    # Independent 4D area integration for source -> colored panel -> receiver.
    total=0.
    for j in range(n):
      y=-.3+.6*(j+.5)/n
      for i in range(n):
        z=-.3+.6*(i+.5)/n
        incoming=0.
        for vj in range(k):
          sy=1+.12*((vj+.5)/k-.5)
          for vi in range(k):
            sz=.12*((vi+.5)/k-.5)
            r2=.6**2+(y-sy)**2+(z-sz)**2
            incoming+=.6**2/r2**2
        incoming*=.12**2/k**2
        r2=1+y*y+z*z
        total+=incoming/(math.pi**2*r2**2)
    return total*.6**2/n**2

def setup():
    scene=scene_root([plane('receiver',(0,0,0),(0,1,0),(0,0,1),(1,0,0),.8,.8),
                      plane('panel',(1,0,0),(0,1,0),(0,0,-1),(-1,0,0),.6,.6)])
    path={'samples':8192,'max_depth':2,'seed':19,
          'materials':[material('receiver',.8),
                       material('panel',[[380,.02],[450,.02],[610,.8],[780,.8]])],
          'light':{'type':'area','origin_m':[.4,1,0],'axis_u':[0,1,0],'axis_v':[0,0,1],
             'width_m':.12,'height_m':.12,'spectrum':{'lines':[[450,1],[610,1]]}},
          'probes':[{'name':'target','x':0,'y':0}]}
    q={'schema_version':'ray_tracing_agent_render_request_v2',
       'render':{'integrator_3d':'disney_v3_spectral','width':1,'height':1},
       'inspection':{'camera_position':v(.35,0,0),'camera_look_at':v(0,0,0),'camera_zoom':1000},
       'spectral':{'exposure':1000,'surface_path_v1':path}}
    return q,scene
import re
CIE_ROWS=[tuple(map(float,r.split(','))) for r in re.findall(r'\{([^{}]+)\}',
       (REPO/'src/render/cie_1931_2deg_380_780.inc').read_text())]
def cie(nm):
    rows=CIE_ROWS
    u=(1000/nm)**2
    at=nm/(1+1e-8*(8060.51+2480990/(132.274-u)+17455.7/(39.32957-u)))-379
    i=int(at)
    return [a*(i+1-at)+b*(at-i) for a,b in zip(rows[i],rows[i+1])]
def reference_checks():
    q,sc=setup()
    factor=oracle()
    coarse=oracle(40,8)
    assert abs(coarse/factor-1)<.001
    expected=[factor*.8*(.02*a+.8*b) for a,b in zip(cie(450),cie(610))]
    values=[]
    for seed in (19,104729,1237,65537):
        q['spectral']['surface_path_v1']['seed']=seed
        d=render('oracle-'+str(seed),q,sc,mesh=False)
        p=d['spectral_surface_path']['probes'][0]
        assert p['direct_xyz']==[0,0,0] and p['volume_xyz']==[0,0,0]
        values.append(p['total_xyz'])
    means=[sum(v[i] for v in values)/len(values) for i in range(3)]
    assert max(abs(a/b-1) for a,b in zip(means,expected))<.1,(means,expected)
    (ROOT/'panel-reference.json').write_text(json.dumps(dict(native_xyz=means,reference_xyz=expected,
        geometric_factor=factor,quadrature_refinement=abs(coarse/factor-1)),indent=2))
    print('Independent bounce reference',means,expected,'factor',factor,flush=True)
    q['spectral']['surface_path_v1']['materials'][1]['reflectance']=[[380,0],[780,0]]
    black=render('oracle-black',q,sc,mesh=False)
    assert black['spectral_surface_path']['probes'][0]['total_xyz']==[0,0,0]

def probe(result):
    return result['spectral_surface_path']['probes'][0]
def make_probe(q):
    q['render'].update(width=1,height=1)
    q['spectral']['surface_path_v1']['probes']=[{'name':'target','x':0,'y':0}]

def transport_checks():
    q,sc=setup()
    path=q['spectral']['surface_path_v1']
    path['samples']=8192
    path['max_depth']=2
    path['light']['spectrum']={'lines':[[610,1]]}
    red=probe(render('check-red',q,sc,mesh=False))
    path['light']['spectrum']={'lines':[[450,1]]}
    blue=probe(render('check-blue',q,sc,mesh=False))
    ratio=(red['total_xyz'][1]/cie(610)[1])/(blue['total_xyz'][1]/cie(450)[1])
    assert abs(ratio/40-1)<1e-10,ratio
    path['light']['spectrum']={'lines':[[610,2]]}
    double=probe(render('check-double',q,sc,mesh=False))
    assert all(math.isclose(a,2*b,rel_tol=1e-12) for a,b in zip(double['total_xyz'],red['total_xyz']))

    # Pure absorption on a directly visible source is analytically deterministic.
    q,sc=setup()
    sc['objects']=sc['objects'][:1]
    path=q['spectral']['surface_path_v1']
    path['materials']=path['materials'][:1]
    path['samples']=2048
    path['max_depth']=1
    path['light'].update(origin_m=[1,0,0],axis_u=[0,1,0],axis_v=[0,0,-1],width_m=2,height_m=2,
                         spectrum={'lines':[[530,1]]})
    q['inspection'].update(camera_position=v(.1,0,0),camera_look_at=v(1,0,0),camera_zoom=1000)
    path['medium']={'bounds_min_m':[.3,-.5,-.5],'bounds_max_m':[.7,.5,.5],
                    'sigma_a_per_m':[[380,2],[780,2]],'sigma_s_per_m':[[380,0],[780,0]],'g':0}
    absorption=probe(render('check-absorption',q,sc,mesh=False))
    expected=math.exp(-.8)
    for a,b in zip(absorption['total_xyz'],cie(530)):
        assert abs(a/b-expected)<1e-7,(a,b,expected)

    # A finite scattering box under uniform all-sphere radiance has a constant
    # radiance solution. Multiple scattering must preserve it.
    path['light']={'type':'sky_cap','axis':[1,0,0],'half_angle_deg':180,'spectrum':{'lines':[[530,1]]}}
    # Put the required opaque primitive far outside the medium and visible sky.
    sc['objects']=[plane('receiver',(-100,0,0),(0,1,0),(0,0,1),(1,0,0),.001,.001)]
    path['medium'].update(sigma_a_per_m=[[380,0],[780,0]],sigma_s_per_m=[[380,2],[780,2]],g=.4)
    path['max_depth']=4
    path['samples']=8192
    equilibrium=probe(render('check-scatter-equilibrium',q,sc,mesh=False))
    rel=[a/b for a,b in zip(equilibrium['total_xyz'],cie(530))]
    assert max(abs(a-1) for a in rel)<.03,rel
    assert equilibrium['volume_xyz'][1]>0

    # The light faces toward the colored panel. It cannot directly illuminate
    # the fog on its rear side. A volume signal here requires a surface bounce.
    q,sc=setup()
    path=q['spectral']['surface_path_v1']
    sc['objects'][0]=plane('receiver',(-.5,0,0),(0,1,0),(0,0,1),(1,0,0),.8,.8)
    path['materials'][0]['reflectance']=[[380,0],[780,0]]
    path['light']['spectrum']={'lines':[[610,1]]}
    path['medium']={'bounds_min_m':[.05,-.2,-.2],'bounds_max_m':[.3,.2,.2],
                    'sigma_a_per_m':[[380,0],[780,0]],'sigma_s_per_m':[[380,.6],[780,.6]],'g':0}
    q['inspection'].update(camera_position=v(-.1,0,0),camera_look_at=v(1,0,0),camera_zoom=1000)
    path['samples']=8192
    fog_red=probe(render('check-surface-fog',q,sc,mesh=False))
    assert fog_red['volume_xyz'][1]>0,fog_red
    path['materials'][1]['reflectance']=[[380,0],[780,0]]
    fog_black=probe(render('check-surface-fog-black',q,sc,mesh=False))
    assert fog_black['volume_xyz']==[0,0,0],fog_black
    # Forward light -> fog -> matte receiver; direct source illumination is zero.
    q,sc=setup();path=q['spectral']['surface_path_v1']
    sc['objects']=sc['objects'][:1];path['materials']=path['materials'][:1]
    path['light'].update(origin_m=[.4,0,0],axis_u=[0,1,0],axis_v=[0,0,1],
        width_m=.12,height_m=.12,spectrum={'lines':[[530,1]]})
    path['medium']={'bounds_min_m':[.6,-.3,-.3],'bounds_max_m':[.9,.3,.3],
        'sigma_a_per_m':[[380,0],[780,0]],'sigma_s_per_m':[[380,2],[780,2]],'g':0}
    fog_surface=probe(render('fog-to-surface',q,sc,mesh=False))
    assert fog_surface['direct_xyz']==[0,0,0] and fog_surface['surface_indirect_xyz']==[0,0,0]
    assert fog_surface['volume_xyz'][1]>0
    path.pop('medium')
    vacuum=probe(render('fog-to-surface-vacuum',q,sc,mesh=False))
    assert vacuum['total_xyz']==[0,0,0]
    results={'line_ratio':ratio,'absorption_ratio':absorption['total_xyz'][1]/cie(530)[1],
             'scattering_equilibrium':rel,'colored_fog_Y':fog_red['volume_xyz'][1], 'fog_to_surface_Y':fog_surface['volume_xyz'][1]}
    (ROOT/'checks.json').write_text(json.dumps(results,indent=2))
    print('PASS spectrum, scaling, absorption, scattering and colored-fog controls',results,flush=True)


def angular_checks():
    # Independent hemisphere quadrature of the declared single-scattering GGX
    # BRDF. The native render samples light and BSDF with MIS, not this grid.
    def reference(rho,roughness,vx,vz):
        ntheta,nphi=900,192
        alpha=max(roughness**2,.001);a2=alpha**2
        def g1(c):return 2/(1+math.sqrt(1+a2*(1-c*c)/(c*c)))
        total=0
        for i in range(ntheta):
            theta=(i+.5)*math.pi/(2*ntheta);z=math.cos(theta);r=math.sin(theta)
            for j in range(nphi):
                phi=(j+.5)*2*math.pi/nphi
                x=r*math.cos(phi);y=r*math.sin(phi)
                length=math.sqrt((x+vx)**2+y*y+(z+vz)**2)
                hz=(z+vz)/length;vh=(vx*(x+vx)+vz*(z+vz))/length
                d=a2/(math.pi*(hz*hz*(a2-1)+1)**2)
                f=rho+(1-rho)*(1-vh)**5
                total+=f*d*g1(vz)*g1(z)/(4*vz)*r
        return total*(math.pi/(2*ntheta))*(2*math.pi/nphi)
    results=[]
    for roughness in (.2,.5,1):
      for angle in (0,.75):
        for rho in (.04,.75):
            q,sc=setup();sc['objects']=sc['objects'][:1]
            p=q['spectral']['surface_path_v1']
            p['materials']=[material('receiver',rho,'ggx',roughness)]
            p['light']={'type':'sky_cap','axis':[1,0,0],'half_angle_deg':180,'spectrum':{'lines':[[530,1]]}}
            vx=math.sin(angle);vz=math.cos(angle)
            q['inspection'].update(camera_position=v(.35*vz,.35*vx,0),camera_look_at=v(0,0,0))
            result=probe(render('ggx-%s-%s-%s'%(roughness,angle,rho),q,sc,mesh=False))
            native=result['total_xyz'][1]/cie(530)[1]
            expected=reference(rho,roughness,vx,vz)
            assert abs(native/expected-1)<.06,(roughness,angle,rho,native,expected)
            assert 0<=native<=1.02
            results.append(dict(roughness=roughness,view_angle=angle,f0=rho,native=native,reference=expected))
    (ROOT/'angular.json').write_text(json.dumps(results,indent=2))
    print('PASS GGX hemisphere reference, 12 roughness/view/F0 cases',flush=True)

def sky_response_checks():
    results=[]
    rho=[[380,.03],[450,.03],[500,.08],[550,.35],[600,.78],[780,.8]]
    def curve(points,nm):
        for (a,x),(b,y) in zip(points,points[1:]):
            if a<=nm<=b:return x+(y-x)*(nm-a)/(b-a)
        raise AssertionError(nm)
    for label in ('flat','warm','blue'):
        knots=[]
        for nm in range(380,781,20):
            if label=='warm':
                m=nm*1e-9;value=1/(m**5*math.expm1(.01438776877/(m*2800)))
            elif label=='blue':
                value=math.exp(-.5*((nm-455)/22)**2)
                # Declared finite support; retain the source sampler's guard
                # against CDF increments below floating-point resolution.
                if value<1e-10:value=0
            else:value=1
            knots.append([nm,value])
        area=sum((b-a)*(x+y)/2 for (a,x),(b,y) in zip(knots,knots[1:]))
        for knot in knots:knot[1]/=area
        expected=[0.,0.,0.]
        for i in range(8000):
            nm=380+(i+.5)*.05
            for j,c in enumerate(cie(nm)):
                expected[j]+=.05*curve(knots,nm)*curve(rho,nm)*c
        q,sc=setup();sc['objects']=sc['objects'][:1]
        path=q['spectral']['surface_path_v1'];path['materials']=[material('receiver',rho)]
        path['light']={'type':'sky_cap','axis':[1,0,0],'half_angle_deg':180,'spectrum':{'continuum':knots}}
        result=probe(render('sky-response-'+label,q,sc,mesh=False))
        actual=result['total_xyz']
        assert max(abs(a/b-1) for a,b in zip(actual,expected))<.03,(label,actual,expected)
        results.append(dict(source=label,native_xyz=actual,reference_xyz=expected))
    (ROOT/'sky-response.json').write_text(json.dumps(results,indent=2))
    print('PASS broadband sky x selective material spectral response',flush=True)

def defaults_checks():
    q,sc=setup()
    p=q['spectral']['surface_path_v1']
    for field in ('samples','max_depth','seed'): p.pop(field,None)
    q['spectral'].pop('exposure',None)
    implicit=render('defaults-implicit',q,sc,mesh=False)
    p.update(samples=512,max_depth=12,seed=19)
    q['spectral'].update(exposure=1,quality_profile='surface_path_v1',display_transform='spectral_soft_v1')
    explicit=render('defaults-explicit',q,sc,mesh=False)
    assert implicit['spectral_quality']==explicit['spectral_quality']
    assert implicit['spectral_surface_path']==explicit['spectral_surface_path']
    q['spectral']['display_transform']='legacy_clip'
    legacy=render('defaults-display-optout',q,sc,mesh=False)
    assert legacy['spectral_surface_path']==explicit['spectral_surface_path']
    p['output_component']='surface_indirect'
    isolated=render('component-indirect',q,sc,mesh=False)
    assert isolated['spectral_surface_path']==legacy['spectral_surface_path']
    p['max_depth']=1
    direct=render('depth-optout',q,sc,mesh=False)
    assert probe(direct)['total_xyz']==[0,0,0]
    zero=copy.deepcopy(q)
    zero['spectral']['surface_path_v1']['light']['spectrum']={'lines':[[530,0]]}
    dark=render('zero-source',zero,sc,mesh=False)
    assert probe(dark)['total_xyz']==[0,0,0] and dark['spectral_surface_path']['paths']==0
    # Reject typos and wrong types rather than silently choosing a default.
    for i,(field,value) in enumerate((('samples',True),('max_depth',13),('seed',-1))):
        invalid=copy.deepcopy(q);invalid['spectral']['surface_path_v1'][field]=value
        d=ROOT/('invalid-'+str(i));d.mkdir();(d/'config').symlink_to(REPO/'config',target_is_directory=True)
        (d/'scene.json').write_text(json.dumps(sc));invalid['scene']={'runtime_scene_path':'scene.json'}
        invalid['output']={'root':'output','overwrite':False};(d/'request.json').write_text(json.dumps(invalid))
        run=subprocess.run([CLI,'--request','request.json','--render','--summary','summary.json',
                            '--summary-file-only'],cwd=d,capture_output=True,timeout=30)
        assert run.returncode!=0,(field,run.stdout,run.stderr)
    for i,value in enumerate((None,'unrecognized_v9')):
        invalid=copy.deepcopy(q);invalid['spectral']['quality_profile']=value
        d=ROOT/('invalid-profile-'+str(i));d.mkdir();(d/'config').symlink_to(REPO/'config',target_is_directory=True)
        (d/'scene.json').write_text(json.dumps(sc));invalid['scene']={'runtime_scene_path':'scene.json'}
        invalid['output']={'root':'output','overwrite':False};(d/'request.json').write_text(json.dumps(invalid))
        result=subprocess.run([CLI,'--request','request.json','--render','--summary','summary.json',
                               '--summary-file-only'],cwd=d,capture_output=True,timeout=30)
        assert result.returncode!=0
    print('PASS pinned defaults, display and depth opt-outs, component identity and invalid fields')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--cli',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='optic-surface-path-') as scratch:
        ROOT=(args.output or Path(scratch)).resolve();ROOT.mkdir(parents=True,exist_ok=True)
        CLI=args.cli.resolve();bench.ROOT=ROOT;bench.CLI=CLI
        reference_checks();transport_checks();angular_checks();sky_response_checks();defaults_checks()
    print('PASS native spectral surface path qualification')

#!/usr/bin/env python3
"""Numerical S5 checks through the public headless entry point."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[2]
FIX = REPO/"tests/fixtures"


def constant(value):
    return [[380,value],[780,value]]


def near(actual, expected, tolerance):
    assert abs(actual-expected) <= tolerance, (actual, expected, tolerance)


def slab_reference(tau, albedo):
    """Discrete ordinates, independent of the renderer's forward photon estimator.
    Iterate the isotropic source function of an open, laterally infinite slab
    with unit incident collimated flux. Integrate the normal outgoing radiance.
    """
    cells, directions = 100, 40
    dt = tau/cells
    direct = [math.exp(-dt*(i+.5)) for i in range(cells)]
    q = [albedo*d/(4*math.pi) for d in direct]
    for _ in range(80):
        intensity = [0.0]*cells
        for m in range(directions):
            mu=(m+.5)/directions
            half=math.exp(-.5*dt/mu)
            for indices in [range(cells),reversed(range(cells))]:
                incoming=0.0
                for i in indices:
                    intensity[i]+=(incoming*half+q[i]*(1-half))*2*math.pi/directions
                    incoming=incoming*half**2+q[i]*(1-half**2)
        new=[albedo*(d+s)/(4*math.pi) for d,s in zip(direct,intensity)]
        err=max(abs(a-b) for a,b in zip(q,new))
        q=new
        if err<1e-10: break
    outgoing=0.0
    for s in q: outgoing=outgoing*math.exp(-dt)+s*(-math.expm1(-dt))
    return outgoing, 4*math.pi*(1-albedo)*dt*sum(q)/albedo


def qualify(cli,root):
    (root/"config").symlink_to(REPO/"config",target_is_directory=True)
    (root/"data/runtime/videos").mkdir(parents=True)
    q=json.loads((FIX/"spectral_volume/request.json").read_text())
    q["scene"]["runtime_scene_path"]=str(FIX/"spectral_prism/scene_runtime.json")
    q["render"].update(width=1,height=1)
    q["output"].update(root=str(root/"render"),overwrite=True)
    s=q["spectral"]; s.update(glass_id="prism",glass="schott_n_bk7"); mapping=s["surface_map"]; lamp=mapping["sources"][0]
    v=s["participating_volume_v1"]
    def camera(x,dx,y=.36,z=0):
        q["inspection"].update(camera_position=dict(x=x,y=y,z=z),camera_look_at=dict(x=x+dx,y=y,z=z))
    def run(render=True,valid=True):
        path=root/"request.json"; summary=root/"summary.json"
        path.write_text(json.dumps(q))
        proc=subprocess.run([cli,"--request",path,"--summary",summary,"--summary-file-only",
                             "--render" if render else "--preflight"],cwd=root,capture_output=True)
        assert (proc.returncode==0)==valid,(proc.returncode,proc.stderr[-400:],summary.read_text()[-450:] if summary.exists() else '')
        if not valid:return
        data=json.loads(summary.read_text())
        st=data["spectral_surface_map"]
        assert st["reused"]==render
        if "spectral_volume" in data:
            for r in data["spectral_volume"]["bands"]:
                closed=sum(r[k] for k in ["receiver_w","escaped_w","absorbed_w","depth_limited_w","surface_loss_w"])
                near(closed,r["launched_w"],1e-10)
        return data
    def radiance(d):return [r["center_band_radiance"] for r in d["spectral_surface_map"]["bands"]]
    # Clear control, chromatic Beer absorption on both direct and camera segments.
    camera(.74,1)
    lamp.update(origin_m=[.70,.36,0],aperture_m=.25,lines=[[450,.5],[650,.5]])
    v.update(sigma_a_per_m=constant(0),sigma_s_per_m=constant(0),g=0)
    clear=run()
    del s["participating_volume_v1"]
    assert radiance(run())==radiance(clear)
    s["participating_volume_v1"]=v
    v["sigma_a_per_m"]=[[380,5],[450,5],[650,20],[780,20]]
    absorption=run()
    for i,(r,a) in enumerate(zip(radiance(absorption),[5,20])):
        near(r/radiance(clear)[i],math.exp(-2*.04*a),1e-9)
        stat=absorption["spectral_volume"]["bands"][i]
        near(stat["absorbed_w"],.5*(-math.expm1(-.04*a)),.013)
    print("Absorbing slab blue/red transmission (two crossings):",[a/b for a,b in zip(radiance(absorption),radiance(clear))])
    # Observe only light scattered toward the camera. The off-axis beam source
    # is external to the scene geometry; no emissive backdrop or surface contribution.
    camera(.94,-1);lamp.update(lines=[[550,1]],aperture_m=.25)
    lo=.8;thickness=.02
    v.update(bounds_min_m=[lo,-.2,-.55],bounds_max_m=[lo+thickness,.90,.55],cell_m=.05)
    irradiance=1/(math.pi*.25**2)
    mapping["photons_per_source"]=131072
    v.update(sigma_a_per_m=constant(0),sigma_s_per_m=constant(.08/thickness),g=0)
    thin=run()
    single=.08*math.exp(-.08)/(4*math.pi)
    rr=thin["spectral_volume"]["bands"][0]
    near(rr["first_scatter_w"],1-math.exp(-.08),.003)
    near(rr["center_single_radiance"]/irradiance,single,.15*single)
    near(rr["sum_cosine"]/rr["scatter_events"],0,.018)
    near(radiance(thin)[0]/irradiance,slab_reference(.08,1)[0],.0013)
    # Phase proposal and directional evaluation at positive and negative g.
    for g in [-.55,.55]:
        v["g"]=g
        d=run()
        r=d["spectral_volume"]["bands"][0]
        near(r["sum_cosine"]/r["scatter_events"],g,.018)
        factor=(1+g)/(1-g)**2
        ratio=r["center_single_radiance"]/(irradiance*single*factor)
        near(ratio,1,.15)
    # Two spectral components of the same source have different scattering
    # mean free paths. Measure both collision power and first-order radiance.
    lamp["lines"]=[[450,.5],[650,.5]]
    v.update(sigma_s_per_m=[[380,4],[450,4],[650,20],[780,20]],g=0)
    colored=run()["spectral_volume"]["bands"]
    for row,tau in zip(colored,[.08,.4]):
        near(row["first_scatter_w"],.5*(-math.expm1(-tau)),.004)
        reference=.5*irradiance*tau*math.exp(-tau)/(4*math.pi)
        near(row["center_single_radiance"],reference,.17*reference)
    print("Blue/red first-scattered watts:",[r["first_scatter_w"] for r in colored])
    lamp["lines"]=[[550,1]]
    # Compare increasing budgets to an independent multiple-scattering solution.
    v.update(sigma_a_per_m=constant(10),sigma_s_per_m=constant(40),g=0)
    expected, expected_abs=slab_reference(1,.8)
    errors=[]
    for n in [4096,32768,131072]:
        mapping["photons_per_source"]=n
        estimates=[]
        for seed in [7,31,83]:
            mapping["seed"]=seed
            result=run(); row=result["spectral_volume"]["bands"][0]
            assert row["higher_scatter_w"]>.15 and row["depth_limited_w"]==0
            if n==131072:
                near(row["absorbed_w"],expected_abs,.009)
                near(row["receiver_w"]+row["escaped_w"],1-expected_abs,.009)
            estimates.append(radiance(result)[0]/irradiance)
        errors.append(math.sqrt(sum((e/expected-1)**2 for e in estimates)/len(estimates)))
    assert errors[-1]<.06 and errors[-1]<errors[0],(expected,estimates,errors)
    print("All-orders slab absorbed watts:",row["absorbed_w"],"reference:",expected_abs)
    replay=run()
    assert replay["spectral_volume"]==result["spectral_volume"] and radiance(replay)==radiance(result)
    print("Multiple scattering normal radiance per incident irradiance:",expected,estimates,"relative RMS:",errors)
    # VF3D: independently authored, linear nodal density, native raw and manifest.
    # Solid-masked last node contributes zero over the last whole voxel.
    v.clear();v.update(kind="vf3d_density",source_path="density.json",frame_index=7,
                        sigma_a_per_m=constant(2),sigma_s_per_m=constant(0),g=0,cell_m=.05)
    header=struct.pack("<5I4xdQd7fI3I4x",0x56463344,1,4,2,2,.7,7,.1,.75,.25,-.125,.0625,0,0,1,0,0,0,0)
    den=[float(i%4+1) for i in range(16)]
    mask=bytes([int(i%4==3) for i in range(16)])
    field=root/"frame_000007.vf3d"
    def write_field():
        field.write_bytes(header+struct.pack("<16f",*den)+bytes(16*16)+mask)
    write_field()
    (root/"density.json").write_text(json.dumps(dict(manifest_version=2,frame_contract="vf3d",frames=[dict(frame_index=7,path=field.name,frame_contract="vf3d")])))
    camera(.71,1,.28,-.1)
    lamp.update(origin_m=[.70,.28,-.1],lines=[[550,1]],aperture_m=.008)
    mapping["photons_per_source"]=16384
    raw=run()
    # Integral of 1 -> 2 -> 3 -> 0 followed by one zero-valued voxel.
    incident=1/(math.pi*.008)**2
    near(radiance(raw)[0]/incident,math.exp(-4*.0625*5.5),1e-7)
    assert raw["spectral_volume"]["frame_index"]==7 and raw["spectral_volume"]["time_s"]==.7
    key=raw["spectral_surface_map"]["key"]
    assert raw["spectral_volume"]["frame_sha256"]==hashlib.sha256(field.read_bytes()).hexdigest()
    # Spatially varying source-to-camera attenuation: independent single-event
    # integral for a collimated beam through the whole linear field.
    v["sigma_s_per_m"]=constant(1)
    camera(1-1e-6,-1,.31,-.06)
    lamp.update(origin_m=[.70,.31,-.06],aperture_m=.052)
    v["cell_m"]=.02
    mapping["photons_per_source"]=131072
    rr=run()["spectral_volume"]["bands"][0]
    mass=.0625*5.5
    expected=mass*math.exp(-3*mass)/(4*math.pi**2*.052**2)
    near(rr["center_single_radiance"],expected,.12*expected)
    near(rr["first_scatter_w"],-math.expm1(-3*mass)/3,.006)
    print("Heterogeneous VF3D single scatter:",rr["center_single_radiance"],"reference:",expected)
    v.update(sigma_s_per_m=constant(0),cell_m=.05)
    camera(.71,1,.28,-.1);lamp.update(origin_m=[.70,.28,-.1],aperture_m=.008)
    mapping["photons_per_source"]=16384
    v["source_path"]=field.name
    assert radiance(run())==radiance(raw)
    v["source_path"]="density.json"
    den[0]+=.5;write_field()
    assert run()["spectral_surface_map"]["key"]!=key
    v["frame_index"]=8;run(False,False);v["frame_index"]=7
    # The checked-in simulator frame and its corresponding pack must decode identically.
    vf=FIX/"scene_project_worker_snapshot/assets/vf3d/active"
    v.update(frame_index=0,source_path=str(vf/"manifest.json"))
    real=run();v["source_path"]=str(vf/"frame_000000.pack")
    near(radiance(run())[0],radiance(real)[0],1e-9)
    # Water body closure: query the established simulator Y to scene Z mapping,
    # below, inside and above the resolved surface. No RGB absorption is imported.
    v.update(kind="water_heightfield",source_path=str(FIX/"water_body_boundary/manifest_valid.json"),frame_index=0)
    lamp["aperture_m"]=.01
    mapping["photons_per_source"]=2048
    for height,wet in [(0,False),(.4,True),(.99,True)]:
        camera(.8,1,.4,height)
        lamp["origin_m"]=[.7,.4,height]
        ratio=radiance(run())[0]*(math.pi*.01)**2
        near(ratio,math.exp(-2*.5) if wet else 1,1e-8)
    v["source_path"]=str(FIX/"water_body_boundary/manifest_dynamic_unified.json")
    near(radiance(run())[0]*(math.pi*.01)**2,math.exp(-1),1e-8)
    # Compare the real water occupancy and a uniform box with the identical
    # flat interior. Both cameras and photons use the physical medium length.
    v.update(sigma_s_per_m=constant(3),cell_m=.04)
    lamp.update(origin_m=[.80,.4,.6],aperture_m=.14)
    camera(.94,-1,.4,.6)
    mapping["photons_per_source"]=65536
    water=run()
    r=water["spectral_volume"]["bands"][0]
    # Source and receiver are inside the medium. First-order contributions come
    # from all illuminated points between x=.80 and x=.94.
    pred=.14*3*math.exp(-5*.14)/(4*math.pi**2*.14**2)
    near(r["center_single_radiance"],pred,.20*pred)
    print("Water single scatter:",r["center_single_radiance"],"reference:",pred)
    v["source_path"]=str(FIX/"water_body_boundary/manifest_legacy.json");run(False,False)
    # A standalone volume bench needs only a detector plane.
    scene=json.loads((FIX/"spectral_prism/scene_runtime.json").read_text())
    scene["objects"]=[o for o in scene["objects"] if o["object_id"]=="screen"]
    (root/"matte.json").write_text(json.dumps(scene))
    q["scene"]["runtime_scene_path"]=str(root/"matte.json")
    del s["glass_id"],s["glass"]
    v["source_path"]=str(FIX/"water_body_boundary/manifest_valid.json")
    assert run()["spectral_volume"]["bands"][0]["scatter_events"]>0
    scene["objects"][0]["primitive"]["height"]=3
    (root/"matte.json").write_text(json.dumps(scene))
    v["sigma_s_per_m"]=constant(0)
    lamp.update(origin_m=[.7,.64,1.03],aperture_m=.01)
    camera(.88,1,.64,1.03)
    # Raised corner at (1.5,.7,1.1) and the adjacent 1 m samples
    # put the crossing at x=.8. Both optical legs end at x=1.
    near(radiance(run())[0]*(math.pi*.01)**2,math.exp(-2*(.20+.12)),2e-6)
    for field,bad in [("g",1),("cell_m",0),("sigma_s_per_m",constant(-1)),("frame_index",1)]:
        good=v[field];v[field]=bad;run(False,False);v[field]=good
    print("VF3D metric integral, exact frames, pack parity, masks and water closure passed")
    print("PASS S5 spectral volumes")


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--cli",type=Path,required=True)
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="optic-s5-bench-") as root:
        qualify(args.cli.resolve(),Path(root))

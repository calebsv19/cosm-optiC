#!/usr/bin/env python3
"""S3 native map, independent receiver radiometry, prism and two-plane lens bench."""
import argparse
import copy
import json
import math
from pathlib import Path
import shutil
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[2]
PI = math.pi


def xyz(x,y,z):
    return dict(x=x,y=y,z=z)


def near(actual, expected, tol):
    assert abs(actual-expected) <= tol, (actual, expected, tol)


def qualify(cli, root):
    # Keep relative mesh references valid while mutating only test-owned geometry.
    fixture = root / "prism"
    shutil.copytree(REPO / "tests/fixtures/spectral_prism", fixture)
    (root/"config").symlink_to(REPO/"config",target_is_directory=True)
    (root/"data/runtime/videos").mkdir(parents=True)
    base = json.loads((REPO/"tests/fixtures/spectral_surface_map/request.json").read_text())
    base["output"] = dict(root=str(root/"out"), overwrite=True)
    base["scene"]["runtime_scene_path"] = str(fixture/"scene_runtime.json")
    base["render"].update(width=1,height=1)
    request, summary = root/"request.json", root/"summary.json"

    def run(q, render=True, valid=True):
        request.write_text(json.dumps(q))
        proc = subprocess.run([cli,"--request",request,"--render" if render else "--preflight",
                               "--summary",summary,"--summary-file-only"],cwd=root,capture_output=True)
        assert (proc.returncode == 0) == valid, (proc.returncode,proc.stderr[-700:])
        if valid:
            d=json.loads(summary.read_text())
            st=d["spectral_surface_map"]
            assert st["reused"] == render
            assert st["launched"] == sum(st[k] for k in ["deposits","direct_arrivals","missed","depth_limited"])
            return st,d.get("spectral_center_pixel_xyz")

    def point(q, pos, target):
        q["inspection"].update(camera_position=xyz(*pos),camera_look_at=xyz(*target))

    def mapping(q):
        return q["spectral"]["surface_map"]

    def lamp(q):
        return mapping(q)["sources"][0]

    lines = json.loads((fixture/"request.json").read_text())["spectral"]["lines"]
    nm = [r["vacuum_nm"] for r in lines]
    screen_y = [.2591634554,.2668098630,.2701995165]
    transmitted = [.883307,.885624,.886637]
    q = copy.deepcopy(base)
    lamp(q).update(lines=[[w,1] for w in nm],continuum=[])
    point(q,[.9,screen_y[1],0],[1,screen_y[1],0])
    st, normal = run(q)
    assert st["deposits"] == mapping(q)["photons_per_source"] and st["gather_candidates"] > 0
    for row,y,power in zip(st["bands"],screen_y,transmitted,strict=True):
        near(row["centroid_m"][1],y,.00005)
        near(row["watts"],power,.001)
    assert max(normal)>0
    direct_key=st["key"]
    point(q,lamp(q)["origin_m"],[0,0,0])
    _,through = run(q)
    assert max(through)>0 and all(math.isfinite(x) for x in through)
    separate=[]
    for w in nm:
        one=copy.deepcopy(q)
        lamp(one).update(lines=[[w,1]])
        _,sensor=run(one)
        separate.append(sensor)
    # Independently stratified one-line and mixed estimates differ by finite sample noise.
    for combined,values in zip(through,zip(*separate)):
        near(combined,sum(values),.06*sum(values))
    assert run(q)[0]["key"] == direct_key
    q["spectral"]["exposure"] *= 2
    assert run(q)[0]["key"] == direct_key
    q["spectral"]["exposure"] /= 2
    # Per-line bulk absorption applies to both the emitted and camera path.
    filtered=copy.deepcopy(q)
    mapping(filtered)["absorption_per_m"]=[[380,0],[nm[0],0],[nm[1],2],[nm[2],6],[780,6]]
    clear_rows=run(q)[0]["bands"]
    tinted_rows=run(filtered)[0]["bands"]
    for c0,c1,sigma in zip(clear_rows,tinted_rows,[0,2,6]):
        ratio=c1["center_band_radiance"]/c0["center_band_radiance"]
        near(ratio,math.exp(-2*sigma*.1994),.002)
    mapping(q)["enabled"]=False
    assert run(q)[1] == [0,0,0]

    # Two independently located lamps. Their direct and glass-transmitted
    # paths share this camera pixel. Direct analytic illumination survives map-off.
    point(q,[.9,screen_y[1],0],[1,screen_y[1],0])
    mapping(q)["enabled"]=True
    direct=copy.deepcopy(lamp(q))
    direct.update(origin_m=[.7,screen_y[1],0],direction=[1,0,0],aperture_m=.01,lines=[[nm[2],3]])
    mapping(q)["sources"].append(direct)
    both,c = run(q)
    assert both["direct_arrivals"] == mapping(q)["photons_per_source"]
    mapping(q)["enabled"]=False
    _,off = run(q)
    assert max(off)>0
    for total,a,b in zip(c,off,normal):
        near(total,a+b,1e-8*max(1,total))
    mapping(q)["enabled"]=True
    mapping(q)["sources"]=[direct]
    assert run(q)[1] == off

    # Finite receiver routing: put another plane just behind the first, well
    # within a gather radius. Its back face and its front face must both be dark.
    scene_path = fixture/"scene_runtime.json"
    original = scene_path.read_text()
    scene = json.loads(original)
    second = copy.deepcopy(scene["objects"][0])
    def move_plane(obj,x):
        obj["primitive"]["frame"]["origin"]["x"] = x
        obj["transform"]["position"]["x"] = x
    move_plane(second,1.0002)
    second["object_id"]="second_screen"
    scene["objects"].append(second)
    scene_path.write_text(json.dumps(scene))
    q=copy.deepcopy(base)
    lamp(q).update(lines=[[nm[1],1]],continuum=[])
    point(q,[1.0001,screen_y[1],0],[2,screen_y[1],0])
    second_st,black=run(q)
    assert second_st["deposits"] and black==[0,0,0]
    point(q,[1.0001,screen_y[1],0],[0,screen_y[1],0])
    assert run(q)[1]==[0,0,0]
    scene_path.write_text(original)

    # Uniform continuum, constant-index matched glass. The known disk has
    # radiance P/(pi^2*a^2). The independently tabulated S1 integral uses
    # unit response density per vacuum nm over the entire visible interval.
    q=copy.deepcopy(base)
    q["spectral"].update(glass="constant",constant_ior=1)
    lamp(q).update(origin_m=[-.2,0,0],direction=[1,0,0],aperture_m=.02)
    point(q,[.9,0,0],[1,0,0])
    expected=[x/400/(PI**2*.02**2) for x in (106.88307286,106.88495010,106.87176020)]
    errors=[]
    for n,r,b,tol in [(1024,.012,8,.4),(8192,.007,32,.13),(65536,.005,80,.05)]:
        q["spectral"]["receiver_radius_m"]=r
        mapping(q).update(photons_per_source=n,bins=b)
        st,sensor=run(q)
        assert st["deposits"]==n and len(st["bands"])==b
        near(sum(row["watts"] for row in st["bands"]),1,1e-12)
        for row in st["bands"]:
            near(row["watts"],1/b,1.01/n)
            assert row["width_nm"]==400/b
        err=max(abs(x/y-1) for x,y in zip(sensor,expected))
        assert err<tol,(n,r,b,err)
        errors.append(err)
    assert errors[-1] < errors[0]
    print("continuum convergence max XYZ relative errors:",errors)

    # Independent integration over a white prism source. Evaluate Snell and
    # unpolarized Fresnel at 4096 quadrature nodes across the full source.
    def prism_reference(vac):
        s=(1000/vac)**2
        l2=(.001*vac/(1+1e-8*(8060.51+2480990/(132.274-s)+17455.7/(39.32957-s))))**2
        n=math.sqrt(1+sum(B*l2/(l2-C) for B,C in [(1.03961212,.00600069867),
                    (.231792344,.0200179144),(1.01046945,103.560653)]))
        i,A=math.radians(50),PI/3
        r=math.asin(math.sin(i)/n)
        e=math.asin(n*math.sin(A-r))
        x=.2/(math.tan(r)+1/math.tan(A))
        y=x*math.tan(r)+(1-x)*math.tan(A-e)
        def T(a,b):
            return 1-.5*((math.sin(a-b)/math.sin(a+b))**2+(math.tan(a-b)/math.tan(a+b))**2)
        return y,T(i,r)*T(A-r,e)
    q=copy.deepcopy(base)
    st,_=run(q,False)
    assert run(q,False)[0]["bands"]==st["bands"]
    for b,row in enumerate(st["bands"]):
        w=row["width_nm"]
        samples=[prism_reference(380+w*(b+(k+.5)/256)) for k in range(256)]
        power=sum(T for _,T in samples)/256*w/400
        centroid=sum(y*T for y,T in samples)/sum(T for _,T in samples)
        near(row["watts"],power,.00015)
        near(row["centroid_m"][1],centroid,.00012)
    # Through-glass continuum gather. Independent differential footprint
    # dy_screen/du for a disk coordinate perpendicular to the incident beam.
    # At the chief ray the true finite-aperture irradiance is uniform locally.
    through_q=copy.deepcopy(q)
    mapping(through_q).update(photons_per_source=65536,bins=80)
    lamp(through_q)["aperture_m"]=.014
    through_q["spectral"]["receiver_radius_m"]=.005
    point(through_q,lamp(through_q)["origin_m"],[0,0,0])
    rows=run(through_q)[0]["bands"]
    # Sum integrated radiance across bands without applying the observer.
    # As the camera path and source share an axis, both Fresnel factors enter.
    reference=0
    incidence=math.radians(50)
    for k in range(4096):
        wave=380+400*(k+.5)/4096
        air_s=(1000/wave)**2
        um=wave*.001/(1+1e-8*(8060.51+2480990/(132.274-air_s)+17455.7/(39.32957-air_s)))
        index=math.sqrt(1+sum(B/(1-C/um**2) for B,C in [(1.03961212,.00600069867),
                       (.231792344,.0200179144),(1.01046945,103.560653)]))
        r=math.asin(math.sin(incidence)/index)
        e=math.asin(index*math.sin(PI/3-r))
        jac=(1-(math.tan(r)-math.tan(PI/3-e))/(math.tan(r)+1/math.tan(PI/3)))/math.cos(incidence)
        reference+=prism_reference(wave)[1]**2/(4096*PI**2*.014**2*jac)
    recovered=sum(row["center_band_radiance"] for row in rows)
    near(recovered,reference,.05*reference)
    print("through-glass broadband integrated radiance:",recovered,"reference:",reference)
    # Continuous and atomic contributions at the same nominal wavelength retain distinct measures.
    mixed=copy.deepcopy(base)
    lamp(mixed)["lines"]=[[380+200/mapping(mixed)["bins"],.2]]
    mixed_rows=run(mixed,False)[0]["bands"]
    near(mixed_rows[0]["vacuum_nm"],mixed_rows[-1]["vacuum_nm"],1e-12)
    assert mixed_rows[0]["width_nm"]>0 and mixed_rows[-1]["width_nm"]==0
    near(mixed_rows[-1]["watts"],.2*prism_reference(mixed_rows[-1]["vacuum_nm"])[1],.0002)
    # Map identity must change for optical, sampling, source and actual mesh changes.
    for owner,key,value in [(q["spectral"],"receiver_radius_m",.002),(mapping(q),"seed",28),
                            (mapping(q),"absorption_per_m",1),(lamp(q),"aperture_m",.003)]:
        old=owner[key];owner[key]=value
        assert run(q,False)[0]["key"]!=st["key"]
        owner[key]=old
    malformed=copy.deepcopy(q)
    lamp(malformed)["continuum"]=[[780,1],[380,1]]
    run(malformed,False,False)

    # A symmetric faceted plano-convex cylindrical lens. A narrow collimated
    # disk at height h hits one planar facet of slope alpha, with known exit
    # x and angle. Two physical receiver depths recover the chromatic focus.
    mesh_path=next((fixture/"assets/mesh_assets").glob("*.json"))
    mesh=json.loads(mesh_path.read_text())
    alpha,h=math.radians(8),.075
    outline=[(0,-.15),(0,.15),(.025,.15),(.025+.15*math.tan(alpha),0),(.025,-.15)]
    vertices=[xyz(x,y,z) for z in [-.05,.05] for x,y in outline]
    faces=[]
    for j in range(1,4):
        faces.extend([(0,j,j+1),(5,6+j,5+j)])
    for j in range(5):
        k=(j+1)%5
        faces.extend([(j,j+5,k+5),(j,k+5,k)])
    mesh["local_bounds"]=dict(min=xyz(0,-.15,-.05),max=xyz(outline[3][0],.15,.05))
    mesh["mesh"]=dict(vertex_count=10,triangle_count=len(faces),vertices=vertices,
                      triangles=[dict(a=a,b=b,c=c,surface_group_id="glass") for a,b,c in faces])
    mesh["surface_groups"][0]["triangle_span"]["count"]=len(faces)
    mesh_path.write_text(json.dumps(mesh))
    assert run(q,False)[0]["key"]!=st["key"]
    lamp(q).update(origin_m=[-.2,h,0],direction=[1,0,0],aperture_m=.00015,
                   lines=[[w,1] for w in nm],continuum=[])
    centers=[]
    for depth in [.4,.8]:
        scene=json.loads(original)
        move_plane(scene["objects"][0],depth)
        scene_path.write_text(json.dumps(scene))
        rows=run(q,False)[0]["bands"]
        centers.append([row["centroid_m"][1] for row in rows])
    focus=[]
    for j,n in enumerate([1.52238,1.5168,1.51432]):
        y0,y1=centers[0][j],centers[1][j]
        f=.4+.4*y0/(y0-y1)
        pred=.025+(.15-h)*math.tan(alpha)+h/math.tan(math.asin(n*math.sin(alpha))-alpha)
        near(f,pred,.00015)
        focus.append(f)
    assert focus[0]<focus[1]<focus[2]
    print("two-plane focal positions (m):",focus)
    print("PASS S3 spectral surface map")


if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--cli",type=Path,required=True)
    args=p.parse_args()
    with tempfile.TemporaryDirectory(prefix="optic-surface-map-") as root:
        qualify(args.cli.resolve(),Path(root))

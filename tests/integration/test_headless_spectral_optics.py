#!/usr/bin/env python3
"""S4 headless emitter, sensor, surface and slab measurements."""
import argparse
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[2]
PI = math.pi


def near(actual, expected, rel=1e-8):
    assert abs(actual - expected) <= rel * max(1e-10, abs(expected)), (actual, expected, rel)


def vector(x, y, z):
    return dict(x=x, y=y, z=z)


def slab(mesh_path, thickness):
    mesh = json.loads(mesh_path.read_text())
    vertices = [vector(x, y, z) for z in [-.8, .8]
                for x, y in [(0, -.8), (0, .8), (thickness, .8), (thickness, -.8)]]
    faces = [(0, 1, 2), (0, 2, 3), (4, 6, 5), (4, 7, 6)]
    for i in range(4):
        j = (i + 1) % 4
        faces.extend([(i, i + 4, j + 4), (i, j + 4, j)])
    mesh["local_bounds"] = dict(min=vector(0, -.8, -.8), max=vector(thickness, .8, .8))
    mesh["mesh"] = dict(vertex_count=8, triangle_count=len(faces), vertices=vertices,
                        triangles=[dict(a=a, b=b, c=c, surface_group_id="glass") for a,b,c in faces])
    mesh["surface_groups"][0]["triangle_span"]["count"] = len(faces)
    mesh_path.write_text(json.dumps(mesh))


def cie_at_air(nm):
    table = (REPO/"src/render/cie_1931_2deg_380_780.inc").read_text()
    rows = re.findall(r'\{([^{}]+)\}', table)
    return [float(x) for x in rows[nm-379].split(",")]


def vacuum(air_nm):
    nm = air_nm
    for _ in range(4):
        s = (1000/nm)**2
        nm = air_nm * (1 + 1e-8*(8060.51+2480990/(132.274-s)+17455.7/(39.32957-s)))
    return nm


def white_lines(wavelengths):
    # Solve each positive three-primary source against the same neutral D65 XYZ.
    def det(a,b,c):
        return sum(a[i]*(b[(i+1)%3]*c[(i+2)%3]-b[(i+2)%3]*c[(i+1)%3]) for i in range(3))
    a,b,c = [cie_at_air(n) for n in wavelengths]
    target = [.95047, 1, 1.08883]
    weights = [det(target,b,c),det(a,target,c),det(a,b,target)]
    lines = [[vacuum(n), w/det(a,b,c)] for n,w in zip(wavelengths,weights)]
    assert all(w>0 for _,w in lines)
    return lines


def qualify(cli, root):
    fixture = root/"bench"
    shutil.copytree(REPO/"tests/fixtures/spectral_prism",fixture)
    (root/"config").symlink_to(REPO/"config",target_is_directory=True)
    (root/"data/runtime/videos").mkdir(parents=True)
    scene_path = fixture/"scene_runtime.json"
    scene = json.loads(scene_path.read_text())
    mesh = next((fixture/"assets/mesh_assets").glob("*.json"))
    slab(mesh,.1)
    q = json.loads((REPO/"tests/fixtures/spectral_surface_map/request.json").read_text())
    q["scene"]["runtime_scene_path"] = str(scene_path)
    q["render"].update(width=1,height=1)
    q["output"].update(root=str(root/"out"),overwrite=True)
    s = q["spectral"]
    s.update(glass="constant",constant_ior=1,exposure=.1)
    mapping = s["surface_map"]
    mapping.update(enabled=False,bins=80,photons_per_source=16384)
    lamp = mapping["sources"][0]
    lamp.update(origin_m=[.8,0,0],direction=[1,0,0],aperture_m=.3)
    def camera(x,y,angle=0):
        q["inspection"].update(camera_position=vector(x,y,0),camera_look_at=vector(x+1,y+math.tan(angle),0))
    camera(.9,0)
    def run(render=True,valid=True):
        request,summary=root/"request.json",root/"summary.json"
        request.write_text(json.dumps(q))
        cmd=[cli,"--request",request,"--summary",summary,"--summary-file-only","--render" if render else "--preflight"]
        result=subprocess.run(cmd,cwd=root,capture_output=True)
        assert (result.returncode==0)==valid,(result.returncode,result.stderr[-800:],summary.read_text()[-500:] if summary.exists() else "")
        if valid:
            return json.loads(summary.read_text())
    def bands(result):
        return result["spectral_surface_map"]["bands"]
    flat = [[380,1],[780,1]]
    s["sensors"]=[dict(type="radiometric",response=flat),dict(type="quantum_efficiency",response=[[380,0],[780,1]])]
    area_factor=1/(PI*lamp["aperture_m"])**2
    # Fine independent integration of the analytic functions (including truncation at domain edges).
    lamp.pop("lines"); lamp.pop("continuum")
    for model,f in [
        (dict(type="blackbody",kelvin=2800),lambda n:(550/n)**5/math.expm1(1.438776877e7/(2800*n))),
        (dict(type="gaussian_mixture",peaks=[[450,3,1],[625,15,.4]]),
         lambda n:math.exp(-.5*((n-450)/3)**2)+.4*math.exp(-.5*((n-625)/15)**2)),
        (dict(type="rgb_approximation",linear_rgb=[1,.4,.1]),
         lambda n:sum(w*math.exp(-.5*((n-c)/20)**2) for w,c in [(1,610),(.4,545),(.1,455)]))]:
        lamp["spectrum"]=dict(model,visible_watts=2)
        result=run()
        raw=result["spectral_center_sensor_signals"]
        near(raw[0],2*area_factor)
        grid=[380+(i+.5)*.05 for i in range(8000)]
        expected=2*area_factor*sum(f(n)*n*1e-9/(6.62607015e-34*299792458)*(n-380)/400 for n in grid)/sum(f(n) for n in grid)
        near(raw[1],expected,.003)
        # Exercise the native wavelength sampler as well as direct integration.
        mapping["enabled"]=True
        lamp["origin_m"][0]=-.2
        sampled=bands(run(False))
        near(sum(r["watts"] for r in sampled),2)
        for a,b in zip(sampled,bands(result)):
            assert abs(a["watts"]-b["center_band_radiance"]/area_factor)<=2.01/mapping["photons_per_source"]
        mapping["enabled"]=False
        lamp["origin_m"][0]=.8
        per_bin=[sum(f(n) for n in grid[j:j+100]) for j in range(0,len(grid),100)]
        observed=[r["center_band_radiance"] for r in bands(result)]
        assert sum(abs(a/sum(observed)-b/sum(per_bin)) for a,b in zip(observed,per_bin))<.025
        # Same deposited spectrum drives both readings. Changing film response preserves transport identity.
        if model["type"]=="blackbody":
            s["display_sensor"]=0
            s["exposure"]=.2
            another=run()
            assert another["spectral_surface_map"]["key"]==result["spectral_surface_map"]["key"]
            assert another["spectral_center_sensor_signals"]==raw
            assert another["spectral_center_pixel_xyz"]==result["spectral_center_pixel_xyz"]
            # Output bitmap is monochrome after selecting the radiometric sensor.
            bmp=(root/"out/frames/frame_0000.bmp").read_bytes()
            off=int.from_bytes(bmp[10:14],"little")
            assert bmp[off]==bmp[off+1]==bmp[off+2] and 0<bmp[off]<255
            del s["display_sensor"]
    lamp.pop("spectrum")
    lamp["continuum"]=[]
    # Two metamers: neutral XYZ agrees before the slab; a selective absorber separates them.
    whites=[]; filtered=[]
    for line_set in [[445,535,605],[470,555,650]]:
        lines=white_lines(line_set)
        lamp["lines"]=lines
        camera(.9,0)
        neutral=run()["spectral_center_pixel_xyz"]
        for a,e in zip(neutral,[.95047,1,1.08883]): near(a,area_factor*e)
        whites.append(neutral)
        camera(-.2,0)
        mapping["absorption_per_m"]=[[380,0],[535,0],[555,8],[780,8]]
        result=run()
        expect=[0.,0.,0.]
        for air,(nm,watts) in zip(line_set,lines):
            sigma=8*max(0,min(1,(nm-535)/20))
            for i,c in enumerate(cie_at_air(air)):
                expect[i]+=area_factor*watts*c*math.exp(-.1*sigma)
        for a,e in zip(result["spectral_center_pixel_xyz"],expect): near(a,e,1e-6)
        filtered.append(result["spectral_center_pixel_xyz"])
        mapping["absorption_per_m"]=0
    assert abs(filtered[0][1]-filtered[1][1])>.15*area_factor
    print("Neutral and filtered metamer Y:",whites[0][1],filtered[0][1],filtered[1][1])
    # Fresnel and optical distance through parallel faces with a camera in the exterior medium.
    lamp.update(lines=[[450,1],[650,1]],aperture_m=1)
    mapping["absorption_per_m"]=[[380,2],[450,2],[650,5],[780,5]]
    s["constant_ior"]=1.5
    for thickness in [.05,.15]:
        slab(mesh,thickness)
        for n0,angle in [(1,0),(1,math.radians(16)),(1.33,math.radians(16))]:
            s["exterior_ior"]=n0
            camera(-.2,0,angle)
            ct=math.sqrt(1-(n0*math.sin(angle)/1.5)**2); ci=math.cos(angle)
            F=.5*(((n0*ci-1.5*ct)/(n0*ci+1.5*ct))**2+((1.5*ci-n0*ct)/(1.5*ci+n0*ct))**2)
            for row,sigma in zip(bands(run()),[2,5]):
                near(row["center_band_radiance"],(1-F)**2*math.exp(-sigma*thickness/ct)/PI**2,1e-6)
    # Uniform projected solar disk, partial overlap of projected source and emitting aperture.
    s["exterior_ior"]=1; mapping["absorption_per_m"]=0
    lamp.update(lines=[[550,1]],aperture_m=.001,angular_radius_deg=.265)
    camera(.9,0)
    near(bands(run())[0]["center_band_radiance"],1/(PI*.001)**2)
    def disk_fraction(y):
        hits=0
        for j in range(24000):
            u=(j+.5)/24000
            sint=math.sqrt(u)*math.sin(math.radians(.265))
            radius=.2*sint/math.sqrt(1-sint*sint)
            phi=2.399963229728653*j
            hits+=(y-radius*math.cos(phi))**2+(radius*math.sin(phi))**2<=.001**2
        return hits/24000
    camera(.9,.001)
    fraction=bands(run())[0]["center_band_radiance"]*(PI*.001)**2
    assert abs(fraction-disk_fraction(.001))<.04
    camera(.9,.002)
    assert bands(run())[0]["center_band_radiance"]==0
    # Surface reflectance and opt-in native procedural texture, with bounded output.
    lamp.update(aperture_m=.5,angular_radius_deg=0,lines=[[455,1],[610,1]])
    camera(.9,0)
    s["receiver_reflectance"]=[[380,.2],[455,.2],[610,.8],[780,.8]]
    for row,v in zip(bands(run()),[.2,.8]): near(row["center_band_radiance"],v/(PI*.5)**2)
    s.pop("receiver_reflectance")
    s["receiver_texture"]="scene_rgb_approximation"
    mat=dict(object_id="screen",object_color=0xAACCAA,material_id=0,texture_id=1,
             texture_strength=1,texture_scale=5,texture_pattern_mode=2)
    scene["extensions"]=dict(ray_tracing=dict(authoring=dict(object_materials=[mat])))
    scene_path.write_text(json.dumps(scene))
    values=[]
    for y in [0,.05,.13,.27,.38]:
        camera(.9,y)
        values.append(bands(run())[0]["center_band_radiance"]*(PI*.5)**2)
    assert min(values)>=0 and max(values)<=1 and max(values)-min(values)>.005,values
    texture_key=run()["spectral_surface_map"]["key"]
    mat["texture_strength"]=0
    scene_path.write_text(json.dumps(scene))
    neutral_texture=[]
    for y in [0,.13,.38]:
        camera(.9,y)
        untextured=run()
        assert untextured["spectral_surface_map"]["key"]==texture_key
        neutral_texture.append(bands(untextured)[0]["center_band_radiance"])
    assert max(neutral_texture)-min(neutral_texture)<1e-12
    print("Native textured receiver sample range:",min(values),max(values))
    scene["extensions"]={};scene_path.write_text(json.dumps(scene));s.pop("receiver_texture")
    # Forward solar cone spreads deposits on an index-matched slab. Reproject
    # the identical received line flux into both detectors with distinct units.
    s.update(constant_ior=1,receiver_radius_m=.005)
    lamp.update(origin_m=[-.2,0,0],aperture_m=.001,angular_radius_deg=1,lines=[[550,2]])
    mapping.update(enabled=True,photons_per_source=32768)
    camera(.9,.013)
    solar=run()
    row=bands(solar)[0]
    near(row["watts"],2)
    # At this interior point the projected angular disk is locally uniform
    # to O(theta^2), convolved with a smaller aperture and gather kernel.
    expected=2/(PI*1.2*math.sin(math.radians(1)))**2
    near(row["center_band_radiance"],expected,.08)
    near(solar["spectral_center_sensor_signals"][0],row["center_band_radiance"])
    near(solar["spectral_center_sensor_signals"][1],row["center_band_radiance"]*(550-380)/400*550e-9/(6.62607015e-34*299792458))
    lamp["angular_radius_deg"]=0
    assert bands(run())[0]["center_band_radiance"]==0
    s["constant_ior"]=1.5
    # Near-smooth rough transmission includes repeated internal Fresnel reflections.
    # A large collecting screen intercepts almost all transmitted paths.
    slab(mesh,.05)
    lamp.update(origin_m=[-.2,0,0],aperture_m=.008,lines=[[550,1]])
    mapping.update(enabled=True,photons_per_source=32768)
    s.update(glass_roughness=.015,receiver_radius_m=.008)
    camera(.9,0)
    result=run()
    row=bands(result)[0]
    near(row["watts"],(1-.04)/(1+.04),.012)
    narrow=row["center_band_radiance"]
    s["glass_roughness"]=.22
    rough=run()
    assert 0<bands(rough)[0]["center_band_radiance"]<.7*narrow
    assert bands(run())==bands(rough)
    camera(-.2,0)
    assert bands(run())[0]["center_band_radiance"]>0
    print("Rough slab forward transmittance:",row["watts"],"solar penumbra fraction:",fraction)
    # Illegal new fields fail the ordinary CLI preflight.
    for holder,field,value in [(s,"glass_roughness",1.1),(lamp,"angular_radius_deg",-1),
                               (s,"display_sensor",4),(s,"receiver_reflectance",[[380,.1],[780,1.2]]),
                               (lamp,"spectrum",dict(type="blackbody",kelvin=0,visible_watts=1))]:
        saved=holder.get(field)
        holder[field]=value
        run(False,False)
        if saved is None: del holder[field]
        else: holder[field]=saved
    print("PASS S4 spectral optics")


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--cli",required=True,type=Path)
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="optic-s4-bench-") as root:
        qualify(args.cli.resolve(), Path(root))

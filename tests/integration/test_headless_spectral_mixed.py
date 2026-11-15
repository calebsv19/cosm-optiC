#!/usr/bin/env python3
"""Native spectral sky, dielectric map and participating medium controls."""
import argparse
import copy
import json
import math
from pathlib import Path
import shutil
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[2]


def vector(x, y, z):
    return {"x": x, "y": y, "z": z}


def prepare(dest):
    shutil.copytree(REPO / "tests/fixtures/spectral_prism", dest / "bench")
    (dest / "config").symlink_to(REPO / "config", target_is_directory=True)
    (dest / "data/runtime/videos").mkdir(parents=True)
    scene_path = dest / "bench/scene_runtime.json"
    scene = json.loads(scene_path.read_text())
    scene["objects"][0]["primitive"].update(width=1, height=.8)
    panel_origin = vector(.745, .105, 0)
    scene["objects"].append({
        "object_id": "orange_panel",
        "object_type": "plane_primitive",
        "space_mode_intent": "2d",
        "dimensional_mode": "plane_locked",
        "locked_plane": "xz",
        "transform": {"position": panel_origin, "rotation": vector(0, 0, 0), "scale": vector(1, 1, 1)},
        "primitive": {
            "kind": "plane_primitive", "width": .49, "height": .85,
            "frame": {"origin": panel_origin, "axis_u": vector(1, 0, 0),
                      "axis_v": vector(0, 0, -1), "normal": vector(0, 1, 0)},
        },
        "flags": {"visible": True, "locked": False, "selectable": True},
    })
    scene_path.write_text(json.dumps(scene, indent=2))
    request = json.loads((REPO / "tests/fixtures/spectral_surface_map/request.json").read_text())
    request["scene"]["runtime_scene_path"] = "bench/scene_runtime.json"
    request["render"].update(width=1, height=1)
    request["inspection"].update(camera_zoom=.92, camera_position=vector(.61, .29, 0),
                                 camera_look_at=vector(1, .2668, 0))
    request["output"]["overwrite"] = False
    spectral = request["spectral"]
    spectral.update(exposure=.65, receiver_radius_m=.0035, smooth_glass_transport="fresnel",
                    glass_camera_samples=8, receiver_reflectance=[[380, .72], [780, .72]],
                    selected_pixel={"x": 0, "y": 0})
    spectral["surface_map"].update(bins=24, photons_per_source=65536)
    spectral["surface_map"]["sources"][0].update(
        aperture_m=.002, lines=[[450, 0], [530, 0], [610, 0]],
        continuum=[[380, .0025], [780, .0025]],
    )
    spectral["sky"] = {
        "axis": [-.7, .69, .18], "half_angle_deg": 28, "grid": 8,
        "radiance_lines": [[450, .018], [530, .023], [610, .067]],
    }
    spectral["diffuse_bounce_v1"] = {
        "panel_id": "orange_panel", "grid": 5, "indirect": True,
        "panel_reflectance": [[380, .045], [450, .045], [530, .23], [610, .86], [780, .86]],
    }
    spectral["participating_volume_v1"] = {
        "kind": "homogeneous", "bounds_min_m": [.65, .12, -.34],
        "bounds_max_m": [.985, .47, .34], "sigma_a_per_m": [[380, .045], [780, .045]],
        "sigma_s_per_m": [[380, .85], [780, .45]], "g": .28, "cell_m": .014,
    }
    return request


def qualify(cli, dest):
    source = prepare(dest)

    def run(name, q, valid=True):
        q["output"]["root"]=name
        req=dest/(name+".request.json")
        summary=dest/(name+".summary.json")
        req.write_text(json.dumps(q,indent=2))
        p=subprocess.run([cli,"--request",req,"--render","--summary",summary,"--summary-file-only"],
                         cwd=dest,capture_output=True,timeout=60)
        assert (p.returncode==0)==valid,(name,p.returncode,p.stderr[-500:],
                                      summary.read_text()[-500:] if summary.exists() else "")
        if valid:
            return json.loads(summary.read_text())

    def lamp_off(q):
        lamp=q["spectral"]["surface_map"]["sources"][0]
        # Keep the zero-valued continuum knots to preserve the same band layout.
        lamp["continuum"]=[[380,0],[780,0]]

    def sky_off(q):
        for line in q["spectral"]["sky"]["radiance_lines"]:line[1]=0

    combined=run("combined",copy.deepcopy(source))
    q=copy.deepcopy(source)
    sky_off(q)
    lamp=run("lamp_only",q)
    q=copy.deepcopy(source)
    lamp_off(q)
    sky=run("sky_only",q)
    assert combined["spectral_surface_map"]["key"]==lamp["spectral_surface_map"]["key"]
    for actual, a, b in zip(combined["spectral_center_pixel_xyz"],lamp["spectral_center_pixel_xyz"],
                            sky["spectral_center_pixel_xyz"]):
        assert math.isclose(actual,a+b,rel_tol=1e-12,abs_tol=1e-12),(actual,a+b)
    components={}
    for row, band in zip(combined["spectral_selected_pixel"]["bands"],
                         combined["spectral_surface_map"]["bands"]):
        keys=("direct_radiance","sky_direct_radiance","indirect_radiance","photon_surface_radiance",
              "single_scatter_radiance","multiple_scatter_radiance")
        assert math.isclose(sum(row[k] for k in keys),band["center_band_radiance"],rel_tol=1e-12,abs_tol=1e-12)
        for k in keys: components[k]=components.get(k,0)+row[k]
    for key in ("sky_direct_radiance","indirect_radiance","photon_surface_radiance","single_scatter_radiance"):
        assert components[key] > 0, components
    print("Mixed source linear superposition and probe closure passed",components)

    q=copy.deepcopy(source)
    lamp_off(q)
    s=q["spectral"]
    s["diffuse_bounce_v1"]["indirect"]=False
    s["sky"].update(axis=[-1,0,0],half_angle_deg=1,radiance_lines=[[610,1]])
    vol=s["participating_volume_v1"]
    vol["sigma_a_per_m"]=[[380,.85],[780,.85]]
    vol["sigma_s_per_m"]=[[380,0],[780,0]]
    q["inspection"].update(camera_position={"x":.61,"y":.2668,"z":0},
                           camera_look_at={"x":1,"y":.2668,"z":0})
    pure=run("sky_absorption",q)
    band=next(r for r in pure["spectral_selected_pixel"]["bands"] if r["vacuum_nm"]==610 and r["width_nm"]==0)
    tau=.85*(.985-.65)
    lo=math.cos(math.radians(1))
    n=2000
    h=(1-lo)/n
    def integrand(mu):return mu*math.exp(-tau/mu)
    integral=h*((integrand(lo)+integrand(1))*.5+sum(integrand(lo+i*h) for i in range(1,n)))
    expected=.72*math.exp(-tau)*2*integral
    assert math.isclose(band["sky_direct_radiance"],expected,rel_tol=1e-6),(band,expected)
    print("Independent sky and camera Beer–Lambert control",band["sky_direct_radiance"],expected)
    q=copy.deepcopy(source)
    q["spectral"]["sky"]["radiance_lines"][0][0]=451
    run("missing_line_atom",q,valid=False)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--cli", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="optic-mixed-spectral-") as temp:
        qualify(args.cli.resolve(), Path(temp))

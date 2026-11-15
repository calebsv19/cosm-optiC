#!/usr/bin/env python3
"""Independent incoherent slab oracle for smooth spectral Fresnel transport."""
import argparse
import json
import math
from pathlib import Path
import shutil
import subprocess
import tempfile

from test_headless_spectral_optics import REPO, slab, vector


def reference(n, angle, thickness, sigma):
    ci = math.cos(angle)
    ct = math.sqrt(1 - (math.sin(angle)/n)**2)
    rs = (ci - n*ct)/(ci + n*ct)
    rp = (n*ci - ct)/(n*ci + ct)
    r = (rs*rs + rp*rp)/2
    a = math.exp(-sigma * thickness / ct)
    series = 1 / (1 - r*r*a*a)
    transmitted = (1-r)**2*a*series
    reflected = r + (1-r)**2*r*a*a*series
    return transmitted, reflected, 1-transmitted-reflected


def qualify(cli, root):
    fixture = root/"bench"
    shutil.copytree(REPO/"tests/fixtures/spectral_prism", fixture)
    (root/"config").symlink_to(REPO/"config", target_is_directory=True)
    (root/"data/runtime/videos").mkdir(parents=True)
    scene_path = fixture/"scene_runtime.json"
    scene = json.loads(scene_path.read_text())
    thickness = .12
    slab(next((fixture/"assets/mesh_assets").glob("*.json")), thickness)
    q = json.loads((REPO/"tests/fixtures/spectral_surface_map/request.json").read_text())
    q["scene"]["runtime_scene_path"] = str(scene_path)
    q["render"].update(width=1, height=1)
    q["output"].update(root=str(root/"frames"), overwrite=True)
    q["inspection"].update(camera_position=vector(.9,0,0), camera_look_at=vector(1,0,0))
    s = q["spectral"]
    s.update(glass="constant", constant_ior=1.5, smooth_glass_transport="fresnel")
    m = s["surface_map"]
    m.update(photons_per_source=65536, seed=19)
    lamp = m["sources"][0]
    lamp.update(aperture_m=.015, lines=[[450,.5],[610,.5]], continuum=[])

    def run(name, side=1, render=False, valid=True):
        receiver = scene["objects"][0]
        x = 1 if side > 0 else -.45
        pos = vector(x,0,0)
        receiver["transform"]["position"] = pos
        receiver["primitive"]["frame"]["origin"] = pos
        scene_path.write_text(json.dumps(scene))
        request, summary = root/"request.json", root/(name+".json")
        request.write_text(json.dumps(q))
        done = subprocess.run([cli,"--request",request,"--summary",summary,"--summary-file-only",
                               "--render" if render else "--preflight"],cwd=root,
                              capture_output=True, timeout=60)
        assert (done.returncode == 0) == valid, (name, done.stderr[-800:],
                                                summary.read_text()[-500:] if summary.exists() else "")
        if valid:
            d = json.loads(summary.read_text())
            bands = d["spectral_surface_map"]["bands"]
            for row in bands:
                b = row["fresnel_transport"]
                closed = sum(b[k] for k in ("receiver_w","escaped_w","glass_absorbed_w",
                                            "medium_absorbed_w","depth_limited_w","other_loss_w"))
                assert abs(closed - b["launched_w"]) < 1e-10 and abs(b["other_loss_w"]) < 1e-12
                assert b["depth_limited_w"] == 0
            return d, [row["fresnel_transport"] for row in bands]

    for degrees, absorption in ((0,(0,0)),(32,(2,5))):
        angle = math.radians(degrees)
        lamp.update(origin_m=[-.25,0,0],direction=[math.cos(angle),math.sin(angle),0])
        m["absorption_per_m"] = [[380,absorption[0]],[450,absorption[0]],
                                 [610,absorption[1]],[780,absorption[1]]]
        _, transmission = run(f"transmit_{degrees}")
        _, reflection = run(f"reflect_{degrees}", -1)
        for i, (t,r) in enumerate(zip(transmission,reflection)):
            expected_t,expected_r,expected_a = reference(1.5,angle,thickness,absorption[i])
            values = (t["receiver_w"]/.5,r["receiver_w"]/.5,t["glass_absorbed_w"]/.5)
            assert max(abs(a-b) for a,b in zip(values,(expected_t,expected_r,expected_a))) < .006
            assert t["reflections"] > 0 and t["tir"] == 0
            print(f"{degrees} deg {lamp['lines'][i][0]} nm measured T/R/A={values} "
                  f"reference={(expected_t,expected_r,expected_a)}")

    # A ray grazing the side of the slab from within exceeds the critical angle.
    lamp.update(origin_m=[-.25,.53,0],direction=[2**-.5,2**-.5,0])
    m["absorption_per_m"] = 0
    _, edge = run("edge_tir")
    assert all(row["tir"] > 0 for row in edge)

    s["constant_ior"] = 1
    lamp.update(origin_m=[-.25,0,0],direction=[1,0,0])
    s["selected_pixel"] = {"x":0,"y":0}
    q["inspection"].update(camera_position=vector(-.1,0,0),camera_look_at=vector(1,0,0))
    matched, bands = run("matched", render=True)
    assert all(abs(row["receiver_w"]-.5)<1e-10 and row["reflections"]==0 for row in bands)
    assert all(row["mean_camera_throughput"] == 1
               for row in matched["spectral_selected_pixel"]["bands"])
    # Illuminate the receiver from behind the slab so its direct radiance is
    # independently known. Only the camera ray crosses the dielectric.
    s["constant_ior"] = 1.5
    s["glass_camera_samples"] = 64
    m["photons_per_source"] = 256
    factors = []
    for side in (1, -1):
        lamp.update(origin_m=[.7 if side > 0 else -.3,0,0],
                    direction=[side,0,0],aperture_m=.08)
        q["inspection"].update(camera_position=vector(-.1 if side > 0 else .2,0,0),
                               camera_look_at=vector(side,0,0))
        values = []
        for seed in range(19,27):
            m["seed"] = seed
            sample, _ = run(f"camera_{side}_{seed}", side, render=True)
            value = sample["spectral_surface_map"]["bands"][0]["center_band_radiance"]
            values.append(value * (math.pi * lamp["aperture_m"])**2 / .5)
        factors.append(sum(values)/len(values))
    expected, _, _ = reference(1.5,0,thickness,0)
    assert max(abs(v-expected) for v in factors) < .04, factors
    print("Opposite camera directions measured T:",factors,"reference:",expected)
    s["glass_roughness"] = .1
    run("rough_conflict",valid=False)
    print("PASS smooth Fresnel slab energy, TIR and index-matched camera controls")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--output-root",type=Path)
    args = parser.parse_args()
    if args.output_root:
        args.output_root.mkdir()
        qualify(args.cli.resolve(),args.output_root.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix="optic-fresnel-") as root:
            qualify(args.cli.resolve(),Path(root))

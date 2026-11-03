#!/usr/bin/env python3
"""Run the disk request, native scene, raw path oracles and undenoised image gates."""
import argparse
import copy
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

FIXTURE = Path(__file__).resolve().parents[1] / "fixtures/spectral_prism"
# Independent catalog line indices and the S0-S1 analytic prism solution.
REFERENCE = [(1.52238, 39.1412993, .2591634554),
             (1.51680, 38.6536338, .2668098630),
             (1.51432, 38.4382083, .2701995165)]


def lit_columns(bmp):
    offset = struct.unpack_from("<I", bmp, 10)[0]
    width, height = struct.unpack_from("<ii", bmp, 18)
    assert struct.unpack_from("<H", bmp, 28)[0] == 32
    result = set()
    for y in range(height):
        for x in range(width):
            start = offset + 4 * (y * width + x)
            if any(bmp[start:start+3]):
                result.add(x)
    return result


def qualify(cli, root):
    # Isolate native config writes and mesh caches from the operator's settings.
    (root / "config").symlink_to(FIXTURE.parents[2] / "config", target_is_directory=True)
    (root / "data/runtime/videos").mkdir(parents=True)
    fixture = root / "relocated"
    shutil.copytree(FIXTURE, fixture)
    base = json.loads((fixture / "request.json").read_text())
    output = root / "frames_out"
    path = fixture / "request.json"
    summary = root / "summary.json"

    def run(request, render=False, success=True, extra=()):
        request = copy.deepcopy(request)
        request["output"] = {"root": str(output), "overwrite": True}
        path.write_text(json.dumps(request))
        result = subprocess.run([cli, "--request", path, "--render" if render else "--preflight",
                                 "--summary", summary, "--summary-file-only", *extra],
                                capture_output=True, text=True, cwd=root)
        assert (result.returncode == 0) == success, (result.returncode, result.stderr[-1200:])
        if success:
            return json.loads(summary.read_text())

    def frame():
        return (output / "frames/frame_0000.bmp").read_bytes()

    report = run(base, render=True)
    first = frame()
    assert report["integrator_3d"] == "disney_v3_spectral" and report["frames_rendered"] == 1
    assert not report["render"]["denoise_enabled"]
    rows = report["spectral_pencil"]
    for line, row, (ior, angle, screen_y) in zip(base["spectral"]["lines"], rows, REFERENCE, strict=True):
        hit = row["forward"]
        assert hit["interfaces"] == 2 and hit["receiver_hit"] and not hit["depth_limited"]
        assert row["camera_axis"]["interfaces"] == 0 and row["camera_axis"]["receiver_hit"]
        assert hit["vacuum_nm"] == row["camera_axis"]["vacuum_nm"] == line["vacuum_nm"]
        assert abs(row["ior"] - ior) < 0.00001
        assert abs(row["deviation_deg"] - angle) < 0.001
        for measured, reference in zip(hit["position_m"], (1, screen_y, 0)):
            assert abs(measured - reference) < .0001
        # Independent two-face unpolarized Fresnel power, radians.
        incidence = math.radians(50)
        r = math.asin(math.sin(incidence) / ior)
        inside = math.radians(60) - r
        exit_angle = math.asin(ior * math.sin(inside))
        def transmit(a, b):
            return 1 - .5 * ((math.sin(a-b)/math.sin(a+b))**2 +
                              (math.tan(a-b)/math.tan(a+b))**2)
        assert abs(hit["path_watts"] - transmit(incidence,r)*transmit(inside,exit_angle)) < .00001

    # Each spectral line must light a real, spatially distinct patch. The
    # combined undenoised image must contain exactly their disjoint union.
    occupied = set()
    previous_max = -1
    for line, (ior, angle, screen_y) in zip(base["spectral"]["lines"], REFERENCE):
        q = copy.deepcopy(base)
        q["spectral"]["lines"] = [line]
        run(q, render=True)
        columns = lit_columns(frame())
        assert columns and min(columns) > previous_max
        # Midpoint of the projected disk to half a pixel. The sensor looks
        # along +x from 0.1 m away with a 55-degree vertical field before zoom.
        meters_per_pixel = 2*.1*math.tan(math.radians(55)/2)/(8*101)
        expected_x = 160 + (screen_y-.265)/meters_per_pixel
        assert abs((min(columns)+max(columns))/2 - expected_x) <= .5
        q["spectral"].update(glass="constant", constant_ior=ior)
        synthetic = run(q)["spectral_pencil"][0]
        assert abs(synthetic["deviation_deg"]-angle) < 1e-6
        assert abs(synthetic["forward"]["position_m"][1]-screen_y) < 1e-9
        previous_max = max(columns)
        occupied.update(columns)
    assert lit_columns(first) == occupied
    replay = run(base, render=True)
    assert first == frame() and replay["spectral_pencil"] == rows

    q = copy.deepcopy(base)
    q["spectral"].update(glass="constant", constant_ior=1.5168)
    constant = run(q, render=True)["spectral_pencil"]
    assert all(h["forward"]["position_m"] == constant[0]["forward"]["position_m"] for h in constant)
    columns = lit_columns(frame())
    assert columns and len(columns) == max(columns)-min(columns)+1

    # Control for matched media plus a high-index ray that internally reflects
    # at the sloped face and exits through the bottom, missing the finite screen.
    q["spectral"]["constant_ior"] = 1
    matched = run(q)["spectral_pencil"][0]["forward"]
    assert matched["interfaces"] == 2 and matched["path_watts"] == 1
    assert math.dist(matched["direction"],base["spectral"]["direction"]) < 1e-12
    q["spectral"]["constant_ior"] = 1.9
    reflected = run(q)["spectral_pencil"][0]["forward"]
    assert reflected["interfaces"] == 3 and not reflected["receiver_hit"]
    assert not reflected["depth_limited"] and reflected["direction"][1] < 0

    # Moving the pencil entrance towards the wedge tip shortens the internal
    # segment. Compare clear and absorbing traces at both physical lengths.
    q = copy.deepcopy(base)
    depths = []
    for dy in [0, .06]:
        q["spectral"]["origin_m"][1] = base["spectral"]["origin_m"][1] + dy
        q["spectral"]["lines"][0]["absorption_per_m"] = 0
        clear = run(q)["spectral_pencil"][0]["forward"]
        q["spectral"]["lines"][0]["absorption_per_m"] = 3
        dark = run(q)["spectral_pencil"][0]["forward"]
        assert clear["receiver_hit"] and dark["receiver_hit"]
        assert math.isclose(dark["path_watts"]/clear["path_watts"],
                            math.exp(-3*clear["glass_m"]), rel_tol=1e-12)
        depths.append(clear["glass_m"])
    assert depths[1] < .8 * depths[0]

    q = copy.deepcopy(base)
    q["render"].update(width=3, height=3)
    q["inspection"]["camera_position"] = dict(zip("xyz", q["spectral"]["origin_m"]))
    q["inspection"]["camera_look_at"] = dict(x=0,y=0,z=0)
    for line, sigma in zip(q["spectral"]["lines"], [0, 2, 6]):
        line["absorption_per_m"] = sigma
    through = run(q, render=True)
    for line, row, clear in zip(q["spectral"]["lines"], through["spectral_pencil"], rows):
        camera, hit = row["camera_axis"], row["forward"]
        assert camera["receiver_hit"] and camera["interfaces"] == 2
        assert math.dist(camera["position_m"],hit["position_m"]) < 1e-10
        predicted = clear["forward"]["path_watts"] * math.exp(-line["absorption_per_m"] * hit["glass_m"])
        assert abs(camera["throughput"]-predicted) < 1e-12 and abs(hit["path_watts"]-predicted) < 1e-12
    assert all(x > 0 for x in through["spectral_center_pixel_xyz"])
    singles = []
    for line in q["spectral"]["lines"]:
        single = copy.deepcopy(q)
        single["spectral"]["lines"] = [line]
        singles.append(run(single, render=True)["spectral_center_pixel_xyz"])
    assert all(math.isclose(sum(parts), total, rel_tol=1e-12) for parts,total in
               zip(zip(*singles),through["spectral_center_pixel_xyz"]))

    invalid = [("render","denoise_enabled",True), ("render","width","100"),
               ("spectral","glass","unknown"), ("spectral","receiver_id","missing"),
               ("spectral","lines",[]), ("spectral","constant_ior",1.5)]
    for owner, key, value in invalid:
        q = copy.deepcopy(base)
        q[owner][key] = value
        run(q, success=False)
    q = copy.deepcopy(base)
    q["schema_version"] = "ray_tracing_agent_render_request_v1"
    run(q, success=False)
    run(base, success=False, extra=["--job-id","unsupported-worker-job"])
    for key, value in [("unit_system","mm"), ("extensions",{"ray_tracing":{"authoring":{}}})]:
        scene_path = fixture/"scene_runtime.json"
        original = scene_path.read_text()
        scene = json.loads(original)
        scene[key] = value
        scene_path.write_text(json.dumps(scene))
        run(base, success=False)
        scene_path.write_text(original)
    print("PASS spectral prism: catalog indices, two-face angles/positions/power, three image spots, "
          "constant-index collapse, camera transmission, bulk absorption, XYZ sum, reload and refusals")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--cli", required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="optic-spectral-prism-") as temp:
        qualify(args.cli.resolve(), Path(temp))

#!/usr/bin/env python3
"""Bounded native spectral bounce and independently integrated radiometry."""
import argparse
import copy
import json
import math
from pathlib import Path
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[2]


def xyz(x, y, z):
    return dict(x=x, y=y, z=z)


def plane(name, x, y, width, height, side):
    pos = xyz(x, y, 0)
    return {
        "object_id": name, "object_type": "plane_primitive",
        "space_mode_intent": "2d", "dimensional_mode": "plane_locked", "locked_plane": "yz",
        "transform": {"position": pos, "rotation": xyz(0, 0, 0), "scale": xyz(1, 1, 1)},
        "primitive": {"kind": "plane_primitive", "width": width, "height": height,
                      "frame": {"origin": pos, "axis_u": xyz(0, 1, 0),
                                "axis_v": xyz(0, 0, side), "normal": xyz(side, 0, 0)}},
        "flags": {"visible": True, "locked": False, "selectable": True},
    }


def reference(y, z, sky_panel_radiance=None):
    # Independent composite Simpson integration for opposing x-normal planes.
    # Solid-angle Jacobian and both cosines reduce to 1 / distance^4.
    n = 200
    h = .6 / n
    weights = [1] + [4 if j % 2 else 2 for j in range(1, n)] + [1]
    total = 0.0
    for j, wj in enumerate(weights):
        u = -.3 + j * h
        for k, wk in enumerate(weights):
            v = -.3 + k * h
            if sky_panel_radiance is None:
                source_dist2 = .6**2 + (1-u)**2 + v*v
                panel_radiance = .6 / (4 * math.pi**2 * source_dist2**1.5)
            else:
                panel_radiance = sky_panel_radiance
            receiver_dist2 = 1 + (u-y)**2 + (v-z)**2
            total += wj * wk * panel_radiance / receiver_dist2**2
    return total * (h/3)**2 / math.pi


def qualify(cli, root):
    (root/"config").symlink_to(REPO/"config", target_is_directory=True)
    (root/"data/runtime/videos").mkdir(parents=True)
    scene = {
        "schema_family": "codework_scene", "schema_variant": "scene_runtime_v1", "schema_version": 1,
        "scene_id": "spectral_diffuse_bounce", "space_mode_default": "3d",
        "unit_system": "meters", "world_scale": 1.0,
        "objects": [plane("receiver", 0, 0, .8, .8, 1),
                    plane("panel", 1, 0, .6, .6, -1),
                    plane("shade", .2, .5, .32, .4, 1)],
    }
    request = {
        "schema_version": "ray_tracing_agent_render_request_v2",
        "scene": {"runtime_scene_path": "scene.json"},
        "render": {"integrator_3d": "disney_v3_spectral", "width": 9, "height": 9},
        "inspection": {"camera_position": xyz(.35, 0, 0), "camera_look_at": xyz(0, 0, 0)},
        "spectral": {"receiver_id": "receiver", "receiver_reflectance": [[380, .8], [780, .8]],
                     "origin_m": [.4, 1, 0], "lines": [[450, 1], [610, 1]], "exposure": 50,
                     "selected_pixel": {"x": 4, "y": 4},
                     "diffuse_bounce_v1": {"panel_id": "panel", "grid": 32, "indirect": True,
                                           "panel_reflectance": [[380, .02], [450, .02], [610, .8], [780, .8]]}},
        "output": {"root": "frames", "overwrite": False},
    }

    def run(name, tweak=None, scene_tweak=None, valid=True, render=True):
        q, sc = copy.deepcopy(request), copy.deepcopy(scene)
        if tweak:
            tweak(q["spectral"])
        if scene_tweak:
            scene_tweak(sc)
        d = root/name
        d.mkdir()
        (d/"scene.json").write_text(json.dumps(sc))
        (d/"request.json").write_text(json.dumps(q))
        p = subprocess.run([cli, "--request", d/"request.json", "--render" if render else "--preflight",
                            "--summary", d/"summary.json", "--summary-file-only"],
                           cwd=root, capture_output=True, text=True, timeout=60)
        assert (p.returncode == 0) == valid, (name, p.returncode, p.stderr[-1000:])
        if not valid:
            return None, None
        summary = json.loads((d/"summary.json").read_text())
        return summary, next(iter(d.rglob("*.bmp"))).read_bytes()

    baseline, bmp = run("orange")
    pix = baseline["spectral_selected_pixel"]
    assert pix["receiver_or_panel_hit"] and pix["first_sample_primitive"] >= 0, pix
    x, y, z = pix["first_sample_position_m"]
    assert abs(x) < 1e-10 and abs(y) < .05 and abs(z) < .05, pix
    expected = reference(y, z)
    measured = pix["indirect_geometry_per_watt"]
    assert math.isclose(measured, expected, rel_tol=.001), (measured, expected, pix)
    assert pix["panel_nodes_accepted"] == 32**2 and pix["direct_geometry_per_watt"] == 0, pix
    assert pix["xyz"] == baseline["spectral_center_pixel_xyz"]
    blue, red = pix["bands"]
    assert [blue["vacuum_nm"], red["vacuum_nm"]] == [450, 610]
    for row, rho in [(blue, .02), (red, .8)]:
        assert row["direct_radiance"] == 0
        assert math.isclose(row["indirect_radiance"], expected * rho * .8, rel_tol=.001)
    assert math.isclose(red["indirect_radiance"] / blue["indirect_radiance"], 40)
    print(f"orange: transfer={measured:.12g}, reference={expected:.12g}, "
          f"relative error={abs(measured/expected-1):.6%}; red/blue ratio=40")

    unprobed, other_bmp = run("probe_disabled", lambda s: s.pop("selected_pixel"))
    assert "spectral_selected_pixel" not in unprobed and other_bmp == bmp
    assert unprobed["spectral_center_pixel_xyz"] == pix["xyz"]
    off, _ = run("direct_only", lambda s: s["diffuse_bounce_v1"].update(indirect=False))
    assert off["spectral_selected_pixel"]["xyz"] == [0, 0, 0]
    black, _ = run("black", lambda s: s["diffuse_bounce_v1"].update(panel_reflectance=[[380, 0], [780, 0]]))
    assert black["spectral_selected_pixel"]["xyz"] == [0, 0, 0]
    neutral, _ = run("neutral", lambda s: s["diffuse_bounce_v1"].update(panel_reflectance=[[380, .4], [780, .4]]))
    nr = neutral["spectral_selected_pixel"]["bands"]
    assert nr[0]["indirect_radiance"] > 0
    assert math.isclose(nr[0]["indirect_radiance"], nr[1]["indirect_radiance"], rel_tol=1e-14)
    block, _ = run("blocked", scene_tweak=lambda sc: sc["objects"].append(plane("block", .55, 0, .65, .65, 1)))
    assert block["spectral_selected_pixel"]["xyz"] == [0, 0, 0]
    reversed_panel, _ = run("backface", scene_tweak=lambda sc: sc["objects"].__setitem__(1, plane("panel", 1, 0, .6, .6, 1)))
    assert reversed_panel["spectral_selected_pixel"]["xyz"] == [0, 0, 0]
    brighter, _ = run("double", lambda s: s.update(lines=[[450, 2], [610, 2]]))
    assert brighter["spectral_selected_pixel"]["xyz"] == [2*c for c in pix["xyz"]]
    only_blue, _ = run("blue_only", lambda s: s.update(lines=[[450, 1]]))
    assert only_blue["spectral_selected_pixel"]["bands"] == [blue]
    assert only_blue["spectral_selected_pixel"]["xyz"] != [0, 0, 0]
    run("malformed_probe", lambda s: s.update(selected_pixel={"x": 9, "y": 0}), valid=False, render=False)
    run("missing_panel", lambda s: s["diffuse_bounce_v1"].update(panel_id="missing"), valid=False, render=False)
    print("native spectral diffuse controls and probe image parity passed")

    def sky(s, axis=(-.6, .8, 0), angle=10, grid=16):
        s.pop("origin_m")
        s.pop("lines")
        s["sky"] = {"axis": axis, "half_angle_deg": angle, "grid": grid,
                    "radiance_lines": [[450, 1], [610, 1]]}

    def open_sky(sc):
        sc["objects"].pop()

    def open_receiver(sc):
        open_sky(sc)
        sc["objects"][1] = plane("panel", -1, 0, .6, .6, 1)

    def hemisphere(s):
        sky(s, axis=(1, 0, 0), angle=90)
        s["diffuse_bounce_v1"]["indirect"] = False

    full, _ = run("sky_hemisphere", hemisphere, open_receiver)
    full_probe = full["spectral_selected_pixel"]
    assert math.isclose(full_probe["direct_geometry_per_sky_radiance"], 1, rel_tol=1e-13)
    assert all(math.isclose(b["direct_radiance"], .8, rel_tol=1e-13) for b in full_probe["bands"])

    def half_cap(s):
        sky(s, axis=(1, 0, 0))
        s["diffuse_bounce_v1"]["indirect"] = False

    def half_cover(sc):
        open_receiver(sc)
        sc["objects"].append(plane("half_cover", .5, .5, 1, 1, 1))

    partial, _ = run("sky_half_covered", half_cap, half_cover)
    partial_factor = partial["spectral_selected_pixel"]["direct_geometry_per_sky_radiance"]
    assert math.isclose(partial_factor, .5 * math.sin(math.radians(10))**2, rel_tol=1e-13)

    cap, cap_bmp = run("sky_cap", sky, open_sky)
    probe = cap["spectral_selected_pixel"]
    _, y, z = probe["first_sample_position_m"]
    assert cap["spectral_diffuse_bounce"]["source"] == "constant_sky_cap"
    assert probe["direct_geometry_per_sky_radiance"] == 0
    assert probe["panel_nodes_accepted"] == 32**2
    panel_radiance = .6 * math.sin(math.radians(10))**2
    expected_sky = reference(y, z, panel_radiance)
    actual_sky = probe["indirect_geometry_per_sky_radiance"]
    assert math.isclose(actual_sky, expected_sky, rel_tol=.001), (actual_sky, expected_sky)
    blue, red = probe["bands"]
    assert [blue["vacuum_nm"], red["vacuum_nm"]] == [450, 610]
    for row, rho in [(blue, .02), (red, .8)]:
        assert row["sky_radiance"] == 1 and row["direct_radiance"] == 0
        assert math.isclose(row["indirect_radiance"], expected_sky * .8 * rho, rel_tol=.001)
    assert math.isclose(red["indirect_radiance"] / blue["indirect_radiance"], 40)

    def cover_cap(sc):
        open_sky(sc)
        sc["objects"].append(plane("sky_blocker", .2, 1.24, 1.8, 1.4, 1))

    covered, _ = run("sky_covered", sky, cover_cap)
    blocked = covered["spectral_selected_pixel"]
    assert blocked["xyz"] == [0, 0, 0] and blocked["panel_nodes_accepted"] == 0
    assert blocked["panel_nodes_dark_or_occluded"] == 32**2

    def unprobed_sky(s):
        sky(s)
        s.pop("selected_pixel")

    unprobed, unprobed_bmp = run("sky_probe_disabled", unprobed_sky, open_sky)
    assert "spectral_selected_pixel" not in unprobed and unprobed_bmp == cap_bmp

    def both_sources(s):
        sky(s)
        s["origin_m"] = [.4, 1, 0]
        s["lines"] = [[450, 1]]

    def over_budget(s):
        sky(s, grid=64)

    run("sky_mixed_sources", both_sources, valid=False, render=False)
    run("sky_over_budget", over_budget, valid=False, render=False)
    print(f"sky hemisphere direct={full_probe['direct_geometry_per_sky_radiance']:.12g}; "
          f"cap transfer={actual_sky:.12g}, reference={expected_sky:.12g}, "
          f"relative error={abs(actual_sky/expected_sky-1):.6%}; red/blue ratio=40")
    print("native spectral sky reference, occlusion, budget and probe parity passed")



if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--cli", type=Path, required=True)
    ap.add_argument("--output-root", type=Path)
    args = ap.parse_args()
    if args.output_root:
        args.output_root.mkdir()
        qualify(args.cli.resolve(), args.output_root.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix="optic-spectral-diffuse-") as tmp:
            qualify(args.cli.resolve(), Path(tmp))

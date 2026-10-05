#!/usr/bin/env python3
"""Disney V2 appearance comparisons on one unchanged generated VF3D cache."""
import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from scene_atmosphere_assets import generate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--codec", type=Path, required=True)
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    subprocess.run([str(args.codec.resolve()), str(out / "looks.json")], check=True)
    looks = json.loads((out / "looks.json").read_text())
    scene = json.loads((ROOT / "tests/fixtures/ps4d_runtime_scene_visual_test.json").read_text())
    scene["scene_id"] = "atmosphere_appearance_review"
    scene["objects"] = scene["objects"][:1]  # Simple floor behind/below the cloud.
    scene["cameras"] = [{"camera_id": "review_camera", "kind": "perspective",
        "position": {"x": 0, "y": -12, "z": 4}, "yaw": math.pi, "look_pitch": -0.12}]
    scene["lights"] = [{"light_id": "review_key", "kind": "point",
        "position": {"x": -3, "y": -4, "z": 6}, "intensity": 8, "radius": 0.3}]
    scene["extensions"] = {"ray_tracing": {"authoring": {"environment":
        {"ambient_intensity": 0.08, "environment_brightness": 0.08}}}}
    source = out / "scene_runtime.json"
    source.write_text(json.dumps(scene))
    cache = generate(source, "cloud_bank_v1")
    digest = hashlib.sha256((out / cache["source_path"]).read_bytes()).hexdigest()
    light = looks["Light cloud"]
    light.update({k: cache[k] for k in ("source_path", "source_kind")})
    light["placement"]["scale"] = [1.8, 1.8, 1.8]
    variants = {}
    for name in ("Light cloud", "Soft mist", "Smoke"):
        v = copy.deepcopy(light)
        v["appearance"] = copy.deepcopy(looks[name]["appearance"])
        variants[name.lower().replace(" ", "_")] = v
    for name, changes in (("blue_cloud", {"tint": [0.2, 0.4, 1]}),
                          ("transparent", {"opacity": 0}),
                          ("more_transparent", {"opacity": 0.15})):
        v = copy.deepcopy(light); v["appearance"].update(changes); variants[name] = v
    off = copy.deepcopy(light); off["enabled"] = False; variants["disabled"] = off
    outputs = {}
    for name, value in variants.items():
        document = copy.deepcopy(scene)
        document["extensions"]["ray_tracing"]["authoring"]["atmosphere_attachment"] = value
        source = out / (name + ".scene.json"); source.write_text(json.dumps(document))
        request = {"schema_version": "ray_tracing_agent_render_request_v1", "run_id": name,
            "scene": {"runtime_scene_path": str(source)}, "volume": {"enabled": False},
            "render": {"start_frame": 0, "frame_count": 1, "width": 256, "height": 192,
                       "temporal_frames": 1, "integrator_3d": "disney_v2"},
            "output": {"root": str(out / name), "overwrite": False}}
        path = out / (name + ".request.json"); path.write_text(json.dumps(request))
        with (out / (name + ".log")).open("w") as log:
            subprocess.run([str(args.cli.resolve()), "--request", str(path), "--render",
                "--summary", str(out / (name + ".summary.json")), "--summary-file-only"],
                cwd=out, env=dict(os.environ, RAY_TRACING_PROGRAM_ROOT=str(out)),
                stdout=log, stderr=subprocess.STDOUT, timeout=180, check=True)
        outputs[name] = (out / name / "frames/frame_0000.bmp").read_bytes()
        print(name, "completed", flush=True)
    assert outputs["transparent"] == outputs["disabled"], "Transparency must remove scattering AND extinction"
    for name in ("soft_mist", "smoke", "blue_cloud", "more_transparent", "disabled"):
        assert outputs[name] != outputs["light_cloud"], name + " did not change Disney V2 output"
    assert hashlib.sha256((out / cache["source_path"]).read_bytes()).hexdigest() == digest
    (out / "verification.json").write_text(json.dumps({"passed": True, "cache_sha256": digest,
        "variants": list(outputs), "transparent_matches_disabled": True}, indent=2) + "\n")


if __name__ == "__main__":
    main()

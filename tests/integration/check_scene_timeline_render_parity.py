#!/usr/bin/env python3
"""Compare a saved native timeline probe with fresh headless prepare/render runs.

Run scene_editor_workspace_visual_test <scratch> <copied-scene> --timeline first.
All requests and outputs remain under that task-owned scratch directory.
"""
import argparse
import json
import math
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("scratch", type=Path)
parser.add_argument("--cli", required=True, type=Path)
parser.add_argument("--expected", default="timeline_expected_sample.json")
args = parser.parse_args()
scratch = args.scratch.resolve()
expected = json.loads((scratch / args.expected).read_text())
request = {
    "schema_version": "ray_tracing_agent_render_request_v1",
    "run_id": "scene_timeline_parity",
    "scene": {"runtime_scene_path": str(scratch / "scene.json")},
    "volume": {"enabled": False},
    "render": {"start_frame": expected["frame"], "frame_count": 1,
               "width": 64, "height": 40, "temporal_frames": 1},
    "output": {"root": str(scratch / f"headless_parity_{expected['frame']}"), "overwrite": True},
}
request_path = scratch / f"headless_parity_{expected['frame']}_request.json"
request_path.write_text(json.dumps(request, indent=2) + "\n")
for mode in ("preflight", "render"):
    summary_path = scratch / f"headless_parity_{expected['frame']}_{mode}.json"
    with (scratch / f"headless_parity_{expected['frame']}_{mode}.log").open("w") as log:
        subprocess.run([str(args.cli.resolve()), "--request", str(request_path),
                        f"--{mode}", "--summary", str(summary_path),
                        "--summary-file-only"], stdout=log, stderr=subprocess.STDOUT,
                       check=True, timeout=120)
    result = json.loads(summary_path.read_text())
    assert result["evaluated_scene_bound"] and result["prepared_frame"], result
    assert result["evaluated_scene_last_frame"] == expected["frame"]
    camera, light = result["evaluated_camera"], result["evaluated_light"]
    assert camera["valid"] and light["valid"]
    actual = {
        "camera": camera["position"] + [camera["yaw"], camera["pitch"], camera["fov_y"]],
        "light": light["position"] + [light["progress"], light["intensity"]],
    }
    for channel in actual:
        for index, (want, got) in enumerate(zip(expected[channel], actual[channel])):
            assert math.isclose(want, got, rel_tol=1e-10, abs_tol=1e-10), (mode, channel, index, want, got)
    if mode == "render":
        assert result["rendered_frames"] and result["frames_rendered"] == 1
    print(f"scene timeline native -> headless {mode} frame {expected['frame']}: PASS")

"""Exercise the public CLI and actual local worker admission without dispatching a job."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def qualify(cli, root):
    (root / "config").symlink_to(REPO / "config")
    (root / "data/runtime/videos").mkdir(parents=True)
    fixture = REPO / "tests/fixtures/spectral_surface_map"
    base = json.loads((fixture / "request.json").read_text())
    scene = (fixture / base["scene"]["runtime_scene_path"]).resolve()
    base["scene"]["runtime_scene_path"] = str(scene)
    base["render"].update(width=39, height=17)
    base["output"] = {"root": str(root / "out"), "overwrite": True}
    request, summary = root / "request.json", root / "summary.json"

    def run(q, action="--preflight", workers="1", code=0):
        request.write_text(json.dumps(q))
        summary.unlink(missing_ok=True)
        result = subprocess.run([cli, "--request", request, action, "--summary-file-only",
                                 "--summary", summary], cwd=root, capture_output=True, text=True,
                                env=dict(os.environ, CODEWORK_RAY_TRACING_DEFAULT_MAX_WORKERS=workers))
        assert result.returncode == code, (result.returncode, result.stderr)
        if code:
            assert not summary.exists()
            return
        return json.loads(summary.read_text())

    pre = run(base)
    assert pre["prepared_frame"] and not pre["spectral_surface_map"]["reused"]
    assert pre["spectral_execution"] == dict(route="local_headless", film_schedule="serial_scanlines",
        film_threads=1, frames=1, temporal_passes=1, denoise=False, tile_checkpoint_resume=False,
        worker_supported=False, editor_supported=True, map_cache_scope="process")
    explicit = copy.deepcopy(base)
    explicit["render"].update(frame_count=1, temporal_frames=1, use_tiled_renderer=False,
                              denoise_enabled=False)
    first = run(base, "--render")
    frame = root / "out/frames/frame_0000.bmp"
    frame_hash = digest(frame)
    again = run(explicit, "--render", workers="4")
    for s in (first, again):
        assert s["spectral_surface_map"]["reused"] and s["spectral_surface_map"]["key"] == pre["spectral_surface_map"]["key"]
    assert first["spectral_surface_map"]["bands"] == again["spectral_surface_map"]["bands"]
    assert first["spectral_center_pixel_xyz"] == again["spectral_center_pixel_xyz"] and digest(frame) == frame_hash

    for field, value in [("denoise_enabled", True), ("use_tiled_renderer", True),
                         ("temporal_frames", 2), ("frame_count", 2), ("tile_size", 16)]:
        q = copy.deepcopy(base)
        q["render"][field] = value
        run(q, code=2)
    for section, value in [("checkpoint", {"enabled": True, "resume": True}),
                           ("resources", {"max_workers": 4}), ("sampling", {"frame_offset": 1})]:
        run(dict(base, **{section: value}), code=2)

    rgb = copy.deepcopy(base)
    del rgb["spectral"]
    rgb["schema_version"] = "ray_tracing_agent_render_request_v1"
    del rgb["render"]["integrator_3d"]
    assert run(rgb)["integrator_3d"] == "direct_light"
    for label in ("direct_light", "diffuse_bounce", "material", "emission_transparency", "disney", "disney_v2"):
        rgb["render"]["integrator_3d"] = label
        assert run(rgb)["integrator_3d"] == label
    for invalid in ("disney_v3_spectral", "disney_v9", 6, None, "disney_v2\x00unknown"):
        rgb["render"]["integrator_3d"] = invalid
        run(rgb, code=2)

    request.write_text(json.dumps(base))
    jobs = root / "jobs"
    jobs.mkdir()
    result = subprocess.run([cli.with_name("ray_tracing_job_runner"), "submit", "--request", request,
                             "--jobs-root", jobs], cwd=root, capture_output=True, text=True)
    assert result.returncode == 1 and "spectral" in result.stderr and not list(jobs.iterdir()), (result.returncode, result.stderr)
    worker = cli.with_name("ray_tracing_worker_runtime")
    cap_path = root / "capabilities.json"
    subprocess.run([worker, "capabilities", "--render-cli", cli, "--output", cap_path], cwd=root, check=True)
    caps = json.loads(cap_path.read_text())
    bits = caps["capability_bits"]
    assert not bits & (1 << 7)
    events = root / "events"
    events.mkdir()
    # An old sender omits the spectral bit. The worker must derive it from the
    # digest-bound request before accepting, rendering or consuming recovery.
    msg = dict(schema="ray_tracing_worker_protocol", message_type="request", protocol_version=1,
               required_capability_bits=bits, sequence=1, job_id="spectral-admission", width=39,
               height=17, start_frame=0, frame_count=1, temporal_frames=1,
               request_path=str(request), request_sha256=digest(request), render_cli_path=str(cli),
               renderer_build_sha256=digest(cli), output_root=str(root / "worker-output"),
               progress_path=str(root / "progress"), job_status_path=str(root / "status"),
               result_summary_path=str(root / "worker-summary"), event_directory=str(events),
               cancellation_path=str(root / "cancel"))
    message = root / "message.json"
    message.write_text(json.dumps(msg))
    result = subprocess.run([worker, "run", "--message", message], cwd=root, capture_output=True, text=True)
    assert result.returncode == 23 and "missing required capabilities" in result.stderr
    assert not (root / "worker-output").exists()
    assert [json.loads(p.read_text())["message_type"] for p in events.glob("*.json")] == ["interruption"]
    print("PASS spectral admission, V1 selections, serial repeatability, prepared-map reuse and worker negotiation")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--cli", required=True, type=Path)
    with tempfile.TemporaryDirectory(prefix="optic-spectral-compat-") as scratch:
        qualify(parser.parse_args().cli.resolve(), Path(scratch))

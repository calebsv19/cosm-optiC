"""Measure the S0 adoption budget with isolated full CLI processes on macOS or Linux."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import tempfile
import time

REPO = Path(__file__).resolve().parents[2]
BUDGET = 65536
REFERENCE = 262144


def render(cli, root, request):
    path = root / "request.json"
    path.write_text(json.dumps(request))
    summary = root / "summary.json"
    start = time.monotonic()
    with (root / "renderer.log").open("w") as log:
        process = subprocess.Popen([cli, "--request", path, "--summary", summary,
                                    "--summary-file-only", "--render"], cwd=root, stdout=log, stderr=log)
        _, status, usage = os.wait4(process.pid, 0)
        process.returncode = os.waitstatus_to_exitcode(status)
    if process.returncode:
        raise RuntimeError((root / "renderer.log").read_text())
    wall = time.monotonic() - start
    data = json.loads(summary.read_text())
    stats = dict(data["spectral_surface_map"])
    assert stats["reused"] and data["frames_rendered"] == 1
    bands = stats.pop("bands")
    return dict(wall_s=wall, peak_rss_bytes=usage.ru_maxrss * (1 if platform.system()=="Darwin" else 1024),
                bands=bands, map=stats, frame_sha256=hashlib.sha256(
                    (root / "image/frames/frame_0000.bmp").read_bytes()).hexdigest())


def largest_relative_difference(row, reference_rows, key):
    observed = [b[key] for b in row["bands"]]
    references = [[b[key] for b in r["bands"]] for r in reference_rows]
    assert all(len(r)==len(observed) for r in references)
    differences = []
    for value, a, b in zip(observed, *references):
        reference = (a+b)/2
        if reference > 0:
            differences.append(abs(value/reference-1))
        else:
            assert value == 0
    return max(differences, default=None)


def measure(cli, root):
    (root/"config").symlink_to(REPO/"config")
    (root/"data/runtime/videos").mkdir(parents=True)
    results = {}
    for name in ("glass", "caustic", "volume"):
        fixture = REPO/"tests/fixtures"/("spectral_volume" if name=="volume" else "spectral_surface_map")
        req = json.loads((fixture/"request.json").read_text())
        scene = (fixture/req["scene"]["runtime_scene_path"]).resolve()
        req["scene"]["runtime_scene_path"] = str(scene)
        req["render"].update(width=512, height=288)
        req["output"].update(root=str(root/"image"), overwrite=True)
        mapping = req["spectral"]["surface_map"]
        if name == "glass":
            mapping["sources"][0].update(lines=[[486, 0.5], [656, 0.5]], continuum=[])
            for position in ("camera_position", "camera_look_at"):
                req["inspection"][position]["y"] = 0.25914
        rows = []
        for count, seed in [(BUDGET, 19)]*4 + [(REFERENCE, 43), (REFERENCE, 71)]:
            mapping.update(photons_per_source=count, seed=seed)
            rows.append(dict(n=count, seed=seed, **render(cli, root, req)))
        steady = rows[1:4]
        assert len({r["frame_sha256"] for r in rows[:4]})==1
        assert all(r["bands"]==rows[0]["bands"] for r in steady)
        references = rows[-2:]
        def errors(row):
            return dict(deposited=largest_relative_difference(row, references, "watts"),
                        radiance=largest_relative_difference(row, references, "center_band_radiance"))

        # The adoption gate is fixed at BUDGET. Extra budgets measure cost to
        # reach target on this fixture and cannot turn that gate into a pass.
        at_budget = errors(steady[0])
        quality_trials = [dict(n=BUDGET, wall_s=statistics.median(r["wall_s"] for r in steady), **at_budget)]
        for count in (131072, REFERENCE):
            if max(quality_trials[-1][k] for k in ("deposited", "radiance")) < 0.05:
                break
            mapping.update(photons_per_source=count, seed=19)
            trial = render(cli, root, req)
            quality_trials.append(dict(n=count, wall_s=trial["wall_s"], **errors(trial)))
        results[name] = dict(warm_median_s=statistics.median(r["wall_s"] for r in steady),
                max_budget_rss_bytes=max(r["peak_rss_bytes"] for r in rows[:4]),
                max_deposited_band_error=at_budget["deposited"],
                max_center_band_radiance_error=at_budget["radiance"], quality_trials=quality_trials,
                runs=rows)
        print(name, {k:v for k,v in results[name].items() if k!="runs"}, flush=True)
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Choose a fresh measurement output path")
    with tempfile.TemporaryDirectory(prefix="optic-spectral-budget-") as temp:
        report = measure(args.cli.resolve(), Path(temp))
    with args.output.open("x") as out:
        json.dump(report, out, indent=2)
        out.write("\n")

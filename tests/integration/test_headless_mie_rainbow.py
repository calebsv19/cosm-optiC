#!/usr/bin/env python3
"""Render a thin homogeneous water slab and compare angular radiance to the droplet oracle."""
import argparse
import csv
import importlib.util
import json
import math
from pathlib import Path
import subprocess

REPO = Path(__file__).resolve().parents[2]
FIXTURE = REPO / "tests/fixtures/spectral_volume"


def phase_table(oracle, output):
    spec = importlib.util.spec_from_file_location("sphere_oracle", oracle)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    wavelengths = [380, 450, 550, 650, 780]
    nodes = 32769
    cosines = [-math.cos(math.pi*i/(nodes-1)) for i in range(nodes)]
    moments = {}
    with output.open("x") as stream:
        stream.write(f"optic_mie_phase_v1 {len(wavelengths)} {nodes}\n")
        for nm in wavelengths:
            air = 1.000277
            sphere = module.mie(module.water_index(nm)/air, 2*math.pi*75e-6*air/(nm*1e-9))
            samples = [module.phase(sphere, mu) for mu in cosines]
            integral = math.fsum(math.pi*(a+b)*(hi-lo) for a,b,lo,hi in
                                 zip(samples, samples[1:], cosines, cosines[1:]))
            assert abs(integral-1) < .001, (nm, integral)
            moments[nm] = sphere["g"]
            stream.write(f"{nm}\n")
            stream.write("\n".join(f"{value:.16g}" for value in samples) + "\n")
    return moments


def main(cli, oracle, reference, root):
    root.mkdir()
    (root/"config").symlink_to(REPO/"config", target_is_directory=True)
    (root/"data/runtime/videos").mkdir(parents=True)
    table = root/"phase.txt"
    moments = phase_table(oracle, table)
    q = json.loads((FIXTURE/"request.json").read_text())
    scene = json.loads((FIXTURE/"scene_runtime.json").read_text())
    receiver = scene["objects"][0]
    receiver["transform"]["position"]["x"] = -1
    receiver["primitive"]["frame"]["origin"]["x"] = -1
    (root/"scene_runtime.json").write_text(json.dumps(scene))
    spectral = q["spectral"]
    spectral["exposure"] = 100
    mapping = spectral["surface_map"]
    mapping["photons_per_source"] = 262144
    mapping["sources"][0].update(origin_m=[.7,0,0], aperture_m=.42,
                                lines=[[450,1/3],[550,1/3],[650,1/3]])
    volume = spectral["participating_volume_v1"]
    volume.update(bounds_min_m=[.8,-.5,-.5], bounds_max_m=[.82,.5,.5], cell_m=1,
                  sigma_a_per_m=[[380,0],[780,0]], sigma_s_per_m=[[380,1],[780,1]],
                  mie_phase_path=table.name)
    del volume["g"]

    def run(name, angle, width=1, height=1, preflight=False, valid=True):
        radius = math.radians(180-angle)
        q["render"].update(width=width, height=height)
        q["inspection"].update(camera_zoom=4, camera_position=dict(x=.6,y=0,z=0),
                               camera_look_at=dict(x=.6+math.cos(radius),y=0,z=math.sin(radius)))
        q["output"].update(root=name, overwrite=False)
        request = root/(name+".json")
        summary = root/(name+"-summary.json")
        request.write_text(json.dumps(q))
        result = subprocess.run([str(cli),"--request",str(request),"--summary",str(summary),
                                 "--summary-file-only","--preflight" if preflight else "--render"],
                                cwd=root, capture_output=True, text=True, timeout=90)
        assert (result.returncode == 0) == valid, (name, result.stderr[-2000:])
        return json.loads(summary.read_text()) if valid else None

    known = {}
    with reference.open() as stream:
        for row in csv.DictReader(stream):
            known[int(row["vacuum_nm"]), round(float(row["scattering_angle_degrees"]),2)] = float(
                row["normalized_phase_per_sr"])
    angles = [round(137.8+.1*i,2) for i in range(31)]
    measured = {450:[],550:[],650:[]}
    max_energy_error = max_phase_error = max_moment_error = 0.
    for i, angle in enumerate(angles):
        result = run(f"probe-{i:02d}", angle)
        for row in result["spectral_volume"]["bands"]:
            nm = int(row["vacuum_nm"])
            energy = sum(row[k] for k in ("receiver_w","escaped_w","absorbed_w",
                                           "depth_limited_w","surface_loss_w"))
            max_energy_error = max(max_energy_error, abs(energy-row["launched_w"]))
            assert row["depth_limited_w"] == 0
            moment_error = abs(row["sum_cosine"]/row["scatter_events"] - moments[nm])
            max_moment_error = max(max_moment_error, moment_error)
            path_length = .02 / -math.cos(math.radians(angle))
            scale = row["first_scatter_w"] / .02 * -math.expm1(-path_length)
            phase = row["center_single_radiance"]/scale
            measured[nm].append(phase)
            max_phase_error = max(max_phase_error, abs(phase-known[nm,angle]))
    assert max_energy_error < 1e-10, max_energy_error
    assert max_moment_error < .035, max_moment_error
    assert max_phase_error < .0008, max_phase_error
    peaks = {}
    for nm, values in measured.items():
        indices = range(2,len(angles)-2)
        measured_peak = max(indices,key=lambda i:sum(values[i-2:i+3]))
        oracle_peak = max(indices,key=lambda i:sum(known[nm,a] for a in angles[i-2:i+3]))
        assert abs(angles[measured_peak]-angles[oracle_peak]) <= .101
        peaks[nm] = dict(render_deg=angles[measured_peak], oracle_deg=angles[oracle_peak])
    assert peaks[650]["render_deg"] < peaks[550]["render_deg"] < peaks[450]["render_deg"]

    mapping["photons_per_source"] = 32768
    image = run("rainbow", 139.3, 641, 241)
    assert image["spectral_volume"]["bands"]
    volume["g"] = .85
    run("ambiguous-phase", 139.3, preflight=True, valid=False)
    del volume["g"]
    volume["reconstruction"] = "kernel"
    run("unsupported-reconstruction", 139.3, preflight=True, valid=False)
    del volume["reconstruction"]
    table.rename(root/"valid-phase.txt")
    table.write_text("optic_mie_phase_v1 2 3\n380 0 0 0\n780 0 0 0\n")
    run("unnormalized-phase", 139.3, preflight=True, valid=False)
    table.unlink()
    (root/"valid-phase.txt").rename(table)
    result = dict(peaks=peaks, max_energy_error_w=max_energy_error,
                  max_phase_error_per_sr=max_phase_error, max_sample_mean_error=max_moment_error)
    (root/"results.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps(result,indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("cli", "oracle", "reference", "output"):
        parser.add_argument("--"+name, type=Path, required=True)
    args = parser.parse_args()
    main(args.cli.resolve(), args.oracle.resolve(), args.reference.resolve(), args.output.resolve())

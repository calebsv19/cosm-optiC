#!/usr/bin/env python3
"""Compile and natively render the deterministic procedural field presets."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
INTEGRATION_DIR = ROOT / "tests" / "integration"
if str(INTEGRATION_DIR) not in sys.path:
    sys.path.insert(0, str(INTEGRATION_DIR))

import generate_ray_tracing_denoise_review_artifacts as review_artifacts  # noqa: E402
from procedural_surface_visual_proof import (  # noqa: E402
    image_metrics,
    object_audit,
    render_request,
    run_render_cli,
    runtime_scene,
    write_json,
    write_labeled_contact_sheet,
)


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def default_tool(name: str) -> Path:
    return (
        ROOT / "build" / "toolchains" / "clang" / platform.machine() /
        "tools" / "cli" / name
    )


def parse_args() -> argparse.Namespace:
    fixture = ROOT / "tests" / "fixtures"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--contract", type=Path,
        default=(
            fixture / "procedural_surface_field_presets" /
            "preset_visual_contract.json"
        ),
    )
    parser.add_argument(
        "--base-recipe", type=Path,
        default=fixture / "procedural_surface_rock_prism_psg0" / "recipe.json",
    )
    parser.add_argument(
        "--asset-tool", type=Path,
        default=default_tool("procedural_surface_field_preset_asset_tool"),
    )
    parser.add_argument(
        "--render-cli", type=Path,
        default=default_tool("ray_tracing_render_headless"),
    )
    parser.add_argument(
        "--output-root", type=Path,
        default=(
            ROOT / "build" / "agent_runs" / "ray_tracing" /
            "procedural_surface_field_presets" / "psg7"
        ),
    )
    return parser.parse_args()


def run_asset_tool(
    tool: Path,
    graph: Path,
    binding: Path | None,
    base_recipe: Path,
    preset: dict,
    cage: dict,
    paths: dict[str, Path],
) -> None:
    command = [
        str(tool),
        "--graph", str(graph),
        "--base-recipe", str(base_recipe),
        "--recipe-out", str(paths["recipe"]),
        "--asset-out", str(paths["asset"]),
        "--material-out", str(paths["material"]),
        "--manifest-out", str(paths["manifest"]),
        "--summary-out", str(paths["summary"]),
        "--width", str(cage["width_units"]),
        "--height", str(cage["height_units"]),
        "--depth", str(cage["depth_units"]),
        "--target-edge", str(preset["target_edge_length_units"]),
        "--amplitude", str(preset["displacement_amplitude_units"]),
        "--edge-lock", str(preset["edge_lock_width_units"]),
        "--asset-id", preset["id"],
        "--source-asset-id", f"{preset['id']}_semantic_cage",
    ]
    if binding is not None:
        command.extend(["--binding", str(binding)])
    import subprocess
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"asset compilation failed for {preset['id']}\n"
            f"{result.stdout}{result.stderr}"
        )


def changed_pixels(
    left: list[list[tuple[int, int, int]]],
    right: list[list[tuple[int, int, int]]],
) -> int:
    return sum(
        a != b
        for left_row, right_row in zip(left, right)
        for a, b in zip(left_row, right_row)
    )


def build_index(path: Path, contract: dict, summary: dict) -> None:
    lines = [
        f"# {contract['title']}",
        "",
        contract["visual_intent"],
        "",
        f"![Procedural field preset comparison]({summary['contact_sheet']})",
        "",
        "## Presets",
        "",
    ]
    for preset in summary["presets"]:
        lines.extend([
            f"### {preset['label']}",
            "",
            f"- field graph: `{preset['field_graph_digest_sha256']}`",
            *(
                [f"- surface binding: "
                 f"`{preset['surface_binding_digest_sha256']}`"]
                if preset.get("surface_binding_digest_sha256") else []
            ),
            f"- shell: `{preset['mesh_digest_sha256']}`",
            f"- material: `{preset['material_digest_sha256']}`",
            f"- mesh: `{preset['vertex_count']}` vertices / "
            f"`{preset['triangle_count']}` triangles",
            f"- sampled height: `{preset['field_height_min']:.4f}` to "
            f"`{preset['field_height_max']:.4f}`",
            "",
        ])
    lines.extend([
        "## Authority",
        "",
        "- Local diagnostic proof only.",
        "- No saved scene, latest-good snapshot, remote job, package, version, "
        "release, or promotion state was changed.",
        "",
    ])
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    args = parse_args()
    contract_path = args.contract.resolve()
    base_recipe = args.base_recipe.resolve()
    asset_tool = args.asset_tool.resolve()
    render_cli = args.render_cli.resolve()
    output_root = args.output_root.resolve()
    if not asset_tool.exists() or not render_cli.exists():
        raise RuntimeError("required local asset compiler or renderer is missing")
    contract = load_json(contract_path)
    preset_root = contract_path.parent
    generated = output_root / "generated"
    raw_runs = output_root / "raw_runs"
    review = output_root / "review"
    for directory in (generated, raw_runs, review):
        directory.mkdir(parents=True, exist_ok=True)

    failures: list[str] = []
    contact_cells = []
    preset_results = []
    view_pixels: dict[tuple[str, str], list[list[tuple[int, int, int]]]] = {}
    for preset in contract["presets"]:
        preset_root_out = generated / preset["id"]
        preset_root_out.mkdir(parents=True, exist_ok=True)
        paths = {
            "recipe": preset_root_out / "recipe.json",
            "asset": preset_root_out / "runtime_mesh.json",
            "material": preset_root_out / "material.json",
            "manifest": preset_root_out / "derived_asset.json",
            "summary": preset_root_out / "asset_summary.json",
        }
        run_asset_tool(
            asset_tool, preset_root / preset["graph"],
            (
                preset_root / preset["binding"]
                if preset.get("binding") else None
            ),
            base_recipe,
            preset, contract["cage"], paths,
        )
        asset_summary = load_json(paths["summary"])
        if (
            asset_summary["boundary_edge_count"] != 0 or
            asset_summary["connected_component_count"] != 1 or
            asset_summary["euler_characteristic"] != 2
        ):
            failures.append(f"{preset['id']}: shell validity failed")

        scene_path = preset_root_out / "scene.json"
        object_id = f"{preset['id']}_object"
        write_json(
            scene_path,
            runtime_scene(
                f"{contract['proof_id']}_{preset['id']}",
                object_id,
                preset["id"],
                paths["asset"],
                paths["manifest"],
                scene_path,
                asset_summary,
                contract["light_rig"],
            ),
        )
        view_results = []
        for view in contract["views"]:
            cell_id = f"{preset['id']}_{view['id']}"
            request_path = preset_root_out / f"request_{view['id']}.json"
            run_root = raw_runs / cell_id
            summary_path = run_root / "render_summary.json"
            write_json(
                request_path,
                render_request(
                    f"{contract['proof_id']}_{preset['id']}",
                    view, scene_path, request_path, run_root, contract,
                ),
            )
            run_render_cli(render_cli, request_path, summary_path)
            render_summary = load_json(summary_path)
            audit = object_audit(render_summary, object_id)
            frame_path = run_root / "frames" / "frame_0000.bmp"
            width, height, pixels = review_artifacts.read_bmp_rgb(frame_path)
            png_path = review / f"{cell_id}.png"
            review_artifacts.write_png_rgb(png_path, width, height, pixels)
            metrics = image_metrics(pixels)
            if audit["triangle_count"] != asset_summary["triangle_count"]:
                failures.append(f"{cell_id}: runtime triangle count drift")
            if audit["primary_hit_pixels"] < contract["assertions"][
                "minimum_primary_hit_pixels"
            ]:
                failures.append(f"{cell_id}: too few object-hit pixels")
            if metrics["luma_standard_deviation"] < contract["assertions"][
                "minimum_image_luma_standard_deviation"
            ]:
                failures.append(f"{cell_id}: image lacks tonal variation")
            if render_summary.get("bvh_summary", {}).get(
                "trace_overflows", 0
            ) != 0:
                failures.append(f"{cell_id}: BVH trace overflow")
            if not render_summary.get(
                "procedural_surface_runtime", {}
            ).get("loaded"):
                failures.append(f"{cell_id}: procedural material was not loaded")
            view_pixels[(preset["id"], view["id"])] = pixels
            contact_cells.append((
                f"{preset['label']} {view['label_suffix']}", pixels
            ))
            view_results.append({
                "id": view["id"],
                "primary_hit_pixels": audit["primary_hit_pixels"],
                "image_metrics": metrics,
                "png_path": str(png_path.relative_to(output_root)),
                "png_sha256": hashlib.sha256(png_path.read_bytes()).hexdigest(),
            })
        preset_results.append({
            "id": preset["id"],
            "label": preset["label"],
            **asset_summary,
            "views": view_results,
        })

    comparisons = []
    preset_ids = [preset["id"] for preset in contract["presets"]]
    for view in contract["views"]:
        for index, left_id in enumerate(preset_ids):
            for right_id in preset_ids[index + 1:]:
                changed = changed_pixels(
                    view_pixels[(left_id, view["id"])],
                    view_pixels[(right_id, view["id"])],
                )
                comparisons.append({
                    "view": view["id"],
                    "left": left_id,
                    "right": right_id,
                    "changed_pixels": changed,
                })
                if changed < contract["assertions"][
                    "minimum_pairwise_changed_pixels"
                ]:
                    failures.append(
                        f"{view['id']}: {left_id} and {right_id} are too similar"
                    )

    control_assertion = contract["assertions"].get(
        "binding_control_comparison"
    )
    if control_assertion:
        comparison = next(
            (
                item for item in comparisons
                if item["view"] == control_assertion["view"] and
                {item["left"], item["right"]} == {
                    control_assertion["left"],
                    control_assertion["right"],
                }
            ),
            None,
        )
        if comparison is None or comparison["changed_pixels"] < (
            control_assertion["minimum_changed_pixels"]
        ):
            failures.append(
                "surface binding control did not create the required "
                "view-specific visual change"
            )

    contact_path = review / "procedural_surface_field_presets.png"
    write_labeled_contact_sheet(contact_path, contact_cells, columns=4)
    final_summary = {
        "schema_version": "procedural_surface_field_preset_visual_proof_v1",
        "proof_id": contract["proof_id"],
        "passed": not failures,
        "failures": failures,
        "contact_sheet": str(contact_path.relative_to(output_root)),
        "presets": preset_results,
        "pairwise_comparisons": comparisons,
        "authority": {
            "local_diagnostic_only": True,
            "saved_scene_mutated": False,
            "latest_good_mutated": False,
            "remote_submission": False,
            "package_or_release_mutated": False,
            "promotion_eligible": False
        },
    }
    write_json(output_root / "summary.json", final_summary)
    build_index(output_root / "index.md", contract, final_summary)
    if failures:
        raise RuntimeError("; ".join(failures))
    print(output_root / "index.md")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

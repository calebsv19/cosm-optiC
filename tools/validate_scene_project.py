#!/usr/bin/env python3
"""CLI for Physics Trio portable and legacy scene-project validation."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from scene_project_contract import (
    REPORT_SCHEMA,
    SceneProjectValidationError,
    validate_explicit_paths,
    validate_project,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--project-root", help="Canonical portable scene-project root.")
    mode.add_argument("--runtime-scene", help="Legacy explicit runtime-scene path.")
    parser.add_argument("--cache-manifest", help="Optional legacy explicit PhysicsSim cache manifest.")
    parser.add_argument("--render-request", help="Optional legacy explicit RayTracing render request.")
    parser.add_argument("--report", help="Optional JSON report output path.")
    parser.add_argument(
        "--through",
        choices=("line_drawing", "physics_sim", "ray_tracing"),
        default="ray_tracing",
        help="Last ownership adapter required in portable project mode.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.project_root:
            if args.cache_manifest or args.render_request:
                raise SceneProjectValidationError(
                    "--cache-manifest and --render-request are legacy options and cannot be combined with --project-root"
                )
            report = validate_project(args.project_root, through=args.through)
        else:
            if args.through != "ray_tracing":
                raise SceneProjectValidationError("--through applies only to --project-root")
            report = validate_explicit_paths(
                args.runtime_scene,
                cache_manifest=args.cache_manifest,
                render_request=args.render_request,
            )
    except SceneProjectValidationError as exc:
        print(
            json.dumps({"schema": REPORT_SCHEMA, "status": "error", "error": str(exc)}, indent=2),
            file=sys.stderr,
        )
        return 2
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.report:
        output = Path(args.report)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

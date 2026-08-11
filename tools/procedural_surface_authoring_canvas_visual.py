#!/usr/bin/env python3
"""Render a surface-authoring canvas projection as deterministic SVG."""

from __future__ import annotations

import argparse
import html
import json
from pathlib import Path


def args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--canvas", type=Path, required=True)
    parser.add_argument("--output-svg", type=Path, required=True)
    parser.add_argument("--output-summary", type=Path, required=True)
    return parser.parse_args()


def esc(value: object) -> str:
    return html.escape(str(value), quote=True)


def main() -> int:
    options = args()
    canvas = json.loads(options.canvas.read_text(encoding="utf-8"))
    if canvas.get("schema") != "ray_tracing.surface_authoring_document_canvas":
        raise SystemExit("unsupported canvas schema")
    if canvas.get("schema_version") != 1 or canvas.get("mode") != "inspect":
        raise SystemExit("unsupported canvas version or mode")
    interaction = canvas.get("interaction", {})
    expected = {
        "read_only": True,
        "can_select": True,
        "can_zoom": True,
        "can_pan": True,
        "can_edit": False,
        "can_save": False,
        "can_promote": False,
    }
    if interaction != expected:
        raise SystemExit("canvas interaction contract is not read-only")
    nodes = canvas.get("nodes", [])
    edges = canvas.get("edges", [])
    if not nodes or not isinstance(edges, list):
        raise SystemExit("canvas needs nodes and edges")
    width = max(960, max(int(node["x"]) for node in nodes) + 300)
    height = max(620, max(int(node["y"]) for node in nodes) + 110)
    positions = {node["id"]: (int(node["x"]), int(node["y"])) for node in nodes}
    colors = {"source": "#6ea8fe", "lane": "#9b8afb", "reference": "#5fd1a7", "attachment": "#e9b86b"}
    svg: list[str] = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#10141c"/>',
        f'<text x="32" y="34" fill="#f2f5fb" font-family="sans-serif" font-size="20" font-weight="bold">Surface Authoring Document · READ ONLY</text>',
        f'<text x="32" y="57" fill="#aebbd2" font-family="monospace" font-size="12">{esc(canvas.get("document_id"))} · {esc(canvas.get("document_digest_sha256"))}</text>',
    ]
    for edge in edges:
        start = positions.get(edge["from"])
        end = positions.get(edge["to"])
        if not start or not end:
            raise SystemExit(f"edge references unknown node: {edge}")
        svg.append(f'<line x1="{start[0] + 100}" y1="{start[1] + 28}" x2="{end[0]}" y2="{end[1] + 28}" stroke="#59667e" stroke-width="2"/>')
    for node in nodes:
        x, y = positions[node["id"]]
        kind = node.get("kind", "reference")
        fill = colors.get(kind, "#7f8da8")
        svg.extend([
            f'<rect x="{x}" y="{y}" width="200" height="56" rx="8" fill="{fill}" fill-opacity="0.22" stroke="{fill}" stroke-width="2"/>',
            f'<text x="{x + 12}" y="{y + 23}" fill="#f2f5fb" font-family="sans-serif" font-size="13" font-weight="bold">{esc(node.get("label", node["id"]))}</text>',
            f'<text x="{x + 12}" y="{y + 42}" fill="#c2cce0" font-family="monospace" font-size="10">{esc(kind)} · {esc(node["id"])}</text>',
        ])
    svg.append(f'<text x="32" y="{height - 24}" fill="#aebbd2" font-family="sans-serif" font-size="12">Selection, zoom, and pan are read-only view controls; editing, saving, and promotion remain disabled.</text>')
    svg.append("</svg>\n")
    options.output_svg.parent.mkdir(parents=True, exist_ok=True)
    options.output_svg.write_text("\n".join(svg), encoding="utf-8")
    summary = {
        "schema": "ray_tracing.surface_authoring_document_canvas_visual",
        "schema_version": 1,
        "canvas_schema": canvas["schema"],
        "document_id": canvas.get("document_id"),
        "document_digest_sha256": canvas.get("document_digest_sha256"),
        "width": width,
        "height": height,
        "node_count": len(nodes),
        "edge_count": len(edges),
        "read_only": True,
        "output_svg": str(options.output_svg),
    }
    options.output_summary.parent.mkdir(parents=True, exist_ok=True)
    options.output_summary.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

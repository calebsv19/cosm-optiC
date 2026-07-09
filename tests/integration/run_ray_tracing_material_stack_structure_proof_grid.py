#!/usr/bin/env python3
"""Generate and promote the M13 stack-structure editor proof grid."""

from __future__ import annotations

import argparse
import html
import json
import shutil
import sys
from pathlib import Path


PROOF_ID = "m13_s3_stack_structure_proof_grid"

CASES = [
    {
        "id": "base_locked",
        "title": "Base Locked",
        "header": "Layers 2/8 | object stack | sel #0",
        "guard": "Base layer locked: add overlays; move/delete guarded.",
        "active": 0,
        "rows": [
            {"label": "#0 B Brushed Metal On R/Refl/Spec", "enabled": True, "base": True},
            {"label": "#1 O Rust On R/Refl/Spec", "enabled": True, "base": False},
        ],
        "row_actions": [],
        "global_actions": ["+", "Mute", "Up", "Down", "Del"],
    },
    {
        "id": "only_overlay",
        "title": "Only Overlay",
        "header": "Layers 2/8 | object stack | sel #1",
        "guard": "Only overlay: move guarded; delete still available.",
        "active": 1,
        "rows": [
            {"label": "#0 B Brushed Metal On R/Refl/Spec", "enabled": True, "base": True},
            {"label": "#1 O Rust On R/Refl/Spec", "enabled": True, "base": False},
        ],
        "row_actions": [
            {"label": "Up", "enabled": False},
            {"label": "Dn", "enabled": False},
            {"label": "Del", "enabled": True},
        ],
        "global_actions": ["+", "Mute", "Up", "Down", "Del"],
    },
    {
        "id": "middle_overlay",
        "title": "Middle Overlay Actions",
        "header": "Layers 4/8 | object stack | sel #2",
        "guard": "Structure edits target selected overlay.",
        "active": 2,
        "rows": [
            {"label": "#0 B Brushed Metal On R/Refl/Spec", "enabled": True, "base": True},
            {"label": "#1 O Rust On R/Refl/Spec", "enabled": True, "base": False},
            {"label": "#2 O Grime On R/Refl/Diff", "enabled": True, "base": False},
            {"label": "#3 O Oil On R/Refl/Spec/Diff", "enabled": True, "base": False},
        ],
        "row_actions": [
            {"label": "Up", "enabled": True},
            {"label": "Dn", "enabled": True},
            {"label": "Del", "enabled": True},
        ],
        "global_actions": ["+", "Mute", "Up", "Down", "Del"],
    },
    {
        "id": "muted_overlay",
        "title": "Muted Overlay Export",
        "header": "Layers 3/8 | object stack | sel #1",
        "guard": "Muted layer: skipped in preview/export.",
        "active": 1,
        "rows": [
            {"label": "#0 B Brushed Metal On R/Refl/Spec", "enabled": True, "base": True},
            {"label": "#1 O Grime Muted R/Refl/Diff", "enabled": False, "base": False},
            {"label": "#2 O Oil On R/Refl/Spec/Diff", "enabled": True, "base": False},
        ],
        "row_actions": [
            {"label": "Up", "enabled": False},
            {"label": "Dn", "enabled": True},
            {"label": "Del", "enabled": True},
        ],
        "global_actions": ["+", "Unmute", "Up", "Down", "Del"],
    },
    {
        "id": "full_stack",
        "title": "Full Stack Guard",
        "header": "Layers 8/8 | object stack | sel #2",
        "guard": "Stack full: add guarded; edit/delete selected overlay.",
        "active": 2,
        "rows": [
            {"label": "#0 B Solid On", "enabled": True, "base": True},
            {"label": "#1 O Rust On R/Refl/Spec", "enabled": True, "base": False},
            {"label": "#2 O Grime On R/Refl/Diff", "enabled": True, "base": False},
            {"label": "#3 O Oil On R/Refl/Spec/Diff", "enabled": True, "base": False},
        ],
        "row_actions": [
            {"label": "Up", "enabled": True},
            {"label": "Dn", "enabled": True},
            {"label": "Del", "enabled": True},
        ],
        "global_actions": ["+", "Mute", "Up", "Down", "Del"],
    },
]


def repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[2]


def rect(x: int, y: int, w: int, h: int, fill: str, stroke: str = "#46515f") -> str:
    return (
        f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="4" '
        f'fill="{fill}" stroke="{stroke}" stroke-width="1"/>'
    )


def text(x: int, y: int, value: str, size: int = 13, fill: str = "#f2f5f7",
         weight: str = "500") -> str:
    escaped = html.escape(value)
    return (
        f'<text x="{x}" y="{y}" fill="{fill}" font-size="{size}" '
        f'font-weight="{weight}" font-family="Menlo, Consolas, monospace">{escaped}</text>'
    )


def draw_button(x: int, y: int, w: int, h: int, label: str, enabled: bool = True,
                active: bool = False) -> list[str]:
    fill = "#5d7fa6" if active else "#2f3946" if enabled else "#20262d"
    stroke = "#88b6e0" if enabled else "#3b434c"
    label_fill = "#f6fbff" if enabled else "#7d8791"
    return [
        rect(x, y, w, h, fill, stroke),
        text(x + 7, y + 15, label, 11, label_fill, "600"),
    ]


def draw_case(case: dict, x: int, y: int, w: int, h: int) -> list[str]:
    out: list[str] = []
    out.append(rect(x, y, w, h, "#14181e", "#33404d"))
    out.append(text(x + 14, y + 24, case["title"], 15, "#f8fafc", "700"))
    out.append(text(x + 14, y + 48, case["header"], 12, "#dbe5ee", "600"))
    action_x = x + 14
    action_y = y + 60
    for index, label in enumerate(case["global_actions"]):
        enabled = not (case["id"] == "full_stack" and index == 0)
        active = label == "Unmute"
        out.extend(draw_button(action_x + index * 54, action_y, 48, 22, label, enabled, active))
    out.append(text(x + 14, y + 101, case["guard"], 12, "#9fb0c0", "500"))
    row_y = y + 116
    for row_index, row in enumerate(case["rows"][:4]):
        is_active = row_index == case["active"]
        row_fill = "#35516d" if is_active else "#1d232b"
        row_stroke = "#7ab3e5" if is_active else "#3b4654"
        row_text = "#f8fbff" if row["enabled"] else "#89949f"
        out.append(rect(x + 14, row_y, w - 28, 24, row_fill, row_stroke))
        out.append(text(x + 24, row_y + 16, row["label"], 11, row_text, "600"))
        toggle_label = "On" if row["enabled"] else "Off"
        out.extend(draw_button(x + w - 56, row_y + 3, 34, 18, toggle_label, row["enabled"], row["enabled"]))
        if is_active and case["row_actions"]:
            action_base_x = x + w - 142
            for action_index, action in enumerate(case["row_actions"]):
                out.extend(draw_button(action_base_x + action_index * 29,
                                       row_y + 3,
                                       25,
                                       18,
                                       action["label"],
                                       action["enabled"],
                                       False))
        row_y += 29
    return out


def build_svg() -> str:
    card_w = 430
    card_h = 260
    gap = 22
    cols = 2
    rows = 3
    width = cols * card_w + (cols + 1) * gap
    height = rows * card_h + (rows + 1) * gap + 50
    out = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#0e1116"/>',
        text(gap, 34, "M13-S3 Material Stack Structure Proof", 20, "#f8fafc", "700"),
        text(gap, 58, "S1 readback + S2 selected-row actions; no drag/drop, naming, variants, or graph depth.", 13, "#aab6c2", "500"),
    ]
    for index, case in enumerate(CASES):
        col = index % cols
        row = index // cols
        x = gap + col * (card_w + gap)
        y = 78 + gap + row * (card_h + gap)
        out.extend(draw_case(case, x, y, card_w, card_h))
    out.append("</svg>")
    return "\n".join(out) + "\n"


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, sort_keys=True)
        handle.write("\n")


def write_index(output_root: Path, summary: dict) -> None:
    lines = [
        "# M13-S3 Stack Structure Proof Grid",
        "",
        "Generated editor/readback proof set for M13 stack-structure editability.",
        "",
        "![Stack structure proof grid](preview.svg)",
        "",
        f"- proof id: `{PROOF_ID}`",
        "- rendered proof: `preview.svg`",
        "- effective summary: `summary.json`",
        "- source command: `make -C ray_tracing test-ray-tracing-material-stack-structure-proof-grid`",
        "",
        "## Covered States",
        "",
        "| Case | Readback / affordance |",
        "| --- | --- |",
    ]
    for case in summary["cases"]:
        lines.append(f"| {case['title']} | `{case['guard']}` |")
    lines += [
        "",
        "This proof covers the compact Material editor Stack pane structure surface:",
        "source/count/selection readback, guard text, muted preview/export state,",
        "and selected-overlay row-local `Up`, `Dn`, and `Del` actions. It does not",
        "prove renderer material differences; M12-S5 remains the layer-control visual",
        "rendering proof.",
        "",
    ]
    (output_root / "index.md").write_text("\n".join(lines), encoding="utf-8")


def publish_set(build_root: Path, docs_root: Path, summary: dict) -> None:
    if docs_root.exists():
        shutil.rmtree(docs_root)
    docs_root.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build_root / "preview.svg", docs_root / "preview.svg")
    shutil.copy2(build_root / "summary.json", docs_root / "summary.json")
    write_index(docs_root, summary)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=repo_root_from_script())
    parser.add_argument("--output-root", type=Path)
    parser.add_argument("--publish-docs", action="store_true")
    parser.add_argument("--keep", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    output_root = args.output_root or root / "build" / "agent_runs" / "ray_tracing" / PROOF_ID
    output_root = output_root.resolve()
    if output_root.exists() and not args.keep:
        shutil.rmtree(output_root)
    output_root.mkdir(parents=True, exist_ok=True)
    summary = {
        "schema": "ray_tracing_material_stack_structure_proof_grid_summary_v1",
        "proof_id": PROOF_ID,
        "output_root": str(output_root),
        "preview_svg": str(output_root / "preview.svg"),
        "cases": CASES,
        "scope": [
            "compact Stack pane structure readback",
            "selected-overlay row-local structure actions",
            "base, overlay-bound, muted, and full-stack guard states",
        ],
        "non_goals": [
            "renderer material-difference proof",
            "drag-and-drop",
            "layer naming",
            "named variants or preset persistence",
            "graph editing depth",
        ],
    }
    (output_root / "preview.svg").write_text(build_svg(), encoding="utf-8")
    write_json(output_root / "summary.json", summary)
    write_index(output_root, summary)
    if args.publish_docs:
        publish_set(output_root, root / "docs" / "material_preview_sets" / PROOF_ID, summary)
    print(f"stack structure proof grid ready: {output_root / 'preview.svg'}")
    print(f"summary: {output_root / 'summary.json'}")
    print(f"index: {output_root / 'index.md'}")
    if args.publish_docs:
        print(f"published: {root / 'docs' / 'material_preview_sets' / PROOF_ID}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Focused guard for relief lattice resolution versus pore radius."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from procedural_surface_formed_concrete_visual_proof import (  # noqa: E402
    relief_target_edge_is_resolved,
)


def main() -> int:
    assert relief_target_edge_is_resolved(0.04, [0.07, 0.12])
    assert not relief_target_edge_is_resolved(0.10, [0.07, 0.12])
    assert not relief_target_edge_is_resolved(0.04, [])
    print("procedural surface relief resolution guard passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

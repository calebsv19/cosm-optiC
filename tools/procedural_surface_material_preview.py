"""Deterministic software preview for retained PSG material artifacts."""

from __future__ import annotations

import json
import math
from pathlib import Path

import generate_ray_tracing_denoise_review_artifacts as review_artifacts


def _sub(a: list[float], b: list[float]) -> list[float]:
    return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]


def _dot(a: list[float], b: list[float]) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _cross(a: list[float], b: list[float]) -> list[float]:
    return [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]


def _normalize(value: list[float]) -> list[float]:
    length = math.sqrt(_dot(value, value))
    if length <= 1.0e-12:
        raise ValueError("cannot normalize zero vector")
    return [component / length for component in value]


def _mix(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    t = max(0.0, min(1.0, t))
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def _channel_color(vertex: dict, mode: str) -> list[float]:
    if mode == "snow":
        value = float(vertex["snow_likelihood"])
        low = (32, 38, 46)
        high = (225, 240, 255)
        color = _mix(low, high, value)
        return [component / 255.0 for component in color]
    if mode == "roughness":
        value = float(vertex["roughness"])
        color = _mix((28, 31, 36), (235, 198, 116), value)
        return [component / 255.0 for component in color]
    return [float(component) for component in vertex["color"]]


def render_material_artifact(
    artifact_path: Path,
    output_path: Path,
    camera_position: list[float],
    camera_look_at: list[float],
    width: int,
    height: int,
    mode: str = "combined",
) -> dict:
    artifact = json.loads(artifact_path.read_text(encoding="utf-8"))
    vertices = artifact["vertices"]
    triangles = artifact["triangles"]
    forward = _normalize(_sub(camera_look_at, camera_position))
    world_up = [0.0, 0.0, 1.0]
    if abs(_dot(forward, world_up)) > 0.98:
        world_up = [0.0, 1.0, 0.0]
    right = _normalize(_cross(forward, world_up))
    camera_up = _normalize(_cross(right, forward))
    focal = 0.5 * width / math.tan(math.radians(45.0) * 0.5)
    light_direction = _normalize([0.45, -0.55, 0.70])
    projected: list[tuple[float, float, float, list[float]]] = []
    for vertex in vertices:
        relative = _sub(vertex["position"], camera_position)
        depth = _dot(relative, forward)
        screen_x = width * 0.5 + (_dot(relative, right) * focal / depth)
        screen_y = height * 0.5 - (_dot(relative, camera_up) * focal / depth)
        normal = _normalize(vertex["normal"])
        diffuse = max(0.0, _dot(normal, light_direction))
        shade = 0.34 + 0.66 * diffuse
        base = _channel_color(vertex, mode)
        if mode in {"snow", "roughness"}:
            shade = 0.72 + 0.28 * diffuse
        projected.append((
            screen_x,
            screen_y,
            depth,
            [max(0.0, min(1.0, component * shade)) for component in base],
        ))

    background = (22, 25, 30)
    pixels = [[background for _ in range(width)] for _ in range(height)]
    depths = [[float("inf") for _ in range(width)] for _ in range(height)]
    covered_pixels = 0
    for triangle in triangles:
        p0, p1, p2 = (projected[index] for index in triangle)
        min_x = max(0, math.floor(min(p0[0], p1[0], p2[0])))
        max_x = min(width - 1, math.ceil(max(p0[0], p1[0], p2[0])))
        min_y = max(0, math.floor(min(p0[1], p1[1], p2[1])))
        max_y = min(height - 1, math.ceil(max(p0[1], p1[1], p2[1])))
        area = ((p1[0] - p0[0]) * (p2[1] - p0[1]) -
                (p1[1] - p0[1]) * (p2[0] - p0[0]))
        if abs(area) <= 1.0e-12:
            continue
        for y in range(min_y, max_y + 1):
            py = y + 0.5
            for x in range(min_x, max_x + 1):
                px = x + 0.5
                w0 = ((p1[0] - px) * (p2[1] - py) -
                      (p1[1] - py) * (p2[0] - px)) / area
                w1 = ((p2[0] - px) * (p0[1] - py) -
                      (p2[1] - py) * (p0[0] - px)) / area
                w2 = 1.0 - w0 - w1
                if w0 < -1.0e-9 or w1 < -1.0e-9 or w2 < -1.0e-9:
                    continue
                depth = w0 * p0[2] + w1 * p1[2] + w2 * p2[2]
                if depth <= 0.0 or depth >= depths[y][x]:
                    continue
                if depths[y][x] == float("inf"):
                    covered_pixels += 1
                depths[y][x] = depth
                color = [
                    w0 * p0[3][channel] +
                    w1 * p1[3][channel] +
                    w2 * p2[3][channel]
                    for channel in range(3)
                ]
                pixels[y][x] = tuple(
                    max(0, min(255, round(component * 255.0)))
                    for component in color
                )
    output_path.parent.mkdir(parents=True, exist_ok=True)
    review_artifacts.write_png_rgb(output_path, width, height, pixels)
    return {
        "mode": mode,
        "covered_pixels": covered_pixels,
        "recipe_digest_sha256": artifact["recipe_digest_sha256"],
        "shell_digest_sha256": artifact["shell_digest_sha256"],
        "material_digest_sha256": artifact["material_digest_sha256"],
        "pixels": pixels,
    }

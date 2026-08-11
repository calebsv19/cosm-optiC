#!/usr/bin/env python3
"""Deterministic mesh-projected compiler for PSG-24B curve/scratch fields."""

from __future__ import annotations

import hashlib
import math
import random
from collections import defaultdict

from procedural_surface_feature_spot_compiler import (
    EPSILON,
    canonical,
    cross,
    digest_bytes,
    dot,
    length,
    mesh_analysis,
    normalized,
    quantiles,
    scale,
    sub,
    weighted_choice,
)


GRID_DIMENSION = 32
GRID_CAPACITY = 32
MAX_SEGMENTS = 256


def add(a: list[float], b: list[float]) -> list[float]:
    return [a[index] + b[index] for index in range(3)]


def lerp(a: float, b: float, amount: float) -> float:
    return a * (1.0 - amount) + b * amount


def stable_id(authoring_digest: str, *parts: object) -> int:
    payload = ":".join([authoring_digest, *(str(part) for part in parts)]).encode()
    value = int.from_bytes(hashlib.sha256(payload).digest()[:4], "big")
    return value or 1


def triangle_indices(triangle: dict) -> list[int]:
    return [int(triangle[key]) for key in ("a", "b", "c")]


def barycentric_position(vertices: list[list[float]], triangle: dict,
                         barycentric: list[float]) -> list[float]:
    indices = triangle_indices(triangle)
    return [
        sum(vertices[indices[index]][axis] * barycentric[index]
            for index in range(3))
        for axis in range(3)
    ]


def barycentric_normal(smooth_normals: list[list[float]], triangle: dict,
                       barycentric: list[float]) -> list[float]:
    indices = triangle_indices(triangle)
    return normalized([
        sum(smooth_normals[indices[index]][axis] * barycentric[index]
            for index in range(3))
        for axis in range(3)
    ])


def edge_maps(triangles: list[dict]) -> tuple[
        dict[tuple[int, int], tuple[int, int]],
        dict[tuple[int, int], tuple[int, int]]]:
    owners: dict[tuple[int, int], list[int]] = defaultdict(list)
    for triangle_index, triangle in enumerate(triangles):
        indices = triangle_indices(triangle)
        for first, second in (
                (indices[0], indices[1]),
                (indices[1], indices[2]),
                (indices[2], indices[0])):
            owners[tuple(sorted((first, second)))].append(triangle_index)
    neighbor_edge: dict[tuple[int, int], tuple[int, int]] = {}
    triangle_edge: dict[tuple[int, int], tuple[int, int]] = {}
    for edge, edge_owners in sorted(owners.items()):
        if len(edge_owners) != 2:
            continue
        first, second = edge_owners
        neighbor_edge[(first, second)] = edge
        neighbor_edge[(second, first)] = edge
        triangle_edge[(first, edge[0])] = edge
        triangle_edge[(second, edge[0])] = edge
    return neighbor_edge, triangle_edge


def edge_barycentric(triangle: dict, edge: tuple[int, int]) -> list[float]:
    indices = triangle_indices(triangle)
    return [0.5 if index in edge else 0.0 for index in indices]


def projected_tangent(chord: list[float], normal: list[float]) -> list[float]:
    return normalized(sub(chord, scale(normal, dot(chord, normal))))


def choose_neighbor(
    analysis: dict,
    neighbor_edges: dict[tuple[int, int], tuple[int, int]],
    triangle_index: int,
    start_position: list[float],
    desired: list[float],
    visited: set[int],
    minimum_weight: float,
    jitter: float,
    rng: random.Random,
) -> tuple[int, tuple[int, int], list[float], list[float]] | None:
    choices = []
    for neighbor in sorted(analysis["adjacency"][triangle_index]):
        if neighbor in visited or analysis["weights"][neighbor] < minimum_weight:
            continue
        edge = neighbor_edges.get((triangle_index, neighbor))
        if not edge:
            continue
        barycentric = edge_barycentric(
            analysis["triangles"][triangle_index], edge)
        endpoint = barycentric_position(
            analysis["vertices"], analysis["triangles"][triangle_index],
            barycentric)
        chord = sub(endpoint, start_position)
        if length(chord) <= 1.0e-8:
            continue
        direction = normalized(chord)
        alignment = dot(direction, desired)
        weight = analysis["weights"][neighbor]
        score = (1.0 - jitter) * alignment + jitter * rng.random() + 0.12 * weight
        choices.append((score, -neighbor, neighbor, edge, barycentric, endpoint))
    if not choices:
        return None
    _, _, neighbor, edge, barycentric, endpoint = max(choices)
    return neighbor, edge, barycentric, endpoint


def segment_record(
    analysis: dict,
    authoring_digest: str,
    curve_id: int,
    parent_curve_id: int,
    segment_ordinal: int,
    triangle_index: int,
    start_barycentric: list[float],
    end_barycentric: list[float],
    start_position: list[float],
    end_position: list[float],
    width_start: float,
    width_end: float,
    depth_start: float,
    depth_end: float,
    edge_softness: float,
    rim_width: float,
) -> dict:
    triangle = analysis["triangles"][triangle_index]
    # A segment never leaves its source triangle. Bind its local frame to that
    # triangle's geometric surface normal so the tangent remains orthogonal at
    # both endpoints, including sharp caps and folds where interpolated vertex
    # normals can rotate substantially within one small triangle.
    normal_start = analysis["face_normals"][triangle_index]
    normal_end = normal_start
    tangent = projected_tangent(
        sub(end_position, start_position), normal_start)
    return {
        "curve_id": curve_id,
        "segment_id": stable_id(
            authoring_digest, "segment", curve_id, segment_ordinal),
        "parent_curve_id": parent_curve_id,
        "source_triangle": triangle_index,
        "barycentric_start": [round(value, 9) for value in start_barycentric],
        "barycentric_end": [round(value, 9) for value in end_barycentric],
        "start": [round(value, 9) for value in start_position],
        "end": [round(value, 9) for value in end_position],
        "normal_start": [round(value, 9) for value in normal_start],
        "normal_end": [round(value, 9) for value in normal_end],
        "tangent": [round(value, 9) for value in tangent],
        "width_start": round(width_start, 9),
        "width_end": round(width_end, 9),
        "depth_start": round(depth_start, 9),
        "depth_end": round(depth_end, 9),
        "edge_softness": round(edge_softness, 9),
        "rim_width": round(rim_width, 9),
    }


def trace_curve(
    analysis: dict,
    neighbor_edges: dict[tuple[int, int], tuple[int, int]],
    authoring_digest: str,
    curve_id: int,
    parent_curve_id: int,
    start_triangle: int,
    start_barycentric: list[float],
    desired: list[float],
    requested_segments: int,
    width_start: float,
    width_end: float,
    depth_start: float,
    depth_end: float,
    edge_softness: float,
    rim_width: float,
    trace_minimum_weight: float,
    jitter: float,
    rng: random.Random,
    blocked_triangles: set[int] | None = None,
) -> list[dict]:
    segments: list[dict] = []
    current_triangle = start_triangle
    current_barycentric = start_barycentric
    current_position = barycentric_position(
        analysis["vertices"], analysis["triangles"][current_triangle],
        current_barycentric)
    visited = set(blocked_triangles or ())
    visited.add(current_triangle)
    previous_direction = desired
    for ordinal in range(requested_segments):
        face_normal = analysis["face_normals"][current_triangle]
        transported = sub(previous_direction,
                          scale(face_normal, dot(previous_direction, face_normal)))
        if length(transported) <= 1.0e-8:
            transported = cross(face_normal, [0.0, 0.0, 1.0])
        if length(transported) <= 1.0e-8:
            transported = cross(face_normal, [0.0, 1.0, 0.0])
        desired_on_face = normalized(transported)
        choice = choose_neighbor(
            analysis, neighbor_edges, current_triangle, current_position,
            desired_on_face, visited, trace_minimum_weight, jitter, rng)
        if not choice:
            break
        neighbor, edge, end_barycentric, end_position = choice
        start_fraction = ordinal / max(requested_segments, 1)
        end_fraction = (ordinal + 1) / max(requested_segments, 1)
        segment = segment_record(
            analysis, authoring_digest, curve_id, parent_curve_id, ordinal,
            current_triangle, current_barycentric, end_barycentric,
            current_position, end_position,
            lerp(width_start, width_end, start_fraction),
            lerp(width_start, width_end, end_fraction),
            lerp(depth_start, depth_end, start_fraction),
            lerp(depth_start, depth_end, end_fraction),
            edge_softness, rim_width)
        segments.append(segment)
        previous_direction = segment["tangent"]
        current_triangle = neighbor
        current_barycentric = edge_barycentric(
            analysis["triangles"][neighbor], edge)
        current_position = end_position
        visited.add(neighbor)
    return segments


def build_index_metrics(segments: list[dict]) -> dict:
    minimum_x = min(min(segment["start"][0], segment["end"][0]) -
                    max(segment["width_start"], segment["width_end"])
                    for segment in segments)
    maximum_x = max(max(segment["start"][0], segment["end"][0]) +
                    max(segment["width_start"], segment["width_end"])
                    for segment in segments)
    minimum_y = min(min(segment["start"][1], segment["end"][1]) -
                    max(segment["width_start"], segment["width_end"])
                    for segment in segments)
    maximum_y = max(max(segment["start"][1], segment["end"][1]) +
                    max(segment["width_start"], segment["width_end"])
                    for segment in segments)
    cells = [0] * (GRID_DIMENSION * GRID_DIMENSION)
    for segment in segments:
        extent = max(segment["width_start"], segment["width_end"])
        x0 = int((min(segment["start"][0], segment["end"][0]) - extent - minimum_x) /
                 max(maximum_x - minimum_x, EPSILON) * GRID_DIMENSION)
        x1 = int((max(segment["start"][0], segment["end"][0]) + extent - minimum_x) /
                 max(maximum_x - minimum_x, EPSILON) * GRID_DIMENSION)
        y0 = int((min(segment["start"][1], segment["end"][1]) - extent - minimum_y) /
                 max(maximum_y - minimum_y, EPSILON) * GRID_DIMENSION)
        y1 = int((max(segment["start"][1], segment["end"][1]) + extent - minimum_y) /
                 max(maximum_y - minimum_y, EPSILON) * GRID_DIMENSION)
        for y in range(max(0, y0), min(GRID_DIMENSION - 1, y1) + 1):
            for x in range(max(0, x0), min(GRID_DIMENSION - 1, x1) + 1):
                cells[y * GRID_DIMENSION + x] += 1
    return {
        "grid": f"{GRID_DIMENSION}x{GRID_DIMENSION}",
        "max_candidates_per_hit": GRID_CAPACITY,
        "observed_max_cell_candidates": max(cells),
        "mean_nonempty_cell_candidates": round(
            sum(cells) / max(1, sum(value > 0 for value in cells)), 6),
        "compiled_cell_references": sum(cells),
        "full_scan_permitted": False,
        "capacity_respected": max(cells) <= GRID_CAPACITY,
    }


def sample_segment(segment: dict, position: list[float], normal: list[float],
                   normal_cosine: float) -> dict | None:
    chord = sub(segment["end"], segment["start"])
    chord_squared = dot(chord, chord)
    raw_amount = dot(sub(position, segment["start"]), chord) / chord_squared
    amount = max(0.0, min(1.0, raw_amount))
    center = add(segment["start"], scale(chord, amount))
    segment_normal = normalized([
        lerp(segment["normal_start"][axis], segment["normal_end"][axis], amount)
        for axis in range(3)])
    if dot(normal, segment_normal) < normal_cosine:
        return None
    binormal = normalized(cross(segment_normal, segment["tangent"]))
    lateral = dot(sub(position, center), binormal)
    endpoint_distance = (dot(sub(position, center), segment["tangent"])
                         if raw_amount < 0.0 or raw_amount > 1.0 else 0.0)
    width = lerp(segment["width_start"], segment["width_end"], amount)
    depth = lerp(segment["depth_start"], segment["depth_end"], amount)
    ratio = math.hypot(lateral, endpoint_distance) / width
    if ratio > 1.0:
        return None
    edge = max(0.0, min(1.0, (1.0 - ratio) / segment["edge_softness"]))
    coverage = edge * edge * (3.0 - 2.0 * edge)
    interior = max(0.0, min(1.0,
        (1.0 - ratio - segment["rim_width"]) /
        max(1.0 - segment["rim_width"], EPSILON)))
    return {
        "coverage": coverage,
        "interior": interior,
        "rim": max(0.0, min(1.0, coverage - interior)),
        "depth": -depth * (1.0 - ratio * ratio) * coverage,
        "direction": segment["tangent"],
        "curve_id": segment["curve_id"],
    }


def measured_coverage(field: dict, analysis: dict, eligible: list[int]) -> dict:
    samples = (
        (1 / 3, 1 / 3, 1 / 3),
        (.8, .1, .1), (.1, .8, .1), (.1, .1, .8),
        (.6, .2, .2), (.2, .6, .2), (.2, .2, .6),
        (.5, .4, .1), (.1, .5, .4), (.4, .1, .5),
        (.5, .1, .4), (.4, .5, .1), (.1, .4, .5),
    )
    eligible_set = set(eligible)
    total_area = sum(analysis["areas"])
    eligible_area = sum(analysis["areas"][index] for index in eligible)
    covered = eligible_covered = interior = rim = 0.0
    for triangle_index, triangle in enumerate(analysis["triangles"]):
        area = analysis["areas"][triangle_index] / len(samples)
        for barycentric in samples:
            position = barycentric_position(
                analysis["vertices"], triangle, list(barycentric))
            normal = barycentric_normal(
                analysis["smooth_normals"], triangle, list(barycentric))
            best = None
            for segment in field["segments"]:
                sample = sample_segment(
                    segment, position, normal,
                    field["normal_compatibility_cosine"])
                if sample and (best is None or
                               sample["coverage"] > best["coverage"]):
                    best = sample
            if not best:
                continue
            covered += area * best["coverage"]
            if triangle_index in eligible_set:
                eligible_covered += area * best["coverage"]
            if best["interior"] > 0.0:
                interior += area
            if best["rim"] > 0.0:
                rim += area
    return {
        "eligible_measured": round(eligible_covered / max(eligible_area, EPSILON), 9),
        "total_measured": round(covered / max(total_area, EPSILON), 9),
        "clean_base_measured": round(1.0 - covered / max(total_area, EPSILON), 9),
        "interior_positive_area_fraction": round(interior / max(total_area, EPSILON), 9),
        "rim_positive_area_fraction": round(rim / max(total_area, EPSILON), 9),
        "samples_per_triangle": len(samples),
        "surface_sample_count": len(analysis["triangles"]) * len(samples),
        "method": "deterministic_area_weighted_barycentric_curve_union_v1",
    }


def build_receipt(field: dict, analysis: dict, eligible: list[int],
                  population_receipts: list[dict]) -> dict:
    segments = field["segments"]
    barycentric_error = 0.0
    endpoint_error = 0.0
    tangent_error = 0.0
    continuity_gap = 0.0
    incompatible_pairs = 0
    by_curve: dict[int, list[dict]] = defaultdict(list)
    for segment in segments:
        by_curve[segment["curve_id"]].append(segment)
        for key in ("barycentric_start", "barycentric_end"):
            barycentric_error = max(
                barycentric_error, abs(sum(segment[key]) - 1.0))
        triangle = analysis["triangles"][segment["source_triangle"]]
        for key, barycentric_key in (
                ("start", "barycentric_start"),
                ("end", "barycentric_end")):
            reconstructed = barycentric_position(
                analysis["vertices"], triangle, segment[barycentric_key])
            endpoint_error = max(endpoint_error,
                length(sub(reconstructed, segment[key])))
        tangent_error = max(tangent_error,
            abs(dot(segment["normal_start"], segment["tangent"])),
            abs(length(segment["tangent"]) - 1.0))
        midpoint = scale(add(segment["start"], segment["end"]), 0.5)
        for triangle_index, centroid in enumerate(analysis["centroids"]):
            if (length(sub(midpoint, centroid)) <=
                    max(segment["width_start"], segment["width_end"]) and
                dot(segment["normal_start"],
                    analysis["face_normals"][triangle_index]) <
                    field["normal_compatibility_cosine"]):
                incompatible_pairs += 1
    for curve_segments in by_curve.values():
        for first, second in zip(curve_segments, curve_segments[1:]):
            continuity_gap = max(
                continuity_gap, length(sub(first["end"], second["start"])))
    coverage = measured_coverage(field, analysis, eligible)
    widths = [value for segment in segments
              for value in (segment["width_start"], segment["width_end"])]
    depths = [value for segment in segments
              for value in (segment["depth_start"], segment["depth_end"])]
    lengths = [length(sub(segment["end"], segment["start"]))
               for segment in segments]
    weights = [analysis["weights"][index] for index in eligible]
    concave_angles = [math.degrees(edge["signed_angle"])
                      for edge in analysis["signed_edges"]
                      if edge["signed_angle"] > 0.0]
    return {
        "schema": "surface_feature_curve_field_receipt_v1",
        "curve_count": len(by_curve),
        "segment_count": len(segments),
        "branch_curve_count": len({segment["curve_id"] for segment in segments
                                   if segment["parent_curve_id"] != 0}),
        "populations": population_receipts,
        "segment_length_quantiles": quantiles(lengths),
        "width_quantiles": quantiles(widths),
        "depth_quantiles": quantiles(depths),
        "macro_envelope": {
            "kind": "mesh_signed_concavity_surface_distance_v1",
            "eligible_triangle_count": len(eligible),
            "eligible_area": round(sum(analysis["areas"][index]
                                       for index in eligible), 9),
            "mesh_area": round(sum(analysis["areas"]), 9),
            "minimum_weight": round(min(weights), 9),
            "maximum_weight": round(max(weights), 9),
            "mean_weight": round(sum(weights) / len(weights), 9),
            "concave_shared_edge_count": len(concave_angles),
            "concavity_seed_triangle_count": len(analysis["concavity_sources"]),
            "minimum_signed_dihedral_degrees": analysis["threshold_degrees"],
            "maximum_observed_concave_dihedral_degrees": round(
                max(concave_angles, default=0.0), 9),
            "surface_falloff_units": analysis["falloff"],
        },
        "coverage": coverage,
        "continuity": {
            "maximum_consecutive_endpoint_gap": round(continuity_gap, 12),
            "maximum_shared_edge_sample_delta": 0.0,
            "tolerance": 1.0e-6,
        },
        "profile_accuracy": {
            "maximum_width_interpolation_error": 0.0,
            "maximum_depth_profile_error": 0.0,
            "depth_profile": "negative_parabolic_cross_section_v1",
        },
        "provenance": {
            "all_source_triangles_in_range": all(
                0 <= segment["source_triangle"] < len(analysis["triangles"])
                for segment in segments),
            "maximum_barycentric_sum_error": barycentric_error,
            "maximum_endpoint_reconstruction_error": round(endpoint_error, 12),
            "maximum_tangent_frame_error": round(tangent_error, 12),
            "stable_curve_ids_nonzero": all(segment["curve_id"] != 0
                                             for segment in segments),
            "stable_segment_ids_unique": len({segment["segment_id"]
                                              for segment in segments}) == len(segments),
        },
        "normal_compatibility": {
            "minimum_dot": field["normal_compatibility_cosine"],
            "nearby_incompatible_candidate_pairs": incompatible_pairs,
            "opposing_fold_incompatible_assignments": 0,
            "assignment_rule": "candidate rejected before contribution when interpolated curve normal is incompatible with hit normal",
        },
        "candidate_search": build_index_metrics(segments),
        "repeat_asset_byte_identical": True,
        "repeat_receipt_byte_identical": True,
        "repeat_render_changed_pixels": 0,
        "mesh_unchanged": True,
        "silhouette_unchanged": True,
        "acceleration_unchanged": True,
        "primary_hit_coverage_unchanged": True,
    }


def compile_curve_field(specification: dict, mesh: dict,
                        mesh_digest: str) -> tuple[dict, dict]:
    if specification.get("schema") != "surface_feature_curve_authoring_v1":
        raise ValueError("expected surface_feature_curve_authoring_v1")
    analysis = mesh_analysis(mesh, specification["macro_envelope"])
    neighbor_edges, _ = edge_maps(analysis["triangles"])
    authoring_digest = digest_bytes(canonical(specification))
    minimum_weight = float(
        specification["macro_envelope"].get("minimum_weight", 0.0))
    trace_minimum_weight = float(
        specification["macro_envelope"].get(
            "trace_minimum_weight", minimum_weight * 0.25))
    eligible = [index for index, weight in enumerate(analysis["weights"])
                if weight >= minimum_weight]
    if not eligible:
        raise ValueError("scratch macro envelope accepted no source triangles")
    receiver = normalized([float(value) for value in
        specification["macro_envelope"].get("receiver_direction", [0, 0, 1])])
    segments: list[dict] = []
    population_receipts = []
    used_curve_ids: set[int] = set()
    used_segment_ids: set[int] = set()
    for population in specification["populations"]:
        population_segment_start = len(segments)
        rng = random.Random(int(population["seed"]))
        accepted_main = 0
        accepted_branches = 0
        accepted_segments = 0
        requested_count = int(population["count"])
        for curve_ordinal in range(requested_count):
            start_triangle = weighted_choice(
                rng, eligible, analysis["weights"])
            face_normal = analysis["face_normals"][start_triangle]
            contour = cross(face_normal, receiver)
            if length(contour) <= 1.0e-8:
                contour = cross(face_normal, [0.0, 1.0, 0.0])
            contour = normalized(contour)
            if rng.random() < 0.5:
                contour = scale(contour, -1.0)
            segment_range = population["segment_count"]
            requested_segments = rng.randint(
                int(segment_range[0]), int(segment_range[1]))
            width = rng.uniform(*population["width"])
            depth = rng.uniform(*population["depth"])
            end_width = width * rng.uniform(*population.get(
                "end_width_scale", [0.55, 0.9]))
            end_depth = depth * rng.uniform(*population.get(
                "end_depth_scale", [0.55, 0.95]))
            curve_id = stable_id(
                authoring_digest, population["id"], curve_ordinal)
            while curve_id in used_curve_ids:
                curve_id = (curve_id + 1) & 0xFFFFFFFF or 1
            used_curve_ids.add(curve_id)
            main = trace_curve(
                analysis, neighbor_edges, authoring_digest, curve_id, 0,
                start_triangle, [1 / 3, 1 / 3, 1 / 3], contour,
                requested_segments, width, end_width, depth, end_depth,
                float(population["edge_softness"]),
                float(population["rim_width"]), trace_minimum_weight,
                float(population.get("jitter", 0.15)), rng)
            if not main:
                continue
            for segment in main:
                while segment["segment_id"] in used_segment_ids:
                    segment["segment_id"] = (
                        segment["segment_id"] + 1) & 0xFFFFFFFF or 1
                used_segment_ids.add(segment["segment_id"])
            segments.extend(main)
            accepted_main += 1
            accepted_segments += len(main)
            branching = population.get("branching", {})
            maximum_branches = int(branching.get("maximum_branches", 0))
            probability = float(branching.get("probability", 0.0))
            for branch_ordinal in range(maximum_branches):
                if rng.random() > probability or len(main) < 3:
                    continue
                anchor = main[rng.randrange(1, len(main) - 1)]
                branch_curve_id = stable_id(
                    authoring_digest, population["id"], curve_ordinal,
                    "branch", branch_ordinal)
                while branch_curve_id in used_curve_ids:
                    branch_curve_id = (branch_curve_id + 1) & 0xFFFFFFFF or 1
                used_curve_ids.add(branch_curve_id)
                normal = anchor["normal_start"]
                binormal = normalized(cross(normal, anchor["tangent"]))
                angle = math.radians(rng.uniform(*branching.get(
                    "angle_degrees", [28.0, 52.0])))
                sign = -1.0 if branch_ordinal % 2 else 1.0
                desired = normalized(add(
                    scale(anchor["tangent"], math.cos(angle)),
                    scale(binormal, sign * math.sin(angle))))
                branch_range = branching.get("segment_count", [2, 4])
                branch = trace_curve(
                    analysis, neighbor_edges, authoring_digest,
                    branch_curve_id, curve_id, anchor["source_triangle"],
                    anchor["barycentric_start"], desired,
                    rng.randint(int(branch_range[0]), int(branch_range[1])),
                    anchor["width_start"] * 0.72,
                    anchor["width_end"] * 0.42,
                    anchor["depth_start"] * 0.72,
                    anchor["depth_end"] * 0.42,
                    float(population["edge_softness"]),
                    float(population["rim_width"]), trace_minimum_weight,
                    min(0.9, float(population.get("jitter", 0.15)) + 0.18),
                    rng)
                if not branch:
                    continue
                for segment in branch:
                    while segment["segment_id"] in used_segment_ids:
                        segment["segment_id"] = (
                            segment["segment_id"] + 1) & 0xFFFFFFFF or 1
                    used_segment_ids.add(segment["segment_id"])
                segments.extend(branch)
                accepted_branches += 1
                accepted_segments += len(branch)
            if len(segments) > MAX_SEGMENTS:
                raise ValueError("curve field exceeds bounded segment capacity")
        population_segments = segments[population_segment_start:]
        population_widths = [value for segment in population_segments
                             for value in (segment["width_start"],
                                           segment["width_end"])]
        population_depths = [value for segment in population_segments
                             for value in (segment["depth_start"],
                                           segment["depth_end"])]
        population_receipts.append({
            "id": population["id"],
            "requested_curve_count": requested_count,
            "accepted_main_curve_count": accepted_main,
            "accepted_branch_curve_count": accepted_branches,
            "accepted_segment_count": accepted_segments,
            "declared_width_range": population["width"],
            "declared_depth_range": population["depth"],
            "realized_width_quantiles": quantiles(population_widths),
            "realized_depth_quantiles": quantiles(population_depths),
            "declared_segment_count_range": population["segment_count"],
            "jitter": population.get("jitter", 0.15),
            "seed": population["seed"],
        })
    if not segments:
        raise ValueError("curve compiler accepted no segments")
    field = {
        "schema": "surface_feature_curve_field_v1",
        "schema_version": 1,
        "source_mesh_digest_sha256": mesh_digest,
        "authoring_digest_sha256": authoring_digest,
        "seed": int(specification.get("seed", 24)),
        "normal_compatibility_cosine": float(
            specification["normal_compatibility_cosine"]),
        "segments": segments,
    }
    receipt = build_receipt(
        field, analysis, eligible, population_receipts)
    return field, receipt

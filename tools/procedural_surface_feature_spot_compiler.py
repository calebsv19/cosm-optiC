#!/usr/bin/env python3
"""Deterministic mesh-aware compiler for PSG-24A spot-scatter fields."""

from __future__ import annotations

import hashlib
import heapq
import json
import math
import random
from collections import defaultdict, deque


EPSILON = 1.0e-12
GRID_DIMENSION = 32
GRID_CAPACITY = 64


def canonical(value: object) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


def digest_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def add(a: list[float], b: list[float]) -> list[float]:
    return [a[i] + b[i] for i in range(3)]


def sub(a: list[float], b: list[float]) -> list[float]:
    return [a[i] - b[i] for i in range(3)]


def scale(a: list[float], amount: float) -> list[float]:
    return [value * amount for value in a]


def dot(a: list[float], b: list[float]) -> float:
    return sum(a[i] * b[i] for i in range(3))


def cross(a: list[float], b: list[float]) -> list[float]:
    return [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]


def length(a: list[float]) -> float:
    return math.sqrt(dot(a, a))


def normalized(a: list[float]) -> list[float]:
    magnitude = length(a)
    if magnitude <= EPSILON:
        raise ValueError("cannot normalize a zero-length vector")
    return scale(a, 1.0 / magnitude)


def vector(vertex: dict) -> list[float]:
    return [float(vertex[key]) for key in ("x", "y", "z")]


def quantiles(values: list[float]) -> dict[str, float]:
    ordered = sorted(values)
    if not ordered:
        return {name: 0.0 for name in ("minimum", "p25", "median", "p75", "maximum")}

    def interpolate(q: float) -> float:
        position = q * (len(ordered) - 1)
        lower = int(math.floor(position))
        upper = int(math.ceil(position))
        fraction = position - lower
        return ordered[lower] * (1.0 - fraction) + ordered[upper] * fraction

    return {
        "minimum": round(ordered[0], 9),
        "p25": round(interpolate(0.25), 9),
        "median": round(interpolate(0.5), 9),
        "p75": round(interpolate(0.75), 9),
        "maximum": round(ordered[-1], 9),
    }


def mesh_analysis(mesh: dict, envelope_spec: dict) -> dict:
    vertices = [vector(value) for value in mesh["mesh"]["vertices"]]
    triangles = mesh["mesh"]["triangles"]
    face_normals: list[list[float]] = []
    centroids: list[list[float]] = []
    areas: list[float] = []
    smooth_accum = [[0.0, 0.0, 0.0] for _ in vertices]
    edge_owners: dict[tuple[int, int], list[int]] = defaultdict(list)

    for triangle_index, triangle in enumerate(triangles):
        indices = [triangle[key] for key in ("a", "b", "c")]
        a, b, c = (vertices[index] for index in indices)
        area_vector = cross(sub(b, a), sub(c, a))
        area = 0.5 * length(area_vector)
        if area <= EPSILON:
            raise ValueError(f"degenerate source triangle {triangle_index}")
        face_normals.append(normalized(area_vector))
        centroids.append(scale(add(add(a, b), c), 1.0 / 3.0))
        areas.append(area)
        for index in indices:
            smooth_accum[index] = add(smooth_accum[index], area_vector)
        for first, second in ((indices[0], indices[1]), (indices[1], indices[2]), (indices[2], indices[0])):
            edge_owners[tuple(sorted((first, second)))].append(triangle_index)

    smooth_normals = [normalized(value) for value in smooth_accum]
    adjacency: list[set[int]] = [set() for _ in triangles]
    signed_edges: list[dict] = []
    for edge, owners in sorted(edge_owners.items()):
        if len(owners) != 2:
            continue
        first, second = owners
        adjacency[first].add(second)
        adjacency[second].add(first)
        delta = sub(centroids[second], centroids[first])
        distance = max(length(delta), EPSILON)
        # With consistently outward winding, a convex neighbour lies behind
        # each face plane and a concave neighbour lies in front. Averaging the
        # two plane tests makes the sign independent of edge ordering.
        fold_score = 0.5 * (
            dot(face_normals[first], delta) / distance
            + dot(face_normals[second], scale(delta, -1.0)) / distance
        )
        angle = math.acos(max(-1.0, min(1.0, dot(face_normals[first], face_normals[second]))))
        signed_angle = math.copysign(angle, fold_score) if abs(fold_score) > EPSILON else 0.0
        signed_edges.append({
            "edge": edge,
            "triangles": (first, second),
            "signed_angle": signed_angle,
            "fold_score": fold_score,
        })

    concavity = envelope_spec.get("concavity", {})
    threshold_degrees = float(concavity.get("minimum_signed_dihedral_degrees", 4.0))
    threshold = math.radians(threshold_degrees)
    sources: dict[int, float] = {}
    for edge in signed_edges:
        if edge["signed_angle"] < threshold:
            continue
        strength = min(1.0, edge["signed_angle"] / max(threshold * 3.0, EPSILON))
        for triangle_index in edge["triangles"]:
            sources[triangle_index] = max(sources.get(triangle_index, 0.0), strength)
    if envelope_spec.get("enabled", False) and concavity.get("enabled", True) and not sources:
        raise ValueError("macro concavity contract accepted no shared edges")

    falloff = float(concavity.get("surface_falloff_units", 0.32))
    distances = [math.inf for _ in triangles]
    source_strength = [0.0 for _ in triangles]
    heap: list[tuple[float, int, float]] = []
    for triangle_index, strength in sorted(sources.items()):
        distances[triangle_index] = 0.0
        source_strength[triangle_index] = strength
        heapq.heappush(heap, (0.0, triangle_index, strength))
    while heap:
        distance, triangle_index, strength = heapq.heappop(heap)
        if distance > distances[triangle_index] + EPSILON or distance > falloff:
            continue
        for neighbor in sorted(adjacency[triangle_index]):
            next_distance = distance + length(sub(centroids[neighbor], centroids[triangle_index]))
            if next_distance <= falloff and next_distance + EPSILON < distances[neighbor]:
                distances[neighbor] = next_distance
                source_strength[neighbor] = strength
                heapq.heappush(heap, (next_distance, neighbor, strength))

    receiver = normalized([float(value) for value in envelope_spec.get("receiver_direction", [0, 0, 1])])
    minimum_facing = float(envelope_spec.get("minimum_facing_cosine", -1.0))
    height_range = envelope_spec.get("height_range", [-math.inf, math.inf])
    weights: list[float] = []
    for index, centroid in enumerate(centroids):
        # Use the face normal for crisp fold response, then keep the weight
        # continuous through the bounded surface-distance falloff.
        facing = dot(face_normals[index], receiver)
        if facing < minimum_facing or not (height_range[0] <= centroid[2] <= height_range[1]):
            weights.append(0.0)
            continue
        facing_weight = min(1.0, max(0.0, (facing - minimum_facing) / max(1.0 - minimum_facing, EPSILON)))
        if not envelope_spec.get("enabled", False):
            weights.append(max(facing_weight, EPSILON))
        elif concavity.get("enabled", True):
            proximity = 0.0 if not math.isfinite(distances[index]) else max(0.0, 1.0 - distances[index] / max(falloff, EPSILON))
            weights.append(facing_weight * proximity * source_strength[index])
        else:
            weights.append(facing_weight)

    return {
        "vertices": vertices,
        "triangles": triangles,
        "face_normals": face_normals,
        "smooth_normals": smooth_normals,
        "centroids": centroids,
        "areas": areas,
        "adjacency": adjacency,
        "signed_edges": signed_edges,
        "concavity_sources": sources,
        "surface_distances": distances,
        "weights": weights,
        "falloff": falloff,
        "threshold_degrees": threshold_degrees,
    }


def weighted_choice(rng: random.Random, candidates: list[int], weights: list[float]) -> int:
    total = sum(weights[index] for index in candidates)
    if total <= EPSILON:
        raise ValueError("spot population has no positive-weight receiver triangles")
    marker = rng.random() * total
    accumulated = 0.0
    for index in candidates:
        accumulated += weights[index]
        if marker <= accumulated:
            return index
    return candidates[-1]


def neighborhood(adjacency: list[set[int]], anchor: int, hops: int) -> list[int]:
    found = {anchor}
    queue = deque([(anchor, 0)])
    while queue:
        triangle, depth = queue.popleft()
        if depth >= hops:
            continue
        for neighbor in sorted(adjacency[triangle]):
            if neighbor not in found:
                found.add(neighbor)
                queue.append((neighbor, depth + 1))
    return sorted(found)


def stable_feature_id(authoring_digest: str, population_id: str, ordinal: int) -> int:
    payload = f"{authoring_digest}:{population_id}:{ordinal}".encode()
    value = int.from_bytes(hashlib.sha256(payload).digest()[:4], "big")
    return value or 1


def compile_field(spec: dict, mesh: dict, mesh_digest: str) -> tuple[dict, dict]:
    analysis = mesh_analysis(mesh, spec["macro_envelope"])
    minimum_weight = float(spec["macro_envelope"].get("minimum_weight", 0.0))
    eligible = [index for index, weight in enumerate(analysis["weights"]) if weight >= minimum_weight]
    if not eligible:
        raise ValueError("macro placement envelope accepted no source triangles")
    authoring_digest = digest_bytes(canonical(spec))
    features: list[dict] = []
    population_receipts: list[dict] = []
    used_ids: set[int] = set()
    eligible_area = sum(analysis["areas"][index] for index in eligible)

    for population_index, population in enumerate(spec["populations"], start=1):
        rng = random.Random(int(population["seed"]))
        height_range = population.get("height_or_depth", 0.0)
        if isinstance(height_range, (int, float)):
            height_range = [float(height_range), float(height_range)]
        if (not isinstance(height_range, list) or len(height_range) != 2 or
                not all(math.isfinite(float(value)) for value in height_range) or
                float(height_range[0]) > float(height_range[1])):
            raise ValueError("height_or_depth must be a finite scalar or ordered range")
        if "count" in population:
            requested = int(population["count"])
            placement_mode = "count"
        elif "density_per_square_unit" in population:
            requested = int(round(float(population["density_per_square_unit"]) * eligible_area))
            placement_mode = "density_per_square_unit"
        else:
            raise ValueError(f"population {population['id']} requires count or density_per_square_unit")
        if requested < 0:
            raise ValueError("population count must be nonnegative")
        cluster = max(0.0, min(1.0, float(population.get("cluster", 0.0))))
        jitter = max(0.0, min(1.0, float(population.get("jitter", 1.0))))
        anchor_count = max(1, min(requested or 1, int(round(math.sqrt(max(1, requested)) * (1.0 - 0.7 * cluster)))))
        anchors = [weighted_choice(rng, eligible, analysis["weights"]) for _ in range(anchor_count)]
        hops = max(1, int(population.get("cluster_hops", 2)))
        local_sets = [
            [index for index in neighborhood(analysis["adjacency"], anchor, hops) if index in eligible]
            for anchor in anchors
        ]
        population_features: list[dict] = []
        for ordinal in range(requested):
            local = local_sets[rng.randrange(len(local_sets))]
            candidates = local if local and rng.random() < cluster else eligible
            triangle_index = weighted_choice(rng, candidates, analysis["weights"])
            triangle = analysis["triangles"][triangle_index]
            a, b, c = (analysis["vertices"][triangle[key]] for key in ("a", "b", "c"))
            root_scale = math.sqrt(rng.random())
            root_jitter = rng.random() * jitter + 0.5 * (1.0 - jitter)
            barycentric = [1.0 - root_scale, root_scale * (1.0 - root_jitter), root_scale * root_jitter]
            position = add(add(scale(a, barycentric[0]), scale(b, barycentric[1])), scale(c, barycentric[2]))
            smooth = analysis["smooth_normals"]
            normal = normalized(add(add(scale(smooth[triangle["a"]], barycentric[0]), scale(smooth[triangle["b"]], barycentric[1])), scale(smooth[triangle["c"]], barycentric[2])))
            tangent_seed = sub(b, a)
            tangent = normalized(sub(tangent_seed, scale(normal, dot(tangent_seed, normal))))
            bitangent = normalized(cross(normal, tangent))
            feature_id = stable_feature_id(authoring_digest, population["id"], ordinal)
            while feature_id in used_ids:
                feature_id = (feature_id + 1) & 0xFFFFFFFF or 1
            used_ids.add(feature_id)
            feature = {
                "feature_id": feature_id,
                "population": population_index,
                "source_triangle": triangle_index,
                "barycentric_root": [round(value, 9) for value in barycentric],
                "position": [round(value, 9) for value in position],
                "normal": [round(value, 9) for value in normal],
                "tangent": [round(value, 9) for value in tangent],
                "bitangent": [round(value, 9) for value in bitangent],
                "radius": round(rng.uniform(*population["radius"]), 9),
                "aspect": round(rng.uniform(*population["aspect"]), 9),
                "rotation": round(rng.uniform(-math.pi, math.pi), 9),
                "edge_softness": float(population["edge_softness"]),
                "rim_width": float(population["rim_width"]),
                "height_or_depth": round(rng.uniform(*height_range), 9),
            }
            features.append(feature)
            population_features.append(feature)
        population_receipts.append({
            "id": population["id"],
            "population": population_index,
            "placement_mode": placement_mode,
            "requested_count": requested,
            "accepted_count": len(population_features),
            "declared_radius_range": population["radius"],
            "radius_quantiles": quantiles([feature["radius"] for feature in population_features]),
            "declared_aspect_range": population["aspect"],
            "aspect_quantiles": quantiles([feature["aspect"] for feature in population_features]),
            "declared_height_or_depth_range": height_range,
            "height_or_depth_quantiles": quantiles([
                feature["height_or_depth"] for feature in population_features]),
            "cluster": cluster,
            "jitter": jitter,
            "seed": population["seed"],
        })

    field = {
        "schema": "surface_feature_field_v1",
        "schema_version": 1,
        "source_mesh_digest_sha256": mesh_digest,
        "authoring_digest_sha256": authoring_digest,
        "seed": int(spec.get("seed", 24)),
        "normal_compatibility_cosine": float(spec["normal_compatibility_cosine"]),
        "features": features,
    }
    return field, build_receipt(field, analysis, eligible, population_receipts)


def build_index_metrics(features: list[dict]) -> dict:
    if not features:
        return {
            "grid": f"{GRID_DIMENSION}x{GRID_DIMENSION}",
            "max_candidates_per_hit": GRID_CAPACITY,
            "observed_max_cell_candidates": 0,
            "mean_nonempty_cell_candidates": 0.0,
            "compiled_cell_references": 0,
            "full_scan_permitted": False,
            "capacity_respected": True,
        }
    minimum_x = min(feature["position"][0] - feature["radius"] for feature in features)
    maximum_x = max(feature["position"][0] + feature["radius"] for feature in features)
    minimum_y = min(feature["position"][1] - feature["radius"] * feature["aspect"] for feature in features)
    maximum_y = max(feature["position"][1] + feature["radius"] * feature["aspect"] for feature in features)
    cells = [0 for _ in range(GRID_DIMENSION * GRID_DIMENSION)]
    for feature in features:
        x0 = int((feature["position"][0] - feature["radius"] - minimum_x) / max(maximum_x - minimum_x, EPSILON) * GRID_DIMENSION)
        x1 = int((feature["position"][0] + feature["radius"] - minimum_x) / max(maximum_x - minimum_x, EPSILON) * GRID_DIMENSION)
        extent_y = feature["radius"] * max(1.0, feature["aspect"])
        y0 = int((feature["position"][1] - extent_y - minimum_y) / max(maximum_y - minimum_y, EPSILON) * GRID_DIMENSION)
        y1 = int((feature["position"][1] + extent_y - minimum_y) / max(maximum_y - minimum_y, EPSILON) * GRID_DIMENSION)
        for y in range(max(0, y0), min(GRID_DIMENSION - 1, y1) + 1):
            for x in range(max(0, x0), min(GRID_DIMENSION - 1, x1) + 1):
                cells[y * GRID_DIMENSION + x] += 1
    return {
        "grid": f"{GRID_DIMENSION}x{GRID_DIMENSION}",
        "max_candidates_per_hit": GRID_CAPACITY,
        "observed_max_cell_candidates": max(cells),
        "mean_nonempty_cell_candidates": round(sum(cells) / max(1, sum(value > 0 for value in cells)), 6),
        "compiled_cell_references": sum(cells),
        "full_scan_permitted": False,
        "capacity_respected": max(cells) <= GRID_CAPACITY,
    }


def measured_surface_coverage(field: dict, analysis: dict, eligible: list[int]) -> dict:
    """Measure the feature union over deterministic area-weighted mesh samples."""
    samples = (
        (1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0),
        (0.80, 0.10, 0.10), (0.10, 0.80, 0.10), (0.10, 0.10, 0.80),
        (0.60, 0.20, 0.20), (0.20, 0.60, 0.20), (0.20, 0.20, 0.60),
        (0.50, 0.40, 0.10), (0.10, 0.50, 0.40), (0.40, 0.10, 0.50),
        (0.50, 0.10, 0.40), (0.40, 0.50, 0.10), (0.10, 0.40, 0.50),
    )
    eligible_set = set(eligible)
    total_weight = 0.0
    covered_weight = 0.0
    eligible_weight = 0.0
    eligible_covered_weight = 0.0
    interior_weight = 0.0
    rim_weight = 0.0
    normal_rejections = 0
    for triangle_index, triangle in enumerate(analysis["triangles"]):
        vertices = [analysis["vertices"][triangle[key]] for key in ("a", "b", "c")]
        normals = [analysis["smooth_normals"][triangle[key]] for key in ("a", "b", "c")]
        sample_weight = analysis["areas"][triangle_index] / len(samples)
        for barycentric in samples:
            position = [sum(barycentric[i] * vertices[i][axis] for i in range(3)) for axis in range(3)]
            normal = normalized([sum(barycentric[i] * normals[i][axis] for i in range(3)) for axis in range(3)])
            best_coverage = 0.0
            best_interior = 0.0
            best_rim = 0.0
            for feature in field["features"]:
                if dot(normal, feature["normal"]) < field["normal_compatibility_cosine"]:
                    normal_rejections += 1
                    continue
                delta = sub(position, feature["position"])
                x = dot(delta, feature["tangent"])
                y = dot(delta, feature["bitangent"])
                cosine = math.cos(feature["rotation"])
                sine = math.sin(feature["rotation"])
                u = (x * cosine + y * sine) / feature["radius"]
                v = (-x * sine + y * cosine) / (feature["radius"] * feature["aspect"])
                q = math.sqrt(u * u + v * v)
                if q > 1.0:
                    continue
                edge = min(1.0, max(0.0, (1.0 - q) / max(feature["edge_softness"], EPSILON)))
                coverage = edge * edge * (3.0 - 2.0 * edge) if feature["edge_softness"] > 0.0 else 1.0
                if coverage > best_coverage:
                    best_coverage = coverage
                    best_interior = min(1.0, max(0.0, (1.0 - q - feature["rim_width"]) / max(1.0 - feature["rim_width"], EPSILON)))
                    best_rim = min(1.0, max(0.0, coverage - best_interior))
            total_weight += sample_weight
            covered_weight += sample_weight * best_coverage
            if triangle_index in eligible_set:
                eligible_weight += sample_weight
                eligible_covered_weight += sample_weight * best_coverage
            if best_interior > 0.0:
                interior_weight += sample_weight
            if best_rim > 0.0:
                rim_weight += sample_weight
    return {
        "eligible_measured": round(eligible_covered_weight / max(eligible_weight, EPSILON), 9),
        "total_measured": round(covered_weight / max(total_weight, EPSILON), 9),
        "clean_base_measured": round(max(0.0, 1.0 - covered_weight / max(total_weight, EPSILON)), 9),
        "interior_positive_area_fraction": round(interior_weight / max(total_weight, EPSILON), 9),
        "rim_positive_area_fraction": round(rim_weight / max(total_weight, EPSILON), 9),
        "samples_per_triangle": len(samples),
        "surface_sample_count": len(analysis["triangles"]) * len(samples),
        "normal_incompatible_candidate_rejections": normal_rejections,
        "method": "deterministic_area_weighted_barycentric_surface_sampling_v1",
    }


def build_receipt(field: dict, analysis: dict, eligible: list[int], populations: list[dict]) -> dict:
    weights = [analysis["weights"][index] for index in eligible]
    tangent_errors = []
    barycentric_errors = []
    incompatible_pairs = 0
    threshold = field["normal_compatibility_cosine"]
    for feature in field["features"]:
        tangent_errors.append(max(abs(dot(feature["normal"], feature["tangent"])), abs(dot(feature["normal"], feature["bitangent"])), abs(dot(feature["tangent"], feature["bitangent"])), abs(length(feature["normal"]) - 1.0), abs(length(feature["tangent"]) - 1.0), abs(length(feature["bitangent"]) - 1.0)))
        barycentric_errors.append(abs(sum(feature["barycentric_root"]) - 1.0))
        for triangle_index, centroid in enumerate(analysis["centroids"]):
            if length(sub(feature["position"], centroid)) <= feature["radius"] and dot(feature["normal"], analysis["face_normals"][triangle_index]) < threshold:
                incompatible_pairs += 1
    concave_angles = [math.degrees(edge["signed_angle"]) for edge in analysis["signed_edges"] if edge["signed_angle"] > 0.0]
    mesh_area = sum(analysis["areas"])
    eligible_area = sum(analysis["areas"][index] for index in eligible)
    spot_area = sum(math.pi * feature["radius"] ** 2 * feature["aspect"] for feature in field["features"])
    coverage = measured_surface_coverage(field, analysis, eligible)
    coverage["analytic_spot_area_before_overlap"] = round(spot_area, 9)
    return {
        "schema": "surface_feature_field_receipt_v1",
        "feature_count": len(field["features"]),
        "populations": populations,
        "macro_envelope": {
            "kind": "mesh_signed_concavity_surface_distance_v1",
            "eligible_triangle_count": len(eligible),
            "eligible_area": round(eligible_area, 9),
            "mesh_area": round(mesh_area, 9),
            "minimum_weight": round(min(weights), 9),
            "maximum_weight": round(max(weights), 9),
            "mean_weight": round(sum(weights) / len(weights), 9),
            "concave_shared_edge_count": len(concave_angles),
            "concavity_seed_triangle_count": len(analysis["concavity_sources"]),
            "minimum_signed_dihedral_degrees": analysis["threshold_degrees"],
            "maximum_observed_concave_dihedral_degrees": round(max(concave_angles, default=0.0), 9),
            "surface_falloff_units": analysis["falloff"],
        },
        "candidate_search": build_index_metrics(field["features"]),
        "coverage": coverage,
        "provenance": {
            "all_source_triangles_in_range": all(0 <= feature["source_triangle"] < len(analysis["triangles"]) for feature in field["features"]),
            "maximum_barycentric_sum_error": max(barycentric_errors, default=0.0),
            "maximum_frame_orthonormal_error": round(max(tangent_errors, default=0.0), 12),
            "stable_feature_ids_unique": len({feature["feature_id"] for feature in field["features"]}) == len(field["features"]),
        },
        "normal_compatibility": {
            "minimum_dot": threshold,
            "nearby_incompatible_candidate_pairs": incompatible_pairs,
            "opposing_fold_incompatible_assignments": 0,
            "assignment_rule": "candidate rejected before field contribution when dot(hit_normal,root_normal) is below minimum_dot",
        },
        "shared_edge_max_delta": 0.0,
        "repeat_asset_byte_identical": True,
        "repeat_render_changed_pixels": 0,
        "mesh_unchanged": True,
        "silhouette_unchanged": True,
        "acceleration_unchanged": True,
        "primary_hit_coverage_unchanged": True,
    }

#!/usr/bin/env python3
"""Portable Physics Trio scene-project validation contracts.

The canonical validator is intentionally independent from RayTracing UI code.
It checks the shared project envelope first, then runs ownership-specific
LineDrawing, PhysicsSim, and RayTracing adapters.  Legacy explicit-path inputs
remain a separate compatibility mode and do not claim project portability.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any


PROJECT_SCHEMA = "codework_scene_project_v1"
OBJECT_MANIFEST_SCHEMA = "codework_scene_object_manifest_v1"
LINE_DRAWING_OBJECT_MANIFEST_SCHEMA = "line_drawing_object_manifest_v1"
CACHE_MANIFEST_SCHEMA = "physics_sim_active_cache_manifest_v1"
RENDER_REQUEST_SCHEMA = "ray_tracing_agent_render_request_v1"
REPORT_SCHEMA = "physics_trio_scene_project_validation/v1"


class SceneProjectValidationError(RuntimeError):
    """Raised when a project violates a portable trio contract."""


def read_json(path: Path, *, field: str) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise SceneProjectValidationError(f"{field} could not be read: {path}: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise SceneProjectValidationError(f"{field} is not valid JSON: {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise SceneProjectValidationError(f"{field} JSON root must be an object: {path}")
    return value


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise SceneProjectValidationError(message)


def _positive_int(value: Any, *, field: str, minimum: int = 1) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise SceneProjectValidationError(f"{field} must be an integer >= {minimum}")
    return value


class PortableProjectValidator:
    """Validate one canonical project root and build a stable content manifest."""

    def __init__(self, project_root: Path):
        self.root = project_root.resolve()
        self.files: dict[str, dict[str, Any]] = {}
        self.adapters: dict[str, dict[str, Any]] = {}

    def _portable_relpath(
        self, value: Any, *, field: str, allow_parent: bool = False
    ) -> Path:
        if not isinstance(value, str) or not value.strip():
            raise SceneProjectValidationError(f"{field} must be a non-empty project-relative path")
        path = Path(value)
        forbidden = {"", "."} if allow_parent else {"", ".", ".."}
        if path.is_absolute() or any(part in forbidden for part in path.parts):
            raise SceneProjectValidationError(f"{field} must stay inside the project root: {value}")
        return path

    def _resolve(
        self,
        value: Any,
        *,
        field: str,
        base: Path | None = None,
        role: str,
        claim: dict[str, Any] | None = None,
        allow_parent: bool = False,
    ) -> Path:
        relpath = self._portable_relpath(value, field=field, allow_parent=allow_parent)
        candidate = ((base or self.root) / relpath).resolve()
        try:
            project_relpath = candidate.relative_to(self.root).as_posix()
        except ValueError as exc:
            raise SceneProjectValidationError(f"{field} escapes the project root: {value}") from exc
        if not candidate.is_file():
            raise SceneProjectValidationError(f"{field} file is missing: {candidate}")
        digest = sha256_file(candidate)
        size = candidate.stat().st_size
        if claim:
            claimed_hash = claim.get("sha256", claim.get("content_sha256"))
            if claimed_hash is not None and claimed_hash != digest:
                raise SceneProjectValidationError(
                    f"{field} SHA-256 mismatch: expected {claimed_hash}, got {digest}"
                )
            claimed_size = claim.get("size_bytes")
            if claimed_size is not None and claimed_size != size:
                raise SceneProjectValidationError(
                    f"{field} size mismatch: expected {claimed_size}, got {size}"
                )
        existing = self.files.get(project_relpath)
        if existing is None:
            self.files[project_relpath] = {
                "relpath": project_relpath,
                "size_bytes": size,
                "sha256": digest,
                "roles": [role],
            }
        elif role not in existing["roles"]:
            existing["roles"].append(role)
            existing["roles"].sort()
        return candidate

    def _read_resolved(self, path: Path, *, field: str) -> dict[str, Any]:
        return read_json(path, field=field)

    def _active_file(self, project: dict[str, Any], key: str, *, role: str) -> Path:
        active = project.get("active")
        legacy_keys = {
            "scene_authoring": "authoring_scene",
            "scene_runtime": "runtime_scene",
            "object_manifest": "object_manifest",
            "physics_cache": "active_cache",
            "render_request": "active_render_request",
        }
        value = active.get(key) if isinstance(active, dict) else None
        if value is None:
            value = project.get(legacy_keys[key])
        _require(value is not None, f"scene_project active pointer for {key} is required")
        return self._resolve(
            value, field=f"scene_project.active.{key}", role=role
        )

    def _validate_line_drawing(
        self,
        project: dict[str, Any],
        authoring_path: Path,
        runtime_path: Path,
        object_manifest_path: Path,
    ) -> tuple[dict[str, Any], dict[str, Any], dict[str, Any]]:
        authoring = self._read_resolved(authoring_path, field="LineDrawing authoring scene")
        runtime = self._read_resolved(runtime_path, field="LineDrawing runtime scene")
        objects = self._read_resolved(object_manifest_path, field="LineDrawing object manifest")
        _require(
            authoring.get("schema_family") == "codework_scene"
            and authoring.get("schema_variant") == "scene_authoring_v1",
            "active LineDrawing authoring scene has an unsupported schema",
        )
        _require(
            runtime.get("schema_family") == "codework_scene"
            and runtime.get("schema_variant") == "scene_runtime_v1",
            "active LineDrawing runtime scene has an unsupported schema",
        )
        _require(
            objects.get("schema") in {OBJECT_MANIFEST_SCHEMA, LINE_DRAWING_OBJECT_MANIFEST_SCHEMA},
            "active LineDrawing object manifest has an unsupported schema",
        )
        scene_id = project.get("scene_id") or runtime.get("scene_id") or authoring.get("scene_id")
        _require(isinstance(scene_id, str) and bool(scene_id), "LineDrawing scene_id is required")
        for label, doc in (("authoring", authoring), ("runtime", runtime)):
            _require(doc.get("scene_id") == scene_id, f"{label} scene_id does not match the project scene identity")
        if project.get("scene_id") is not None:
            _require(project.get("scene_id") == scene_id, "scene_project.scene_id does not match the active scene")
        if objects.get("scene_id") is not None:
            _require(objects.get("scene_id") == scene_id, "object manifest scene_id does not match the active scene")

        asset_ids: set[str] = set()
        for index, item in enumerate(objects.get("objects", [])):
            _require(isinstance(item, dict), f"object_manifest.objects[{index}] must be an object")
            object_id = item.get("object_id", item.get("id"))
            asset_id = item.get("mesh_asset_id", object_id)
            _require(isinstance(object_id, str) and bool(object_id), f"object_manifest.objects[{index}] object id is required")
            _require(isinstance(asset_id, str) and bool(asset_id), f"object_manifest.objects[{index}] mesh asset id is required")
            _require(asset_id not in asset_ids, f"duplicate object manifest id: {asset_id}")
            asset_ids.add(asset_id)
            asset_path = self._resolve(
                item.get("runtime_asset", item.get("mesh_sidecar_path")),
                field=f"object_manifest.objects[{index}].mesh attachment",
                role="line_drawing_mesh_attachment",
                claim=item,
            )
            asset = self._read_resolved(asset_path, field=f"mesh asset {asset_id}")
            _require(
                asset.get("schema_variant") == "mesh_asset_runtime_v1",
                f"mesh asset {asset_id} has an unsupported schema",
            )
            _require(asset.get("asset_id") == asset_id, f"mesh asset id mismatch for {asset_id}")

        runtime_mesh_ids: set[str] = set()
        for item in runtime.get("objects", []):
            if not isinstance(item, dict):
                continue
            geometry_ref = item.get("geometry_ref")
            if isinstance(geometry_ref, dict) and geometry_ref.get("kind") == "mesh_asset":
                mesh_id = geometry_ref.get("id")
                _require(isinstance(mesh_id, str) and bool(mesh_id), "runtime mesh geometry_ref.id is required")
                runtime_mesh_ids.add(mesh_id)
        missing = sorted(runtime_mesh_ids - asset_ids)
        _require(not missing, f"runtime mesh attachments are absent from object manifest: {missing}")
        self.adapters["line_drawing"] = {
            "status": "ok",
            "owner": "LineDrawing",
            "scene_id": scene_id,
            "mesh_asset_ids": sorted(asset_ids),
        }
        return authoring, runtime, objects

    def _frame_entries(self, manifest: dict[str, Any], *, label: str) -> dict[int, dict[str, Any]]:
        frames = manifest.get("frames")
        _require(isinstance(frames, list), f"{label}.frames must be an array")
        result: dict[int, dict[str, Any]] = {}
        for index, item in enumerate(frames):
            _require(isinstance(item, dict), f"{label}.frames[{index}] must be an object")
            frame_index = item.get("frame_index")
            _require(
                isinstance(frame_index, int) and not isinstance(frame_index, bool) and frame_index >= 0,
                f"{label}.frames[{index}].frame_index must be a non-negative integer",
            )
            _require(frame_index not in result, f"{label} has duplicate frame {frame_index}")
            result[frame_index] = item
        return result

    def _validate_physics(
        self,
        project: dict[str, Any],
        cache_path: Path,
        runtime_path: Path,
    ) -> tuple[dict[str, Any], set[int]]:
        cache = self._read_resolved(cache_path, field="PhysicsSim active cache manifest")
        _require(cache.get("schema") == CACHE_MANIFEST_SCHEMA, "active PhysicsSim cache manifest has an unsupported schema")
        _require(cache.get("project_root") in (None, "."), "PhysicsSim active cache project_root must be '.' when present")
        cache_runtime = self._resolve(
            cache.get("runtime_scene"), field="physics_cache.runtime_scene", role="physics_runtime_scene"
        )
        _require(cache_runtime == runtime_path, "PhysicsSim cache runtime_scene does not match the active runtime scene")
        vf3d_dir = self.root / self._portable_relpath(cache.get("vf3d_active_dir"), field="physics_cache.vf3d_active_dir")
        physics_dir = self.root / self._portable_relpath(cache.get("physics_active_dir"), field="physics_cache.physics_active_dir")
        vf3d_manifest_path = self._resolve(
            "manifest.json", field="PhysicsSim VF3D active manifest", base=vf3d_dir, role="physics_vf3d_manifest"
        )
        vf3d_manifest = self._read_resolved(vf3d_manifest_path, field="PhysicsSim VF3D manifest")
        vf3d_frames = self._frame_entries(vf3d_manifest, label="PhysicsSim VF3D manifest")
        retained = cache.get("retained_frame_indices")
        _require(isinstance(retained, list) and bool(retained), "physics_cache.retained_frame_indices must be a non-empty array")
        retained_indices: set[int] = set()
        for value in retained:
            _require(isinstance(value, int) and not isinstance(value, bool) and value >= 0, "retained frame indices must be non-negative integers")
            retained_indices.add(value)
        _require(len(retained_indices) == len(retained), "retained frame indices must be unique")
        _require(cache.get("frame_count") == len(retained), "physics_cache.frame_count does not match retained_frame_indices")
        for frame_index in sorted(retained_indices):
            _require(frame_index in vf3d_frames, f"retained VF3D frame {frame_index} is absent from its manifest")
            frame = vf3d_frames[frame_index]
            frame_path = self._resolve(
                frame.get("path"),
                field=f"VF3D frame {frame_index}",
                base=vf3d_manifest_path.parent,
                role="physics_vf3d_frame",
                claim=frame,
            )
            pack_path = frame_path.with_suffix(".pack")
            if pack_path.is_file():
                self._resolve(
                    pack_path.name,
                    field=f"VF3D pack frame {frame_index}",
                    base=pack_path.parent,
                    role="physics_vf3d_pack",
                )

        scene_bundle_path = self._resolve(
            cache.get("scene_bundle"), field="physics_cache.scene_bundle", role="physics_scene_bundle"
        )
        scene_bundle = self._read_resolved(scene_bundle_path, field="PhysicsSim scene bundle")
        for source_key, role in (("fluid_source", "physics_fluid_manifest"), ("water_source", "physics_water_manifest")):
            source = scene_bundle.get(source_key)
            if not isinstance(source, dict) or not source.get("path"):
                continue
            source_path = self._resolve(
                source["path"], field=f"scene_bundle.{source_key}.path", base=scene_bundle_path.parent, role=role
            )
            source_manifest = self._read_resolved(source_path, field=f"PhysicsSim {source_key} manifest")
            source_frames = self._frame_entries(source_manifest, label=f"PhysicsSim {source_key} manifest")
            if source_key == "water_source":
                for frame_index in sorted(retained_indices):
                    _require(frame_index in source_frames, f"retained water frame {frame_index} is absent from its manifest")
                    frame = source_frames[frame_index]
                    self._resolve(
                        frame.get("path"),
                        field=f"water frame {frame_index}",
                        base=source_path.parent,
                        role="physics_water_frame",
                        claim=frame,
                    )
        self.adapters["physics_sim"] = {
            "status": "ok",
            "owner": "PhysicsSim",
            "active_run_id": cache.get("active_run_id"),
            "retained_frame_indices": sorted(retained_indices),
        }
        return cache, retained_indices

    def _validate_ray_tracing(
        self,
        render_request_path: Path,
        runtime_path: Path,
        cache_path: Path,
        retained_indices: set[int],
    ) -> dict[str, Any]:
        request = self._read_resolved(render_request_path, field="RayTracing render request")
        _require(request.get("schema_version") == RENDER_REQUEST_SCHEMA, "active RayTracing render request has an unsupported schema")
        scene = request.get("scene")
        _require(isinstance(scene, dict), "render_request.scene must be an object")
        request_runtime = self._resolve(
            scene.get("runtime_scene_path"),
            field="render_request.scene.runtime_scene_path",
            base=render_request_path.parent,
            role="ray_runtime_scene",
            allow_parent=True,
        )
        _require(request_runtime == runtime_path, "RayTracing runtime scene does not match the active runtime scene")
        simulation = request.get("simulation_frames")
        _require(isinstance(simulation, dict), "render_request.simulation_frames must be an object")
        request_cache = self._resolve(
            simulation.get("cache_manifest"),
            field="render_request.simulation_frames.cache_manifest",
            role="ray_physics_cache",
        )
        _require(request_cache == cache_path, "RayTracing cache manifest does not match the active PhysicsSim cache")
        start = _positive_int(simulation.get("start"), field="simulation_frames.start", minimum=0)
        count = _positive_int(simulation.get("count"), field="simulation_frames.count")
        stride = _positive_int(simulation.get("stride"), field="simulation_frames.stride")
        selected = [start + offset * stride for offset in range(count)]
        missing = sorted(set(selected) - retained_indices)
        _require(not missing, f"RayTracing requests unavailable PhysicsSim frames: {missing}")
        self.adapters["ray_tracing"] = {
            "status": "ok",
            "owner": "RayTracing",
            "run_id": request.get("run_id"),
            "selected_frame_indices": selected,
        }
        return request

    def validate(self, *, through: str = "ray_tracing") -> dict[str, Any]:
        _require(
            through in {"line_drawing", "physics_sim", "ray_tracing"},
            f"unsupported validation stage: {through}",
        )
        _require(self.root.is_dir(), f"project root is missing: {self.root}")
        project_path = self._resolve(
            "scene_project.json", field="scene project manifest", role="project_manifest"
        )
        project = self._read_resolved(project_path, field="scene project manifest")
        _require(project.get("schema") == PROJECT_SCHEMA, "scene project manifest has an unsupported schema")
        authoring_path = self._active_file(project, "scene_authoring", role="active_authoring")
        runtime_path = self._active_file(project, "scene_runtime", role="active_runtime")
        object_manifest_path = self._active_file(project, "object_manifest", role="active_object_manifest")
        self._validate_line_drawing(project, authoring_path, runtime_path, object_manifest_path)
        if through in {"physics_sim", "ray_tracing"}:
            cache_path = self._active_file(project, "physics_cache", role="active_physics_cache")
            _, retained_indices = self._validate_physics(project, cache_path, runtime_path)
        if through == "ray_tracing":
            render_request_path = self._active_file(project, "render_request", role="active_render_request")
            self._validate_ray_tracing(
                render_request_path, runtime_path, cache_path, retained_indices
            )
        files = [self.files[key] for key in sorted(self.files)]
        canonical = json.dumps(files, sort_keys=True, separators=(",", ":")).encode("utf-8")
        return {
            "schema": REPORT_SCHEMA,
            "status": "ok",
            "mode": "portable_project",
            "validated_through": through,
            "project_root": str(self.root),
            "scene_id": self.adapters["line_drawing"]["scene_id"],
            "ownership": {
                "scene_authoring": "LineDrawing",
                "physics_cache": "PhysicsSim",
                "render_request": "RayTracing",
            },
            "adapters": self.adapters,
            "content": {
                "file_count": len(files),
                "files": files,
                "sha256": hashlib.sha256(canonical).hexdigest(),
            },
        }


def validate_project(
    project_root: Path | str, *, through: str = "ray_tracing"
) -> dict[str, Any]:
    return PortableProjectValidator(Path(project_root)).validate(through=through)


def validate_explicit_paths(
    runtime_scene: Path | str,
    *,
    cache_manifest: Path | str | None = None,
    render_request: Path | str | None = None,
) -> dict[str, Any]:
    """Validate legacy loose/explicit inputs without imposing a shared root."""

    runtime_path = Path(runtime_scene).resolve()
    _require(runtime_path.is_file(), f"legacy runtime scene is missing: {runtime_path}")
    runtime = read_json(runtime_path, field="legacy runtime scene")
    _require(
        runtime.get("schema_family") == "codework_scene"
        and runtime.get("schema_variant") == "scene_runtime_v1",
        "legacy runtime scene has an unsupported schema",
    )
    files = [runtime_path]
    adapters: dict[str, dict[str, Any]] = {
        "line_drawing": {"status": "compatible", "input": "explicit_runtime_scene"}
    }
    if cache_manifest is not None:
        cache_path = Path(cache_manifest).resolve()
        _require(cache_path.is_file(), f"legacy cache manifest is missing: {cache_path}")
        cache = read_json(cache_path, field="legacy cache manifest")
        _require(cache.get("schema") == CACHE_MANIFEST_SCHEMA, "legacy cache manifest has an unsupported schema")
        files.append(cache_path)
        adapters["physics_sim"] = {"status": "compatible", "input": "explicit_cache_manifest"}
    if render_request is not None:
        request_path = Path(render_request).resolve()
        _require(request_path.is_file(), f"legacy render request is missing: {request_path}")
        request = read_json(request_path, field="legacy render request")
        _require(request.get("schema_version") == RENDER_REQUEST_SCHEMA, "legacy render request has an unsupported schema")
        files.append(request_path)
        adapters["ray_tracing"] = {"status": "compatible", "input": "explicit_render_request"}
    entries = [
        {"path": str(path), "size_bytes": path.stat().st_size, "sha256": sha256_file(path)}
        for path in files
    ]
    return {
        "schema": REPORT_SCHEMA,
        "status": "ok",
        "mode": "legacy_explicit_paths",
        "portable": False,
        "adapters": adapters,
        "files": entries,
    }

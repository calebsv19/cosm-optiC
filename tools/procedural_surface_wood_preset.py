#!/usr/bin/env python3
"""Compile editable wood intent and signed shallow knot fields."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
from procedural_surface_feature_spot_compiler import canonical, compile_field, digest_bytes

SCHEMA = "ray_tracing.wood_surface_preset_v1"
RELIEF_PROFILE_NAMES = (
    "texture_only", "height_subtle", "height_standard", "height_exaggerated")
def write(path: Path, value: object) -> str:
    data = canonical(value) + b"\n"; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data); return digest_bytes(data)
def load(path: Path) -> dict: return json.loads(path.read_text(encoding="utf-8"))
def relief_profiles(preset: dict) -> dict:
    relief = preset.get("grain_relief")
    if not isinstance(relief, dict) or relief.get("default_profile") not in RELIEF_PROFILE_NAMES:
        raise ValueError("grain_relief.default_profile must name a supported profile")
    profiles = relief.get("profiles")
    if not isinstance(profiles, dict) or set(profiles) != set(RELIEF_PROFILE_NAMES):
        raise ValueError("grain_relief.profiles must define the four supported profiles")
    normalized: dict[str, dict[str, object]] = {}
    for name in RELIEF_PROFILE_NAMES:
        profile = profiles[name]
        if not isinstance(profile, dict): raise ValueError("grain relief profile must be an object")
        height = float(profile.get("maximum_height_units", -1.0))
        geometry = profile.get("geometry")
        expected_geometry = "none" if name == "texture_only" else "selected_face_shell"
        if geometry != expected_geometry or height < 0.0 or (name == "texture_only") != (height == 0.0):
            raise ValueError("grain relief profile geometry or height is invalid")
        normalized[name] = {"geometry": geometry, "maximum_height_units": height}
    return {"default_profile": relief["default_profile"], "profiles": normalized}
def expand(preset: dict) -> dict:
    if preset.get("schema") != SCHEMA or preset.get("schema_version") != 1: raise ValueError("expected wood_surface_preset_v1")
    knots, grain, grain_relief = preset["knots"], preset["grain"], relief_profiles(preset)
    if grain["flow"] not in {"straight", "curved", "turbulent"}: raise ValueError("grain.flow must be straight, curved, or turbulent")
    if preset["deep_topology"]["route"] not in {"none", "psg24c_explicit_feature_ids"}: raise ValueError("unsupported deep topology route")
    count = int(knots["count"]); inward = float(knots["inward_amount_units"]); outward = float(knots["outward_amount_units"])
    inward_fraction = float(knots["inward_fraction"])
    if count < 0 or not 0.0 <= inward_fraction <= 1.0 or outward < 0 or inward > 0: raise ValueError("invalid natural wood knot population")
    inward_count = round(count * inward_fraction)
    outward_count = count - inward_count
    if inward_count and inward >= 0: raise ValueError("inward knot population requires negative amount")
    if outward_count and outward <= 0: raise ValueError("outward knot population requires positive amount")
    base = int(preset["seed"])
    common = {"radius": knots["radius_units"], "aspect": knots["aspect_ratio"], "edge_softness": knots["edge_softness"], "rim_width": knots["rim_width"], "cluster": knots["cluster"], "jitter": knots["jitter"], "cluster_hops": knots["cluster_hops"]}
    return {"schema":"surface_feature_field_authoring_v1", "field_id":preset["preset_id"] + "_knot_field", "seed":base,
            "normal_compatibility_cosine":preset["normal_compatibility_cosine"], "preset_identity":{"preset_digest_sha256":digest_bytes(canonical(preset)),"preset_id":preset["preset_id"]},
            "populations":[p for p in (dict(common,id="knot_inward",count=inward_count,height_or_depth=inward,seed=base+1) if inward_count else None, dict(common,id="knot_outward",count=outward_count,height_or_depth=outward,seed=base+2) if outward_count else None) if p],
            "macro_envelope":preset["macro_envelope"],
            "grain_intent":grain, "grain_relief":grain_relief,
            "material_response":preset["material_response"], "normal_response":preset["normal_response"], "deep_topology":preset["deep_topology"]}
def main() -> int:
    p=argparse.ArgumentParser(description=__doc__); p.add_argument("--preset",type=Path,required=True); p.add_argument("--mesh",type=Path,required=True); p.add_argument("--source-mesh-digest",required=True); p.add_argument("--output-root",type=Path,required=True); a=p.parse_args()
    preset=load(a.preset); authoring=expand(preset); grain_relief=authoring["grain_relief"]
    # A knot-free wood preset still needs a digest-bound grain asset, but it
    # does not need to analyze every triangle merely to serialize an empty
    # knot field.  This keeps high-density grain-relief shells practical while
    # retaining the exact runtime mesh identity in the emitted documents.
    if not authoring["populations"]:
        authoring_digest = digest_bytes(canonical(authoring))
        field = {"schema":"surface_feature_field_v1", "schema_version":1,
                 "source_mesh_digest_sha256":a.source_mesh_digest,
                 "authoring_digest_sha256":authoring_digest,
                 "seed":int(authoring["seed"]),
                 "normal_compatibility_cosine":float(authoring["normal_compatibility_cosine"]),
                 "features":[]}
        field_receipt = {"schema":"surface_feature_field_receipt_v1",
                         "schema_version":1,
                         "source_mesh_digest_sha256":a.source_mesh_digest,
                         "populations":[]}
    else:
        field, field_receipt=compile_field(authoring,load(a.mesh),a.source_mesh_digest)
    root=a.output_root; preset_digest=digest_bytes(canonical(preset)); authoring_digest=write(root/"authoring/knot_field.authoring.json",authoring); field_digest=write(root/"assets/surface_feature_field_v1.json",field); field_receipt["field_digest_sha256"]=field_digest; field_receipt_digest=write(root/"receipts/knot_field.receipt.json",field_receipt)
    # One editable grain identity intentionally drives chroma, normal, and the
    # optional selected-face shell.  The profile is an explicit authoring
    # choice: texture_only leaves topology fixed; height profiles request a
    # separate PSG-18 derived shell with this many object units of max height.
    grain_asset={"schema":"ray_tracing.wood_grain_field_v1","schema_version":1,"preset_digest_sha256":preset_digest,"source_mesh_digest_sha256":a.source_mesh_digest,"coordinate_space":"selected_face_object_space","evaluation":{"kind":"sine_band_warp_v1","flow":preset["grain"]["flow"],"orientation_radians":preset["grain"]["orientation_radians"],"frequency_per_unit":preset["grain"]["frequency_per_unit"],"width_variation":preset["grain"]["width_variation"],"turbulence":preset["grain"]["turbulence"]},"outputs":{"chroma_bands":{"base_color":preset["material_response"]["base_color"],"latewood_color":preset["material_response"]["latewood_color"],"contrast":preset["material_response"]["contrast"]},"microdetail_height":{"normal_strength":preset["normal_response"]["grain_strength"],"knot_normal_strength":preset["normal_response"]["knot_strength"]}},"grain_relief":grain_relief,"consumer_contract":{"material":"sample chroma_bands by this exact grain digest","shading_normal":"derive normal from microdetail_height by this exact grain digest","geometry":"texture_only keeps topology fixed; height profiles compile a separate PSG-18 selected-face shell"}}
    grain_digest=write(root/"assets/wood_grain_field_v1.json",grain_asset)
    receipt={"schema":"ray_tracing.wood_surface_preset_receipt_v1","schema_version":1,"preset_id":preset["preset_id"],"preset_digest_sha256":preset_digest,"source_mesh_digest_sha256":a.source_mesh_digest,"field_authoring_digest_sha256":authoring_digest,"field_digest_sha256":field_digest,"field_receipt_digest_sha256":field_receipt_digest,"grain":{"asset_digest_sha256":grain_digest,"intent":preset["grain"],"geometry_claim":"field identity only"},"material":{"shared_grain_field_digest_sha256":grain_digest,"intent":preset["material_response"],"geometry_claim":"none"},"shading_normal":{"shared_grain_field_digest_sha256":grain_digest,"intent":preset["normal_response"],"geometry_claim":"none"},"grain_height_profiles":{"shared_grain_field_digest_sha256":grain_digest,"default_profile":grain_relief["default_profile"],"profiles":grain_relief["profiles"],"physical_consumer":"PSG-18 selected-face shell","executed":False},"shallow_knot_relief":{"consumer":"PSG-18 signed selected-face shell","field_digest_sha256":field_digest,"ranges":{x["id"]:x["height_or_depth_quantiles"] for x in field_receipt["populations"]}},"deep_topology":{"route":preset["deep_topology"]["route"],"requires":"separate PSG-24C explicit feature-id authorization and compile","executed":False}}
    digest=write(root/"receipts/wood_surface_preset.receipt.json",receipt); print(json.dumps({"field":str(root/"assets/surface_feature_field_v1.json"),"receipt":str(root/"receipts/wood_surface_preset.receipt.json"),"receipt_digest_sha256":digest},sort_keys=True)); return 0
if __name__=="__main__": raise SystemExit(main())

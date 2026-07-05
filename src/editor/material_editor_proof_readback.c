#include "editor/material_editor_internal.h"

#include <stdio.h>
#include <string.h>

#include "editor/material_editor_layer_model.h"
#include "material/material.h"
#include "material/material_manager.h"

static const char* material_editor_proof_texture_family(int texture_id) {
    if (texture_id == RUNTIME_MATERIAL_TEXTURE_3D_RUST) return "rust_procedural";
    if (texture_id == RUNTIME_MATERIAL_TEXTURE_3D_FOG) return "fog_procedural";
    return "solid_or_none";
}

static void material_editor_proof_describe_source_material(
    const SceneObject* obj,
    RuntimeMaterialTextureLayerKind layer_kind,
    char* out,
    size_t out_size) {
    if (!out || out_size == 0u) return;
    if (!obj) {
        snprintf(out, out_size, "none");
        return;
    }
    if (layer_kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) {
        snprintf(out,
                 out_size,
                 "layer_kind=%s material_id=%d",
                 RuntimeMaterialTextureLayerKindDisplayName(layer_kind),
                 obj->material_id);
        return;
    }
    snprintf(out,
             out_size,
             "material_id=%d texture_family=%s",
             obj->material_id,
             material_editor_proof_texture_family(obj->textureId));
}

static RuntimeMaterialTextureLayerKind material_editor_proof_active_layer_kind(
    const SceneObject* obj) {
    RuntimeMaterialTextureLayer layer;
    if (!obj) return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
    if (material_editor_use_object_layer_controls(obj) &&
        material_editor_get_active_layer(obj, NULL, &layer, NULL)) {
        return layer.kind;
    }
    if (obj->textureId == RUNTIME_MATERIAL_TEXTURE_3D_RUST) {
        return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST;
    }
    if (obj->textureId == RUNTIME_MATERIAL_TEXTURE_3D_FOG) {
        return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG;
    }
    return RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
}

static bool material_editor_proof_object_has_tint(const SceneObject* obj) {
    if (!obj) return false;
    return SceneObjectColorR(obj) < 245u ||
           SceneObjectColorG(obj) < 245u ||
           SceneObjectColorB(obj) < 245u;
}

static double material_editor_proof_layer_roughness(const RuntimeMaterialTextureLayer* layer) {
    if (!layer) return 0.0;
    if (layer->roughnessInfluence > 0.0) return layer->roughnessInfluence;
    return 0.0;
}

static void material_editor_proof_describe_glass(
    const SceneObject* obj,
    RuntimeMaterialTextureLayerKind layer_kind,
    const RuntimeMaterialTextureLayer* active_layer,
    MaterialEditorProofReadback* out_readback) {
    bool tinted = material_editor_proof_object_has_tint(obj);
    double roughness = material_editor_proof_layer_roughness(active_layer);
    if (!obj || !out_readback || obj->material_id != MATERIAL_PRESET_TRANSPARENT) return;
    out_readback->glass_proof_readback = true;
    if (layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_FOG ||
        layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES ||
        layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME ||
        layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL) {
        snprintf(out_readback->glass_proof_case,
                 sizeof(out_readback->glass_proof_case),
                 "%s glass overlay",
                 RuntimeMaterialTextureLayerKindDisplayName(layer_kind));
        snprintf(out_readback->glass_proof_package,
                 sizeof(out_readback->glass_proof_package),
                 "m4_s3_overlay_stack_response_matrix");
        snprintf(out_readback->glass_proof_coverage,
                 sizeof(out_readback->glass_proof_coverage),
                 "Existing M4-S3 overlay matrix covers fog/scratches/oil/grime response readback.");
        snprintf(out_readback->glass_missing_proof,
                 sizeof(out_readback->glass_missing_proof),
                 "Missing: combined tinted dirty glass and full physical transport/caustic proof.");
        return;
    }
    snprintf(out_readback->glass_proof_package,
             sizeof(out_readback->glass_proof_package),
             "m4_s1_glass_roughness_transmission_matrix");
    if (roughness >= 0.35) {
        snprintf(out_readback->glass_proof_case,
                 sizeof(out_readback->glass_proof_case),
                 "frosted glass");
        snprintf(out_readback->glass_proof_coverage,
                 sizeof(out_readback->glass_proof_coverage),
                 "Existing M4-S1 roughness/transmission matrix covers smooth-to-frosted glass contrast.");
    } else if (tinted) {
        snprintf(out_readback->glass_proof_case,
                 sizeof(out_readback->glass_proof_case),
                 "tinted glass");
        snprintf(out_readback->glass_proof_coverage,
                 sizeof(out_readback->glass_proof_coverage),
                 "Existing M4-S1 covers transmission/roughness route; tint-specific proof remains partial.");
    } else {
        snprintf(out_readback->glass_proof_case,
                 sizeof(out_readback->glass_proof_case),
                 "clear glass");
        snprintf(out_readback->glass_proof_coverage,
                 sizeof(out_readback->glass_proof_coverage),
                 "Existing M4-S1 matrix covers clear/smooth glass via current alpha-transparency bridge.");
    }
    snprintf(out_readback->glass_missing_proof,
             sizeof(out_readback->glass_missing_proof),
             "%s",
             tinted ? "Missing: dedicated tinted glass color/absorption proof and caustic/transport proof."
                    : "Missing: full physical glass transport, caustics, absorption color/density proof.");
}

static void material_editor_proof_describe_mirror(
    const SceneObject* obj,
    MaterialEditorProofReadback* out_readback) {
    double reflectivity = 0.0;
    double roughness = 0.0;
    double specular = 0.0;
    int tint = 0xFFFFFF;
    bool tinted = false;
    bool illuminated = animSettings.lightIntensity > 0.0;

    if (!obj || !out_readback || obj->material_id != MATERIAL_PRESET_MIRROR) return;
    if (!SceneObjectResolveMirrorResponse(obj, &reflectivity, &roughness, &specular, &tint)) {
        return;
    }
    (void)reflectivity;
    (void)specular;
    out_readback->mirror_proof_readback = true;
    snprintf(out_readback->material_family, sizeof(out_readback->material_family), "Mirror");

    if (roughness >= 0.18) {
        snprintf(out_readback->mirror_proof_case,
                 sizeof(out_readback->mirror_proof_case),
                 "rough mirror");
        snprintf(out_readback->mirror_proof_package,
                 sizeof(out_readback->mirror_proof_package),
                 "disney_v2_mirror_glossy_preservation_matrix");
        snprintf(out_readback->mirror_proof_coverage,
                 sizeof(out_readback->mirror_proof_coverage),
                 "Existing mirror/glossy preservation coverage tracks rough reflection diagnostics and denoise preservation.");
        snprintf(out_readback->mirror_missing_proof,
                 sizeof(out_readback->mirror_missing_proof),
                 "Missing: full user-facing rough mirror visual package tied to compact Proof pane state.");
        return;
    }

    tinted = (tint & 0x00FFFFFF) != 0x00FFFFFF || material_editor_proof_object_has_tint(obj);
    if (tinted) {
        snprintf(out_readback->mirror_proof_case,
                 sizeof(out_readback->mirror_proof_case),
                 "tinted mirror");
        snprintf(out_readback->mirror_proof_package,
                 sizeof(out_readback->mirror_proof_package),
                 "su4_mirror_surface_unification_matrix");
        snprintf(out_readback->mirror_proof_coverage,
                 sizeof(out_readback->mirror_proof_coverage),
                 "SU4 mirror surface-unification coverage proves reflected geometry parity; tint readback is editor/payload-backed.");
        snprintf(out_readback->mirror_missing_proof,
                 sizeof(out_readback->mirror_missing_proof),
                 "Missing: dedicated tinted mirror color-proof package across direct/material/Disney routes.");
        return;
    }

    if (illuminated) {
        snprintf(out_readback->mirror_proof_case,
                 sizeof(out_readback->mirror_proof_case),
                 "illuminated mirror dominance");
        snprintf(out_readback->mirror_proof_package,
                 sizeof(out_readback->mirror_proof_package),
                 "m10_s4_illuminated_mirror_dominance_regression");
        snprintf(out_readback->mirror_proof_coverage,
                 sizeof(out_readback->mirror_proof_coverage),
                 "M10-S4 source proof keeps reflected geometry dominant under bright direct light.");
        snprintf(out_readback->mirror_missing_proof,
                 sizeof(out_readback->mirror_missing_proof),
                 "Missing: packaged visual matrix for illuminated default Mirror after the Disney combiner repair.");
        return;
    }

    snprintf(out_readback->mirror_proof_case,
             sizeof(out_readback->mirror_proof_case),
             "default mirror");
    snprintf(out_readback->mirror_proof_package,
             sizeof(out_readback->mirror_proof_package),
             "su4_mirror_surface_unification_matrix");
    snprintf(out_readback->mirror_proof_coverage,
             sizeof(out_readback->mirror_proof_coverage),
             "SU4 plane/prism/runtime-mesh matrix covers default mirror dominance and reflected geometry parity.");
    snprintf(out_readback->mirror_missing_proof,
             sizeof(out_readback->mirror_missing_proof),
             "Missing: compact editor-launched Mirror proof package; current Proof pane is readback-only.");
}

static void material_editor_proof_describe_metal(
    const SceneObject* obj,
    RuntimeMaterialTextureLayerKind layer_kind,
    const RuntimeMaterialTextureLayer* active_layer,
    MaterialEditorProofReadback* out_readback) {
    const Material* material = NULL;
    double roughness = 0.0;
    bool tinted = material_editor_proof_object_has_tint(obj);
    if (!obj || !out_readback || obj->material_id != MATERIAL_PRESET_ROUGH_METAL) return;
    material = MaterialManagerGet(obj->material_id);
    roughness = material ? material->roughness : 0.6;
    if (active_layer && active_layer->roughnessInfluence > 0.0) {
        roughness = active_layer->roughnessInfluence;
    }
    out_readback->metal_proof_readback = true;
    snprintf(out_readback->material_family, sizeof(out_readback->material_family), "Metal");

    if (layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_RUST ||
        layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_GRIME ||
        layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_OIL ||
        layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SCRATCHES ||
        layer_kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_EDGE_WEAR) {
        snprintf(out_readback->metal_proof_case,
                 sizeof(out_readback->metal_proof_case),
                 "%s metal overlay",
                 RuntimeMaterialTextureLayerKindDisplayName(layer_kind));
        snprintf(out_readback->metal_proof_package,
                 sizeof(out_readback->metal_proof_package),
                 "m11_s5_material_family_preview_grid");
        snprintf(out_readback->metal_proof_coverage,
                 sizeof(out_readback->metal_proof_coverage),
                 "M11-S5 visual grid covers damaged Metal overlays, with M4-S3 retaining source-backed overlay semantics.");
        snprintf(out_readback->metal_missing_proof,
                 sizeof(out_readback->metal_missing_proof),
                 "Missing: first-class metallic payload promotion; metallic still guarded.");
        return;
    }

    if (roughness <= 0.25) {
        snprintf(out_readback->metal_proof_case,
                 sizeof(out_readback->metal_proof_case),
                 "polished metal");
        snprintf(out_readback->metal_proof_package,
                 sizeof(out_readback->metal_proof_package),
                 "m11_s5_material_family_preview_grid");
        snprintf(out_readback->metal_proof_coverage,
                 sizeof(out_readback->metal_proof_coverage),
                 "M11-S5 visual grid covers polished Metal contrast; Disney v2 diagnostics retain lobe-order evidence.");
        snprintf(out_readback->metal_missing_proof,
                 sizeof(out_readback->metal_missing_proof),
                 "Missing: first-class metallic payload promotion.");
        return;
    }

    if (tinted) {
        snprintf(out_readback->metal_proof_case,
                 sizeof(out_readback->metal_proof_case),
                 "tinted rough metal");
        snprintf(out_readback->metal_proof_package,
                 sizeof(out_readback->metal_proof_package),
                 "m11_s5_material_family_preview_grid");
        snprintf(out_readback->metal_proof_coverage,
                 sizeof(out_readback->metal_proof_coverage),
                 "M11-S5 visual grid covers tinted/colored Metal contrast; M4-S2 retains reflect/specular evidence.");
        snprintf(out_readback->metal_missing_proof,
                 sizeof(out_readback->metal_missing_proof),
                 "Missing: first-class metallic payload promotion; metallic remains guarded.");
        return;
    }

    snprintf(out_readback->metal_proof_case,
             sizeof(out_readback->metal_proof_case),
             "default rough metal");
    snprintf(out_readback->metal_proof_package,
             sizeof(out_readback->metal_proof_package),
             "m11_s5_material_family_preview_grid");
    snprintf(out_readback->metal_proof_coverage,
             sizeof(out_readback->metal_proof_coverage),
             "M11-S5 visual grid covers rough Metal contrast; M4-S2 retains no-accidental-metallic evidence.");
    snprintf(out_readback->metal_missing_proof,
             sizeof(out_readback->metal_missing_proof),
             "Missing: first-class metallic payload promotion.");
}

void MaterialEditorFormatProofReadbackStatus(const MaterialEditorProofReadback* readback,
                                             char* out,
                                             size_t out_size) {
    if (!out || out_size == 0u) return;
    if (!readback || !readback->m4_request_compatible) {
        snprintf(out, out_size, "M4 proof route unavailable");
        return;
    }
    snprintf(out,
             out_size,
             "%s | %s | %s",
             readback->route_primary,
             readback->destination_label,
             readback->route_status);
}

bool MaterialEditorBuildFocusedProofReadback(MaterialEditorProofReadback* out_readback) {
    SceneObject* obj = material_editor_focused_object();
    RuntimeMaterialTextureLayerKind layer_kind = RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE;
    RuntimeMaterialTextureLayer active_layer = {0};
    MaterialEditorMutationDestination destination =
        MaterialEditorMutationDestinationForFocusedTextureControls();
    MaterialEditorPanelGroup group =
        MaterialEditorPanelGroupForMutationDestination(destination);
    int focused_index = MaterialEditorResolveFocusedObjectIndex();
    if (!out_readback) return false;
    memset(out_readback, 0, sizeof(*out_readback));
    if (!obj || focused_index < 0) return false;
    layer_kind = material_editor_proof_active_layer_kind(obj);
    if (!material_editor_get_active_layer(obj, NULL, &active_layer, NULL)) {
        memset(&active_layer, 0, sizeof(active_layer));
    }
    snprintf(out_readback->request_schema,
             sizeof(out_readback->request_schema),
             "%s",
             MATERIAL_EDITOR_PROOF_SCHEMA);
    snprintf(out_readback->summary_schema,
             sizeof(out_readback->summary_schema),
             "%s",
             MATERIAL_EDITOR_PROOF_SUMMARY_SCHEMA);
    snprintf(out_readback->proof_id,
             sizeof(out_readback->proof_id),
             "m5_s3_material_editor_focused_readback");
    snprintf(out_readback->phase, sizeof(out_readback->phase), "M5-S3");
    snprintf(out_readback->route_primary,
             sizeof(out_readback->route_primary),
             "headless_material_preview");
    snprintf(out_readback->route_status,
             sizeof(out_readback->route_status),
             "request_shape_only_not_launched");
    snprintf(out_readback->request_path, sizeof(out_readback->request_path), "request.json");
    snprintf(out_readback->summary_path, sizeof(out_readback->summary_path), "summary.json");
    snprintf(out_readback->index_path, sizeof(out_readback->index_path), "index.md");
    snprintf(out_readback->image_path, sizeof(out_readback->image_path), "preview.bmp");
    snprintf(out_readback->image_status,
             sizeof(out_readback->image_status),
             "not_generated_by_editor");
    snprintf(out_readback->destination_label,
             sizeof(out_readback->destination_label),
             "%s",
             MaterialEditorMutationDestinationLabel(destination));
    snprintf(out_readback->panel_group_label,
             sizeof(out_readback->panel_group_label),
             "%s",
             MaterialEditorPanelGroupLabel(group));
    snprintf(out_readback->row_id,
             sizeof(out_readback->row_id),
             "focused_object_%d_%s",
             focused_index,
             out_readback->destination_label);
    snprintf(out_readback->label,
             sizeof(out_readback->label),
             "Material editor focused %s readback",
             out_readback->destination_label);
    snprintf(out_readback->material_family,
             sizeof(out_readback->material_family),
             "%s",
             layer_kind != RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE
                 ? RuntimeMaterialTextureLayerKindDisplayName(layer_kind)
                 : material_editor_proof_texture_family(obj->textureId));
    material_editor_proof_describe_source_material(obj,
                                                   layer_kind,
                                                   out_readback->source_material,
                                                   sizeof(out_readback->source_material));
    material_editor_proof_describe_glass(obj, layer_kind, &active_layer, out_readback);
    material_editor_proof_describe_mirror(obj, out_readback);
    material_editor_proof_describe_metal(obj, layer_kind, &active_layer, out_readback);
    snprintf(out_readback->expected_behavior,
             sizeof(out_readback->expected_behavior),
             "Editor emits M4-compatible proof request/readback labels only; no render launch");
    snprintf(out_readback->deferred_status,
             sizeof(out_readback->deferred_status),
             "image_generation_deferred_to_headless_material_preview");
    out_readback->m4_request_compatible = true;
    out_readback->launch_deferred = true;
    return true;
}

bool MaterialEditorPrimeProofReadbackForFocused(void) {
    bool ok = MaterialEditorBuildFocusedProofReadback(&s_material_editor_proof_readback);
    s_material_editor_proof_readback_valid = ok;
    MaterialEditorFormatProofReadbackStatus(ok ? &s_material_editor_proof_readback : NULL,
                                            s_material_editor_proof_readback_status,
                                            sizeof(s_material_editor_proof_readback_status));
    return ok;
}

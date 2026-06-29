#include "editor/material_editor_internal.h"

#include <stdio.h>
#include <string.h>

#include "editor/material_editor_layer_model.h"

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
    MaterialEditorMutationDestination destination =
        MaterialEditorMutationDestinationForFocusedTextureControls();
    MaterialEditorPanelGroup group =
        MaterialEditorPanelGroupForMutationDestination(destination);
    int focused_index = MaterialEditorResolveFocusedObjectIndex();
    if (!out_readback) return false;
    memset(out_readback, 0, sizeof(*out_readback));
    if (!obj || focused_index < 0) return false;
    layer_kind = material_editor_proof_active_layer_kind(obj);
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

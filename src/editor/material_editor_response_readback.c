#include "editor/material_editor_internal.h"

#include <stdio.h>
#include <string.h>

#include "editor/material_editor_layer_model.h"
#include "material/material.h"
#include "material/material_manager.h"

static double material_editor_response_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static MaterialEditorResponseFamily material_editor_response_family_from_material_id(
    int material_id) {
    if (material_id == MATERIAL_PRESET_TRANSPARENT) {
        return MATERIAL_EDITOR_RESPONSE_FAMILY_GLASS;
    }
    if (material_id == MATERIAL_PRESET_ROUGH_METAL ||
        material_id == MATERIAL_PRESET_MIRROR) {
        return MATERIAL_EDITOR_RESPONSE_FAMILY_METAL;
    }
    if (material_id == MATERIAL_PRESET_EMISSIVE) {
        return MATERIAL_EDITOR_RESPONSE_FAMILY_EMISSIVE;
    }
    return MATERIAL_EDITOR_RESPONSE_FAMILY_GENERIC;
}

static void material_editor_response_add_row(MaterialEditorResponseReadback* readback,
                                             MaterialEditorResponseField field,
                                             const char* label,
                                             const char* value,
                                             const char* note,
                                             MaterialEditorResponseFieldState state) {
    MaterialEditorResponseRow* row = NULL;
    if (!readback || readback->row_count < 0 ||
        readback->row_count >= MATERIAL_EDITOR_RESPONSE_MAX_ROWS ||
        !label || !value || !note) {
        return;
    }
    row = &readback->rows[readback->row_count];
    memset(row, 0, sizeof(*row));
    row->field = field;
    snprintf(row->label, sizeof(row->label), "%s", label);
    snprintf(row->value, sizeof(row->value), "%s", value);
    snprintf(row->note, sizeof(row->note), "%s", note);
    row->state = state;
    if (state == MATERIAL_EDITOR_RESPONSE_FIELD_GUARDED) {
        readback->has_guarded_fields = true;
    }
    readback->row_count += 1;
}

static void material_editor_response_add_numeric_row(
    MaterialEditorResponseReadback* readback,
    MaterialEditorResponseField field,
    const char* label,
    double value,
    const char* note,
    MaterialEditorResponseFieldState state) {
    char value_text[48];
    snprintf(value_text, sizeof(value_text), "%.2f", value);
    material_editor_response_add_row(readback, field, label, value_text, note, state);
}

static void material_editor_response_base_values(const SceneObject* obj,
                                                 const Material* material,
                                                 double* out_transmission,
                                                 double* out_roughness,
                                                 double* out_reflectivity,
                                                 double* out_specular) {
    if (out_transmission) {
        *out_transmission = material ? material_editor_response_clamp01(
                                           material->transparency *
                                           material_editor_response_clamp01(obj ? obj->alpha : 1.0))
                                     : 0.0;
    }
    if (out_roughness) {
        *out_roughness = material ? material_editor_response_clamp01(material->roughness)
                                  : material_editor_response_clamp01(obj ? obj->roughness : 0.5);
    }
    if (out_reflectivity) {
        *out_reflectivity = material ? material_editor_response_clamp01(material->reflectivity)
                                     : material_editor_response_clamp01(obj ? obj->reflectivity : 0.0);
    }
    if (out_specular) {
        *out_specular = material ? material_editor_response_clamp01(material->specular)
                                 : 0.0;
    }
}

static bool material_editor_response_active_layer_values(const SceneObject* obj,
                                                         double* io_roughness,
                                                         double* io_reflectivity,
                                                         double* io_specular,
                                                         bool* out_has_layer) {
    RuntimeMaterialTextureLayer layer = {0};
    bool has_layer = false;
    if (out_has_layer) *out_has_layer = false;
    if (!obj) return false;
    has_layer = material_editor_get_active_layer(obj, NULL, &layer, NULL);
    if (!has_layer) return false;
    if (out_has_layer) *out_has_layer = true;
    if (io_roughness && layer.roughnessInfluence > 1e-9) {
        *io_roughness = material_editor_response_clamp01(layer.roughnessInfluence);
    }
    if (io_reflectivity && layer.reflectivityInfluence > 1e-9) {
        *io_reflectivity = material_editor_response_clamp01(layer.reflectivityInfluence);
    }
    if (io_specular && layer.specularInfluence > 1e-9) {
        *io_specular = material_editor_response_clamp01(layer.specularInfluence);
    }
    return true;
}

static void material_editor_response_tint_value(const SceneObject* obj,
                                                const Material* material,
                                                char* out,
                                                size_t out_size) {
    double r = material ? material->base_color.x : 1.0;
    double g = material ? material->base_color.y : 1.0;
    double b = material ? material->base_color.z : 1.0;
    if (!out || out_size == 0u) return;
    if (obj) {
        r = (double)SceneObjectColorR(obj) / 255.0;
        g = (double)SceneObjectColorG(obj) / 255.0;
        b = (double)SceneObjectColorB(obj) / 255.0;
    }
    snprintf(out,
             out_size,
             "%.2f %.2f %.2f",
             material_editor_response_clamp01(r),
             material_editor_response_clamp01(g),
             material_editor_response_clamp01(b));
}

static bool material_editor_build_glass_response(MaterialEditorResponseReadback* readback,
                                                 const SceneObject* obj,
                                                 const Material* material) {
    double transmission = 0.0;
    double roughness = 0.0;
    double reflectivity = 0.0;
    double specular = 0.0;
    double ior = 1.0;
    double absorption_distance = 1.0;
    bool thin_walled = false;
    bool has_layer = false;
    char tint[48];
    if (!readback || !obj) return false;
    material_editor_response_base_values(obj,
                                         material,
                                         &transmission,
                                         &roughness,
                                         &reflectivity,
                                         &specular);
    material_editor_response_active_layer_values(obj,
                                                 &roughness,
                                                 &reflectivity,
                                                 &specular,
                                                 &has_layer);
    SceneObjectResolveGlassTransport(obj,
                                     &transmission,
                                     &ior,
                                     &absorption_distance,
                                     &thin_walled);
    material_editor_response_tint_value(obj, material, tint, sizeof(tint));
    readback->family = MATERIAL_EDITOR_RESPONSE_FAMILY_GLASS;
    readback->family_specific = true;
    snprintf(readback->title, sizeof(readback->title), "Glass Response");
    snprintf(readback->subtitle,
             sizeof(readback->subtitle),
             "Preset transport plus selected-layer surface response");
    snprintf(readback->route_label,
             sizeof(readback->route_label),
             "%s | %s",
             has_layer ? "selected layer" : "preset fallback",
             "glass family matrix");
    material_editor_response_add_numeric_row(readback,
                                             MATERIAL_EDITOR_RESPONSE_FIELD_TRANSMISSION,
                                             "Trans",
                                             transmission,
                                             obj->hasGlassTransportOverride ? "object transport"
                                                                            : "preset transport",
                                             MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE);
    material_editor_response_add_numeric_row(readback,
                                             MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS,
                                             "Rough",
                                             roughness,
                                             has_layer ? "selected layer" : "preset",
                                             MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE);
    material_editor_response_add_numeric_row(readback,
                                             MATERIAL_EDITOR_RESPONSE_FIELD_IOR,
                                             "IOR",
                                             ior,
                                             obj->hasGlassTransportOverride ? "object transport"
                                                                            : "preset transport",
                                             MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE);
    material_editor_response_add_numeric_row(readback,
                                             MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY,
                                             "Reflect",
                                             reflectivity,
                                             has_layer ? "compat layer" : "compat preset",
                                             has_layer ? MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE
                                                       : MATERIAL_EDITOR_RESPONSE_FIELD_READBACK);
    material_editor_response_add_numeric_row(readback,
                                             MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR,
                                             "Spec",
                                             specular,
                                             has_layer ? "lobe layer" : "lobe preset",
                                             has_layer ? MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE
                                                       : MATERIAL_EDITOR_RESPONSE_FIELD_READBACK);
    material_editor_response_add_row(readback,
                                     MATERIAL_EDITOR_RESPONSE_FIELD_TINT,
                                     "Tint",
                                     tint,
                                     "object color bridge",
                                     MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE);
    material_editor_response_add_numeric_row(readback,
                                             MATERIAL_EDITOR_RESPONSE_FIELD_ABSORPTION,
                                             "Absorb",
                                             absorption_distance,
                                             obj->hasGlassTransportOverride ? "object distance"
                                                                            : "preset distance",
                                             MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE);
    material_editor_response_add_row(readback,
                                     MATERIAL_EDITOR_RESPONSE_FIELD_THIN_WALLED,
                                     "Thin",
                                     thin_walled ? "on" : "off",
                                     obj->hasGlassTransportOverride ? "object mode" : "preset mode",
                                     MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE);
    return true;
}

static bool material_editor_build_generic_response(MaterialEditorResponseReadback* readback,
                                                   const SceneObject* obj,
                                                   const Material* material,
                                                   MaterialEditorResponseFamily family) {
    double transmission = 0.0;
    double roughness = 0.0;
    double reflectivity = 0.0;
    double specular = 0.0;
    bool has_layer = false;
    if (!readback || !obj) return false;
    material_editor_response_base_values(obj,
                                         material,
                                         &transmission,
                                         &roughness,
                                         &reflectivity,
                                         &specular);
    material_editor_response_active_layer_values(obj,
                                                 &roughness,
                                                 &reflectivity,
                                                 &specular,
                                                 &has_layer);
    readback->family = family;
    readback->family_specific = false;
    snprintf(readback->title,
             sizeof(readback->title),
             "%s Response",
             MaterialEditorResponseFamilyLabel(family));
    snprintf(readback->subtitle,
             sizeof(readback->subtitle),
             "Generic selected-layer response until this family gets a pane");
    snprintf(readback->route_label,
             sizeof(readback->route_label),
             "%s | generic matrix fallback",
             has_layer ? "selected layer" : "preset fallback");
    material_editor_response_add_numeric_row(readback,
                                             MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS,
                                             "Rough",
                                             roughness,
                                             has_layer ? "selected layer" : "preset",
                                             MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE);
    material_editor_response_add_numeric_row(readback,
                                             MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY,
                                             "Reflect",
                                             reflectivity,
                                             "compat readback",
                                             MATERIAL_EDITOR_RESPONSE_FIELD_READBACK);
    material_editor_response_add_numeric_row(readback,
                                             MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR,
                                             "Spec",
                                             specular,
                                             "lobe readback",
                                             MATERIAL_EDITOR_RESPONSE_FIELD_READBACK);
    material_editor_response_add_numeric_row(readback,
                                             MATERIAL_EDITOR_RESPONSE_FIELD_TRANSMISSION,
                                             "Trans",
                                             transmission,
                                             "family deferred",
                                             MATERIAL_EDITOR_RESPONSE_FIELD_GUARDED);
    return true;
}

const char* MaterialEditorResponseFamilyLabel(MaterialEditorResponseFamily family) {
    if (family == MATERIAL_EDITOR_RESPONSE_FAMILY_GLASS) return "Glass";
    if (family == MATERIAL_EDITOR_RESPONSE_FAMILY_METAL) return "Metal";
    if (family == MATERIAL_EDITOR_RESPONSE_FAMILY_EMISSIVE) return "Emissive";
    return "Material";
}

const char* MaterialEditorResponseFieldStateLabel(MaterialEditorResponseFieldState state) {
    if (state == MATERIAL_EDITOR_RESPONSE_FIELD_EDITABLE) return "edit";
    if (state == MATERIAL_EDITOR_RESPONSE_FIELD_GUARDED) return "guard";
    return "read";
}

const char* MaterialEditorResponseFieldLabel(MaterialEditorResponseField field) {
    if (field == MATERIAL_EDITOR_RESPONSE_FIELD_TRANSMISSION) return "transmission";
    if (field == MATERIAL_EDITOR_RESPONSE_FIELD_ROUGHNESS) return "roughness";
    if (field == MATERIAL_EDITOR_RESPONSE_FIELD_IOR) return "ior";
    if (field == MATERIAL_EDITOR_RESPONSE_FIELD_REFLECTIVITY) return "reflectivity";
    if (field == MATERIAL_EDITOR_RESPONSE_FIELD_SPECULAR) return "specular";
    if (field == MATERIAL_EDITOR_RESPONSE_FIELD_TINT) return "tint";
    if (field == MATERIAL_EDITOR_RESPONSE_FIELD_ABSORPTION) return "absorption";
    if (field == MATERIAL_EDITOR_RESPONSE_FIELD_THIN_WALLED) return "thin_walled";
    return "none";
}

bool MaterialEditorBuildResponseReadback(MaterialEditorResponseReadback* out_readback) {
    SceneObject* obj = material_editor_focused_object();
    MaterialEditorResponseFamily family = MATERIAL_EDITOR_RESPONSE_FAMILY_GENERIC;
    const Material* material = NULL;
    if (!out_readback) return false;
    memset(out_readback, 0, sizeof(*out_readback));
    if (!obj) {
        snprintf(out_readback->title, sizeof(out_readback->title), "No Response");
        snprintf(out_readback->subtitle,
                 sizeof(out_readback->subtitle),
                 "Select an object before editing material response.");
        return false;
    }
    material = MaterialManagerGet(obj->material_id);
    family = material_editor_response_family_from_material_id(obj->material_id);
    if (family == MATERIAL_EDITOR_RESPONSE_FAMILY_GLASS) {
        return material_editor_build_glass_response(out_readback, obj, material);
    }
    return material_editor_build_generic_response(out_readback, obj, material, family);
}

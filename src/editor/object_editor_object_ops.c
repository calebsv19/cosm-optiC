#include "editor/object_editor_object_ops.h"

#include <math.h>

#include "material/material_manager.h"

static double object_editor_object_ops_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

void ObjectEditorObjectAssignMaterial(SceneObject* obj, int material_id) {
    const Material* preset = NULL;
    if (!obj) return;
    if (material_id < 0 || material_id >= MaterialManagerCount()) return;
    preset = MaterialManagerGet(material_id);
    obj->material_id = material_id;
    SceneObjectClearGlassTransportOverride(obj);
    if (preset) {
        obj->reflectivity = object_editor_object_ops_clamp01(preset->reflectivity);
        obj->roughness = object_editor_object_ops_clamp01(preset->roughness);
        obj->emissiveStrength =
            (preset->emissive.x > 0.0f || preset->emissive.y > 0.0f ||
             preset->emissive.z > 0.0f)
                ? 1.0
                : 0.0;
    }
    MarkObjectDirty(obj);
}

void ObjectEditorObjectAssignColor(SceneObject* obj, int packed_color) {
    if (!obj) return;
    obj->color = packed_color & 0xFFFFFF;
    MarkObjectDirty(obj);
}

void ObjectEditorObjectAssignAlpha(SceneObject* obj, double alpha) {
    if (!obj) return;
    obj->alpha = object_editor_object_ops_clamp01(alpha);
    MarkObjectDirty(obj);
}

void ObjectEditorObjectAssignEmissiveStrength(SceneObject* obj, double emissive_strength) {
    if (!obj) return;
    obj->emissiveStrength = object_editor_object_ops_clamp01(emissive_strength);
    MarkObjectDirty(obj);
}

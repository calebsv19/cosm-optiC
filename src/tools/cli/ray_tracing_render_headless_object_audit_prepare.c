#include "tools/ray_tracing_render_headless_internal.h"

#include "config/config_manager.h"
#include "import/runtime_scene_bridge.h"

#include <stdio.h>
#include <string.h>

static int ray_tracing_headless_positive_max_int(int a, int b) {
    return (a > b) ? a : b;
}

static int ray_tracing_headless_ceil_div_int(int value, int divisor) {
    if (divisor <= 0) return value;
    if (value <= 0) return 0;
    return (value + divisor - 1) / divisor;
}

static RayTracingHeadlessObjectAuditEntry *ray_tracing_headless_object_audit_ensure_entry(
    RayTracingHeadlessPreflight *preflight,
    int scene_object_index) {
    RayTracingHeadlessObjectAuditEntry *free_entry = NULL;
    char object_id[64] = {0};
    if (!preflight || scene_object_index < 0) return NULL;
    for (int i = 0; i < RAY_TRACING_HEADLESS_OBJECT_AUDIT_MAX; ++i) {
        RayTracingHeadlessObjectAuditEntry *entry = &preflight->object_audit[i];
        if (entry->used && entry->scene_object_index == scene_object_index) {
            return entry;
        }
        if (!entry->used && !free_entry) {
            free_entry = entry;
        }
    }
    if (!free_entry) return NULL;
    memset(free_entry, 0, sizeof(*free_entry));
    free_entry->used = true;
    free_entry->scene_object_index = scene_object_index;
    if (runtime_scene_bridge_get_last_object_id_for_scene_index(scene_object_index,
                                                                object_id,
                                                                sizeof(object_id))) {
        snprintf(free_entry->object_id, sizeof(free_entry->object_id), "%s", object_id);
    }
    if (scene_object_index >= 0 && scene_object_index < sceneSettings.objectCount) {
        snprintf(free_entry->object_type,
                 sizeof(free_entry->object_type),
                 "%s",
                 sceneSettings.sceneObjects[scene_object_index].type);
        free_entry->material_id = sceneSettings.sceneObjects[scene_object_index].material_id;
        free_entry->alpha = sceneSettings.sceneObjects[scene_object_index].alpha;
        free_entry->reflectivity = sceneSettings.sceneObjects[scene_object_index].reflectivity;
        free_entry->roughness = sceneSettings.sceneObjects[scene_object_index].roughness;
        free_entry->emissive_strength =
            sceneSettings.sceneObjects[scene_object_index].emissiveStrength;
        free_entry->texture_id = sceneSettings.sceneObjects[scene_object_index].textureId;
        free_entry->texture_strength =
            sceneSettings.sceneObjects[scene_object_index].textureStrength;
        free_entry->texture_scale = sceneSettings.sceneObjects[scene_object_index].textureScale;
        free_entry->texture_offset_u =
            sceneSettings.sceneObjects[scene_object_index].textureOffsetU;
        free_entry->texture_offset_v =
            sceneSettings.sceneObjects[scene_object_index].textureOffsetV;
        free_entry->texture_seed = sceneSettings.sceneObjects[scene_object_index].textureSeed;
        free_entry->texture_pattern_mode =
            sceneSettings.sceneObjects[scene_object_index].texturePatternMode;
        free_entry->texture_coverage =
            sceneSettings.sceneObjects[scene_object_index].textureCoverage;
        free_entry->texture_grain =
            sceneSettings.sceneObjects[scene_object_index].textureGrain;
        free_entry->texture_edge_softness =
            sceneSettings.sceneObjects[scene_object_index].textureEdgeSoftness;
        free_entry->texture_contrast =
            sceneSettings.sceneObjects[scene_object_index].textureContrast;
        free_entry->texture_flow =
            sceneSettings.sceneObjects[scene_object_index].textureFlow;
        free_entry->texture_color_depth =
            sceneSettings.sceneObjects[scene_object_index].textureColorDepth;
        free_entry->texture_surface_damage =
            sceneSettings.sceneObjects[scene_object_index].textureSurfaceDamage;
        free_entry->packed_color = sceneSettings.sceneObjects[scene_object_index].color;
    } else {
        free_entry->material_id = -1;
    }
    preflight->object_audit_count += 1;
    return free_entry;
}

static void ray_tracing_headless_object_audit_note_primitive_center(
    RayTracingHeadlessObjectAuditEntry *entry,
    const RuntimePrimitive3D *primitive) {
    Vec3 center = vec3(0.0, 0.0, 0.0);
    if (!entry || !primitive) return;
    switch (primitive->kind) {
        case RUNTIME_PRIMITIVE_3D_KIND_PLANE:
            center = primitive->shape.plane.origin;
            break;
        case RUNTIME_PRIMITIVE_3D_KIND_RECT_PRISM:
            center = primitive->shape.rectPrism.origin;
            break;
        default:
            return;
    }
    entry->center_x += center.x;
    entry->center_y += center.y;
    entry->center_z += center.z;
}

void ray_tracing_headless_audit_prepared_frame(
    RayTracingHeadlessPreflight *preflight,
    const RuntimeNative3DPreparedFrame *frame,
    const RayTracingAgentRenderRequest *request) {
    int max_dimension = 0;
    int source_max_dimension = 0;
    int stride = 1;
    int audit_width = 0;
    int audit_height = 0;
    if (!preflight || !frame || !frame->valid) return;

    preflight->object_audit_enabled = request ? request->object_audit_enabled : true;
    max_dimension = request ? request->object_audit_max_dimension : 160;
    if (max_dimension < 16) max_dimension = 16;
    if (!preflight->object_audit_enabled) {
        preflight->object_audit_scale_factor = 0;
        return;
    }
    source_max_dimension = ray_tracing_headless_positive_max_int(frame->width, frame->height);
    if (source_max_dimension > max_dimension) {
        stride = ray_tracing_headless_ceil_div_int(source_max_dimension, max_dimension);
        if (stride < 1) stride = 1;
    }
    audit_width = ray_tracing_headless_ceil_div_int(frame->width, stride);
    audit_height = ray_tracing_headless_ceil_div_int(frame->height, stride);
    preflight->object_audit_stride_x = stride;
    preflight->object_audit_stride_y = stride;
    preflight->object_audit_width = audit_width;
    preflight->object_audit_height = audit_height;
    preflight->object_audit_scale_factor = stride;

    for (int i = 0; i < sceneSettings.objectCount; ++i) {
        (void)ray_tracing_headless_object_audit_ensure_entry(preflight, i);
    }

    for (int i = 0; i < frame->scene.primitiveCount; ++i) {
        const RuntimePrimitive3D *primitive = &frame->scene.primitives[i];
        RayTracingHeadlessObjectAuditEntry *entry =
            ray_tracing_headless_object_audit_ensure_entry(preflight,
                                                           primitive->source.sceneObjectIndex);
        if (!entry) continue;
        entry->primitive_count += 1;
        ray_tracing_headless_object_audit_note_primitive_center(entry, primitive);
    }

    for (int i = 0; i < frame->scene.triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D *triangle = &frame->scene.triangleMesh.triangles[i];
        RayTracingHeadlessObjectAuditEntry *entry =
            ray_tracing_headless_object_audit_ensure_entry(preflight,
                                                           triangle->sceneObjectIndex);
        if (!entry) continue;
        entry->triangle_count += 1;
    }

    for (int y = 0; y < frame->height; y += stride) {
        for (int x = 0; x < frame->width; x += stride) {
            HitInfo3D hit = {0};
            Ray3D ray = RuntimeCameraProjector3D_MakePrimaryRay(&frame->projector,
                                                                (double)x,
                                                                (double)y);
            preflight->object_audit_sample_count += 1;
            if (!RuntimeRay3D_TraceSceneFirstHit(&frame->scene, &ray, 1e-6, 1.0e9, &hit)) {
                continue;
            }
            if (hit.sceneObjectIndex < 0) continue;
            RayTracingHeadlessObjectAuditEntry *entry =
                ray_tracing_headless_object_audit_ensure_entry(preflight,
                                                               hit.sceneObjectIndex);
            if (!entry) continue;
            entry->primary_hit_pixels += stride * stride;
        }
    }

    for (int i = 0; i < RAY_TRACING_HEADLESS_OBJECT_AUDIT_MAX; ++i) {
        RayTracingHeadlessObjectAuditEntry *entry = &preflight->object_audit[i];
        Vec3 center = vec3(0.0, 0.0, 0.0);
        if (!entry->used || entry->primitive_count <= 0) continue;
        center.x = entry->center_x / (double)entry->primitive_count;
        center.y = entry->center_y / (double)entry->primitive_count;
        center.z = entry->center_z / (double)entry->primitive_count;
        entry->center_x = center.x;
        entry->center_y = center.y;
        entry->center_z = center.z;
        entry->center_projectable =
            RuntimeCameraProjector3D_ProjectPoint(&frame->projector,
                                                  center,
                                                  &entry->center_screen_x,
                                                  &entry->center_screen_y,
                                                  &entry->center_camera_depth,
                                                  &entry->center_inside_viewport);
    }
}

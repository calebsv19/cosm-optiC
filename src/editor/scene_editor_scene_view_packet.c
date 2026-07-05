#include "editor/scene_editor_scene_view_packet.h"

#include <json-c/json.h>
#include <math.h>
#include <string.h>

#include "config/config_manager.h"
#include "editor/material_preview_surface_eval.h"
#include "editor/scene_editor_material_preview_internal.h"
#include "material/material.h"
#include "render/runtime_scene_3d.h"

static double scene_view_packet_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static unsigned char scene_view_packet_byte_from_unit(double value) {
    value = scene_view_packet_clamp01(value);
    return (unsigned char)lround(value * 255.0);
}

const char* SceneEditorSceneViewPreviewQualityLabel(
    SceneEditorSceneViewPreviewQuality quality) {
    return core_scene_view_preview_quality_name((CoreSceneViewPreviewQuality)quality);
}

const char* SceneEditorSceneViewDegradedReasonLabel(
    SceneEditorSceneViewDegradedReason reason) {
    return core_scene_view_degraded_reason_name((CoreSceneViewDegradedReason)reason);
}

static void scene_view_packet_pick_id_from_address(
    const SceneEditorMaterialPreviewTriangleAddress* address,
    SceneEditorSceneViewPickId* out_pick_id) {
    if (!address || !out_pick_id) return;
    out_pick_id->sceneObjectIndex = address->sceneObjectIndex;
    out_pick_id->primitiveIndex = address->primitiveIndex;
    out_pick_id->triangleIndex = address->triangleIndex;
    out_pick_id->localTriangleIndex = address->localTriangleIndex;
    out_pick_id->faceGroupIndex = address->faceGroupIndex;
}

static unsigned int scene_view_packet_display_flags(const SceneObject* object,
                                                    const RuntimeMaterialSurfaceEval* eval) {
    unsigned int flags = 0u;
    if (!object) return flags;
    if (eval && eval->transparency > 0.01) {
        flags |= SCENE_EDITOR_SCENE_VIEW_DISPLAY_TRANSPARENT;
    }
    if (object->material_id == MATERIAL_PRESET_TRANSPARENT || object->alpha < 0.99) {
        flags |= SCENE_EDITOR_SCENE_VIEW_DISPLAY_TRANSPARENT;
    }
    if (object->material_id == MATERIAL_PRESET_EMISSIVE || object->emissiveStrength > 0.01) {
        flags |= SCENE_EDITOR_SCENE_VIEW_DISPLAY_EMISSIVE;
    }
    if (object->material_id == MATERIAL_PRESET_MIRROR || object->reflectivity > 0.85) {
        flags |= SCENE_EDITOR_SCENE_VIEW_DISPLAY_MIRROR;
    }
    if ((eval && eval->textureMask > 0.01) ||
        (object->textureId != 0 && object->textureStrength > 0.01)) {
        flags |= SCENE_EDITOR_SCENE_VIEW_DISPLAY_TEXTURED;
    }
    return flags;
}

static void scene_view_packet_fill_rgba(const SceneObject* object,
                                        int scene_object_index,
                                        int primitive_index,
                                        int face_group_index,
                                        SceneEditorSceneViewTriangle* out_triangle) {
    RuntimeMaterialSurfaceEval eval = {0};
    double visible_alpha = 1.0;
    if (!object || !out_triangle) return;
    if (!MaterialPreviewSurfaceEvaluateFacePrimitive(object,
                                                     scene_object_index,
                                                     primitive_index,
                                                     face_group_index,
                                                     0.333333333333,
                                                     0.333333333333,
                                                     &eval)) {
        eval.colorR = (double)SceneObjectColorR(object) / 255.0;
        eval.colorG = (double)SceneObjectColorG(object) / 255.0;
        eval.colorB = (double)SceneObjectColorB(object) / 255.0;
        eval.transparency = 1.0 - scene_view_packet_clamp01(object->opacity);
    }
    visible_alpha = 1.0 - scene_view_packet_clamp01(eval.transparency);
    if (object->material_id != MATERIAL_PRESET_TRANSPARENT) {
        visible_alpha = scene_view_packet_clamp01(object->opacity);
        if (visible_alpha <= 0.0) visible_alpha = 1.0;
    }
    out_triangle->rgba[0] = scene_view_packet_byte_from_unit(eval.colorR);
    out_triangle->rgba[1] = scene_view_packet_byte_from_unit(eval.colorG);
    out_triangle->rgba[2] = scene_view_packet_byte_from_unit(eval.colorB);
    out_triangle->rgba[3] = scene_view_packet_byte_from_unit(visible_alpha);
    out_triangle->displayFlags = scene_view_packet_display_flags(object, &eval);
}

void SceneEditorSceneViewPacketInit(SceneEditorSceneViewPacket* packet) {
    if (!packet) return;
    memset(packet, 0, sizeof(*packet));
    packet->focusedObjectIndex = -1;
    packet->previewQuality = SCENE_EDITOR_SCENE_VIEW_PREVIEW_OUTLINE;
    packet->degradedReason = SCENE_EDITOR_SCENE_VIEW_DEGRADED_NONE;
    packet->complete = true;
}

bool SceneEditorSceneViewPacketBuildFocusedObject(
    int focused_object_index,
    SceneEditorSceneViewPreviewQuality preview_quality,
    const SceneEditorDigestOverlayProjector* projector,
    SceneEditorSceneViewPacket* out_packet) {
    RuntimeScene3D scene;
    int max_face_group = -1;
    bool projected = projector != NULL;
    bool capped = false;
    if (!out_packet) return false;
    SceneEditorSceneViewPacketInit(out_packet);
    out_packet->focusedObjectIndex = focused_object_index;
    out_packet->previewQuality = preview_quality;

    if (focused_object_index < 0 || focused_object_index >= sceneSettings.objectCount) {
        out_packet->degradedReason = SCENE_EDITOR_SCENE_VIEW_DEGRADED_OBJECT_NOT_FOUND;
        out_packet->complete = false;
        return false;
    }
    if (!scene_editor_material_preview_build_scene(&scene)) {
        out_packet->degradedReason =
            SCENE_EDITOR_SCENE_VIEW_DEGRADED_NO_PRIMITIVE_SEED_STATE;
        out_packet->complete = false;
        return false;
    }

    for (int i = 0; i < scene.triangleMesh.triangleCount; ++i) {
        const RuntimeTriangle3D* triangle = &scene.triangleMesh.triangles[i];
        SceneEditorSceneViewTriangle* out_triangle = NULL;
        SceneEditorMaterialPreviewTriangleAddress address = {0};
        int sx = 0;
        int sy = 0;
        if (triangle->sceneObjectIndex != focused_object_index) continue;
        if (out_packet->triangleCount >= SCENE_EDITOR_SCENE_VIEW_PACKET_MAX_TRIANGLES) {
            capped = true;
            break;
        }
        out_triangle = &out_packet->triangles[out_packet->triangleCount];
        scene_editor_material_preview_fill_address(&address,
                                                   focused_object_index,
                                                   triangle->primitiveIndex,
                                                   i,
                                                   triangle->localTriangleIndex >= 0
                                                       ? triangle->localTriangleIndex
                                                       : out_packet->triangleCount);
        scene_view_packet_pick_id_from_address(&address, &out_triangle->pickId);
        out_triangle->p0[0] = triangle->p0.x;
        out_triangle->p0[1] = triangle->p0.y;
        out_triangle->p0[2] = triangle->p0.z;
        out_triangle->p1[0] = triangle->p1.x;
        out_triangle->p1[1] = triangle->p1.y;
        out_triangle->p1[2] = triangle->p1.z;
        out_triangle->p2[0] = triangle->p2.x;
        out_triangle->p2[1] = triangle->p2.y;
        out_triangle->p2[2] = triangle->p2.z;
        if (projector) {
            if (SceneEditorDigestOverlayProjectPoint(projector,
                                                     triangle->p0.x,
                                                     triangle->p0.y,
                                                     triangle->p0.z,
                                                     &sx,
                                                     &sy)) {
                out_triangle->screen0[0] = sx;
                out_triangle->screen0[1] = sy;
            } else {
                projected = false;
            }
            if (SceneEditorDigestOverlayProjectPoint(projector,
                                                     triangle->p1.x,
                                                     triangle->p1.y,
                                                     triangle->p1.z,
                                                     &sx,
                                                     &sy)) {
                out_triangle->screen1[0] = sx;
                out_triangle->screen1[1] = sy;
            } else {
                projected = false;
            }
            if (SceneEditorDigestOverlayProjectPoint(projector,
                                                     triangle->p2.x,
                                                     triangle->p2.y,
                                                     triangle->p2.z,
                                                     &sx,
                                                     &sy)) {
                out_triangle->screen2[0] = sx;
                out_triangle->screen2[1] = sy;
            } else {
                projected = false;
            }
            out_triangle->depth =
                (scene_editor_material_preview_view_depth(projector,
                                                          triangle->p0.x,
                                                          triangle->p0.y,
                                                          triangle->p0.z) +
                 scene_editor_material_preview_view_depth(projector,
                                                          triangle->p1.x,
                                                          triangle->p1.y,
                                                          triangle->p1.z) +
                 scene_editor_material_preview_view_depth(projector,
                                                          triangle->p2.x,
                                                          triangle->p2.y,
                                                          triangle->p2.z)) /
                3.0;
        }
        scene_view_packet_fill_rgba(&sceneSettings.sceneObjects[focused_object_index],
                                    focused_object_index,
                                    triangle->primitiveIndex,
                                    address.faceGroupIndex,
                                    out_triangle);
        if (address.faceGroupIndex > max_face_group) {
            max_face_group = address.faceGroupIndex;
        }
        out_packet->triangleCount += 1;
    }

    out_packet->faceGroupCount = max_face_group + 1;
    out_packet->projected = projected && out_packet->triangleCount > 0;
    out_packet->complete = !capped && out_packet->triangleCount > 0 && out_packet->projected;
    if (capped) {
        out_packet->degradedReason = SCENE_EDITOR_SCENE_VIEW_DEGRADED_TRIANGLE_CAP_REACHED;
    } else if (out_packet->triangleCount <= 0) {
        out_packet->degradedReason = SCENE_EDITOR_SCENE_VIEW_DEGRADED_OBJECT_NOT_FOUND;
    } else if (!out_packet->projected) {
        out_packet->degradedReason = SCENE_EDITOR_SCENE_VIEW_DEGRADED_PROJECTION_UNAVAILABLE;
    }
    RuntimeScene3D_Free(&scene);
    return out_packet->triangleCount > 0;
}

static json_object* scene_view_packet_json_vec3(const double values[3]) {
    json_object* array = json_object_new_array();
    if (!array) return NULL;
    json_object_array_add(array, json_object_new_double(values[0]));
    json_object_array_add(array, json_object_new_double(values[1]));
    json_object_array_add(array, json_object_new_double(values[2]));
    return array;
}

static json_object* scene_view_packet_json_screen(const int values[2]) {
    json_object* array = json_object_new_array();
    if (!array) return NULL;
    json_object_array_add(array, json_object_new_int(values[0]));
    json_object_array_add(array, json_object_new_int(values[1]));
    return array;
}

static json_object* scene_view_packet_json_rgba(const unsigned char values[4]) {
    json_object* array = json_object_new_array();
    if (!array) return NULL;
    json_object_array_add(array, json_object_new_int((int)values[0]));
    json_object_array_add(array, json_object_new_int((int)values[1]));
    json_object_array_add(array, json_object_new_int((int)values[2]));
    json_object_array_add(array, json_object_new_int((int)values[3]));
    return array;
}

static json_object* scene_view_packet_json_pick_id(
    const SceneEditorSceneViewPickId* pick_id) {
    json_object* obj = json_object_new_object();
    if (!obj || !pick_id) return obj;
    json_object_object_add(obj, "scene_object_index",
                           json_object_new_int(pick_id->sceneObjectIndex));
    json_object_object_add(obj, "primitive_index",
                           json_object_new_int(pick_id->primitiveIndex));
    json_object_object_add(obj, "triangle_index",
                           json_object_new_int(pick_id->triangleIndex));
    json_object_object_add(obj, "local_triangle_index",
                           json_object_new_int(pick_id->localTriangleIndex));
    json_object_object_add(obj, "face_group_index",
                           json_object_new_int(pick_id->faceGroupIndex));
    return obj;
}

bool SceneEditorSceneViewPacketToJsonString(const SceneEditorSceneViewPacket* packet,
                                            char* out_json,
                                            size_t out_json_capacity) {
    json_object* root = NULL;
    json_object* triangles = NULL;
    const char* json_text = NULL;
    size_t json_len = 0u;
    bool ok = false;
    if (!packet || !out_json || out_json_capacity == 0u) return false;
    out_json[0] = '\0';
    root = json_object_new_object();
    triangles = json_object_new_array();
    if (!root || !triangles) goto cleanup;

    json_object_object_add(root, "schema_family", json_object_new_string("codework_scene_view"));
    json_object_object_add(root,
                           "schema_variant",
                           json_object_new_string(SCENE_EDITOR_SCENE_VIEW_PACKET_SCHEMA_VARIANT));
    json_object_object_add(root, "focused_object_index",
                           json_object_new_int(packet->focusedObjectIndex));
    json_object_object_add(root, "preview_quality",
                           json_object_new_string(SceneEditorSceneViewPreviewQualityLabel(
                               packet->previewQuality)));
    json_object_object_add(root, "preview_quality_id",
                           json_object_new_int((int)packet->previewQuality));
    json_object_object_add(root, "degraded_reason",
                           json_object_new_string(SceneEditorSceneViewDegradedReasonLabel(
                               packet->degradedReason)));
    json_object_object_add(root, "degraded_reason_id",
                           json_object_new_int((int)packet->degradedReason));
    json_object_object_add(root, "projected", json_object_new_boolean(packet->projected));
    json_object_object_add(root, "complete", json_object_new_boolean(packet->complete));
    json_object_object_add(root, "triangle_count", json_object_new_int(packet->triangleCount));
    json_object_object_add(root, "face_group_count", json_object_new_int(packet->faceGroupCount));

    for (int i = 0; i < packet->triangleCount &&
                    i < SCENE_EDITOR_SCENE_VIEW_PACKET_MAX_TRIANGLES;
         ++i) {
        const SceneEditorSceneViewTriangle* triangle = &packet->triangles[i];
        json_object* tri = json_object_new_object();
        if (!tri) goto cleanup;
        json_object_object_add(tri, "pick_id",
                               scene_view_packet_json_pick_id(&triangle->pickId));
        json_object_object_add(tri, "p0", scene_view_packet_json_vec3(triangle->p0));
        json_object_object_add(tri, "p1", scene_view_packet_json_vec3(triangle->p1));
        json_object_object_add(tri, "p2", scene_view_packet_json_vec3(triangle->p2));
        json_object_object_add(tri, "screen0",
                               scene_view_packet_json_screen(triangle->screen0));
        json_object_object_add(tri, "screen1",
                               scene_view_packet_json_screen(triangle->screen1));
        json_object_object_add(tri, "screen2",
                               scene_view_packet_json_screen(triangle->screen2));
        json_object_object_add(tri, "depth", json_object_new_double(triangle->depth));
        json_object_object_add(tri, "rgba", scene_view_packet_json_rgba(triangle->rgba));
        json_object_object_add(tri, "display_flags",
                               json_object_new_int((int)triangle->displayFlags));
        json_object_array_add(triangles, tri);
    }
    json_object_object_add(root, "triangles", triangles);
    triangles = NULL;
    json_text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if (!json_text) goto cleanup;
    json_len = strlen(json_text);
    if (json_len + 1u > out_json_capacity) goto cleanup;
    memcpy(out_json, json_text, json_len + 1u);
    ok = true;

cleanup:
    if (triangles) json_object_put(triangles);
    if (root) json_object_put(root);
    return ok;
}

bool SceneEditorSceneViewPacketReadbackFromJsonString(
    const char* json,
    SceneEditorSceneViewPacketReadback* out_readback) {
    return core_scene_view_packet_readback_from_json_string(json, out_readback).code == CORE_OK;
}

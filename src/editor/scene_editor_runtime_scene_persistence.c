#include "editor/scene_editor_runtime_scene_persistence.h"

#include "camera/camera_path_3d.h"
#include "config/config_manager.h"
#include "config/config_scene_path_io.h"
#include "core_scene.h"
#include "core_io.h"
#include "editor/scene_editor_material_graph.h"
#include "editor/scene_editor_material_face_placement.h"
#include "editor/scene_editor_material_stack.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_material_texture_stack_3d.h"
#include "render/runtime_material_graph_3d.h"
#include "render/runtime_material_authored_texture_3d.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void scene_editor_runtime_scene_authored_manifest_write_path(
    const char* runtime_scene_path,
    const char* manifest_path,
    char* out_path,
    size_t out_path_size) {
    char scene_dir[4096];
    size_t scene_dir_len = 0u;
    if (!out_path || out_path_size == 0u) return;
    out_path[0] = '\0';
    if (!manifest_path || !manifest_path[0]) return;
    if (!runtime_scene_path || !runtime_scene_path[0]) {
        snprintf(out_path, out_path_size, "%s", manifest_path);
        return;
    }
    if (manifest_path[0] != '/' ||
        core_scene_dirname(runtime_scene_path, scene_dir, sizeof(scene_dir)).code != CORE_OK) {
        snprintf(out_path, out_path_size, "%s", manifest_path);
        return;
    }
    scene_dir_len = strlen(scene_dir);
    if (scene_dir_len > 0u &&
        strncmp(manifest_path, scene_dir, scene_dir_len) == 0 &&
        manifest_path[scene_dir_len] == '/') {
        snprintf(out_path, out_path_size, "%s", manifest_path + scene_dir_len + 1u);
        return;
    }
    snprintf(out_path, out_path_size, "%s", manifest_path);
}

static bool scene_editor_runtime_scene_path_is_absolute(const char* path) {
    return path && path[0] == '/';
}

static bool scene_editor_runtime_scene_authored_manifest_is_under_scene_dir(
    const char* runtime_scene_path,
    const char* manifest_path,
    char* out_relative_path,
    size_t out_relative_path_size) {
    char scene_dir[4096];
    size_t scene_dir_len = 0u;
    if (out_relative_path && out_relative_path_size > 0u) {
        out_relative_path[0] = '\0';
    }
    if (!runtime_scene_path || !runtime_scene_path[0] || !manifest_path || !manifest_path[0] ||
        !scene_editor_runtime_scene_path_is_absolute(manifest_path) ||
        !out_relative_path || out_relative_path_size == 0u) {
        return false;
    }
    if (core_scene_dirname(runtime_scene_path, scene_dir, sizeof(scene_dir)).code != CORE_OK) {
        return false;
    }
    scene_dir_len = strlen(scene_dir);
    if (scene_dir_len == 0u ||
        strncmp(manifest_path, scene_dir, scene_dir_len) != 0 ||
        manifest_path[scene_dir_len] != '/') {
        return false;
    }
    snprintf(out_relative_path, out_relative_path_size, "%s", manifest_path + scene_dir_len + 1u);
    return out_relative_path[0] != '\0';
}

static void scene_editor_runtime_scene_authored_texture_add_path_fields(
    json_object* authored_texture,
    const char* runtime_scene_path,
    const char* manifest_path) {
    char manifest_write_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
    if (!authored_texture || !manifest_path || !manifest_path[0]) return;
    if (scene_editor_runtime_scene_authored_manifest_is_under_scene_dir(
            runtime_scene_path,
            manifest_path,
            manifest_write_path,
            sizeof(manifest_write_path))) {
        json_object_object_add(authored_texture,
                               "manifest_path",
                               json_object_new_string(manifest_write_path));
        json_object_object_add(authored_texture,
                               "path_scope",
                               json_object_new_string("scene_relative"));
        return;
    }
    if (scene_editor_runtime_scene_path_is_absolute(manifest_path)) {
        json_object_object_add(authored_texture,
                               "path_scope",
                               json_object_new_string("local_absolute"));
        json_object_object_add(authored_texture,
                               "local_manifest_path",
                               json_object_new_string(manifest_path));
        return;
    }
    scene_editor_runtime_scene_authored_manifest_write_path(runtime_scene_path,
                                                            manifest_path,
                                                            manifest_write_path,
                                                            sizeof(manifest_write_path));
    json_object_object_add(authored_texture,
                           "manifest_path",
                           json_object_new_string(manifest_write_path[0]
                                                      ? manifest_write_path
                                                      : manifest_path));
    json_object_object_add(authored_texture,
                           "path_scope",
                           json_object_new_string("scene_relative"));
}

static void scene_editor_runtime_scene_diag(char* out_diagnostics,
                                            size_t out_diagnostics_size,
                                            const char* message) {
    if (!out_diagnostics || out_diagnostics_size == 0 || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

static bool scene_editor_runtime_scene_read_file(const char* path,
                                                 char** out_text,
                                                 size_t* out_size,
                                                 char* out_diagnostics,
                                                 size_t out_diagnostics_size) {
    CoreBuffer file_data = {0};
    char* text = NULL;
    CoreResult io_result;
    if (out_text) *out_text = NULL;
    if (out_size) *out_size = 0;
    if (!path || !path[0] || !out_text) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "runtime scene path missing");
        return false;
    }
    io_result = core_io_read_all(path, &file_data);
    if (io_result.code != CORE_OK || !file_data.data || file_data.size == 0) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "failed to read runtime scene");
        core_io_buffer_free(&file_data);
        return false;
    }
    text = (char*)malloc(file_data.size + 1u);
    if (!text) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "out of memory");
        core_io_buffer_free(&file_data);
        return false;
    }
    memcpy(text, file_data.data, file_data.size);
    text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);
    *out_text = text;
    if (out_size) *out_size = file_data.size;
    scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

static void scene_editor_runtime_scene_scale_path(Path* dst,
                                                  const Path* src,
                                                  double factor) {
    int i = 0;
    if (!dst || !src) return;
    *dst = *src;
    for (i = 0; i < dst->numPoints && i < MAX_BEZIER_POINTS; ++i) {
        dst->points[i].x *= factor;
        dst->points[i].y *= factor;
        if (i < MAX_BEZIER_POINTS - 1) {
            dst->handles[i][0].vx *= factor;
            dst->handles[i][0].vy *= factor;
            dst->handles[i][1].vx *= factor;
            dst->handles[i][1].vy *= factor;
        }
    }
}

static void scene_editor_runtime_scene_scale_camera_path3d(CameraPath3D* dst,
                                                           const CameraPath3D* src,
                                                           const Path* path,
                                                           double factor) {
    if (!dst || !src || !path) return;
    *dst = *src;
    CameraPath3D_ScaleWorldUnits(dst, path, factor);
}

static bool scene_editor_runtime_scene_resolve_metrics(const char* runtime_scene_json,
                                                       double* out_world_scale,
                                                       long long* out_next_clock,
                                                       char* out_diagnostics,
                                                       size_t out_diagnostics_size) {
    json_object* root = NULL;
    json_object* world_scale_obj = NULL;
    json_object* extensions = NULL;
    json_object* overlay_merge = NULL;
    json_object* producer_clocks = NULL;
    json_object* ray_clock = NULL;
    double world_scale = 1.0;
    long long next_clock = 1;

    if (!runtime_scene_json || !out_world_scale || !out_next_clock) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "invalid input");
        return false;
    }

    root = json_tokener_parse(runtime_scene_json);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "invalid runtime scene json");
        return false;
    }

    if (json_object_object_get_ex(root, "world_scale", &world_scale_obj) &&
        (json_object_is_type(world_scale_obj, json_type_double) ||
         json_object_is_type(world_scale_obj, json_type_int))) {
        world_scale = json_object_get_double(world_scale_obj);
    }
    if (!(world_scale > 0.0)) {
        world_scale = 1.0;
    }

    if (json_object_object_get_ex(root, "extensions", &extensions) &&
        json_object_is_type(extensions, json_type_object) &&
        json_object_object_get_ex(extensions, "overlay_merge", &overlay_merge) &&
        json_object_is_type(overlay_merge, json_type_object) &&
        json_object_object_get_ex(overlay_merge, "producer_clocks", &producer_clocks) &&
        json_object_is_type(producer_clocks, json_type_object) &&
        json_object_object_get_ex(producer_clocks, "ray_tracing", &ray_clock) &&
        json_object_is_type(ray_clock, json_type_int)) {
        next_clock = json_object_get_int64(ray_clock) + 1;
    }

    json_object_put(root);
    *out_world_scale = world_scale;
    *out_next_clock = next_clock;
    scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

static json_object* scene_editor_runtime_scene_texture_params_json(
    RuntimeMaterialTexture3DParams params) {
    json_object* parameters = json_object_new_object();
    params = RuntimeMaterialTexture3DNormalizeParams(params);
    if (!parameters) return NULL;
    json_object_object_add(parameters, "pattern_mode", json_object_new_int(params.patternMode));
    json_object_object_add(parameters, "coverage", json_object_new_double(params.coverage));
    json_object_object_add(parameters, "grain", json_object_new_double(params.grain));
    json_object_object_add(parameters, "edge_softness", json_object_new_double(params.edgeSoftness));
    json_object_object_add(parameters, "contrast", json_object_new_double(params.contrast));
    json_object_object_add(parameters, "flow", json_object_new_double(params.flow));
    json_object_object_add(parameters, "color_depth", json_object_new_double(params.colorDepth));
    json_object_object_add(parameters, "surface_damage", json_object_new_double(params.surfaceDamage));
    json_object_object_add(parameters, "seed", json_object_new_int(params.seed));
    return parameters;
}

static json_object* scene_editor_runtime_scene_texture_placement_json(
    const RuntimeMaterialTexture3DPlacement* placement) {
    json_object* placement_obj = json_object_new_object();
    if (!placement_obj || !placement) return placement_obj;
    json_object_object_add(placement_obj, "offset_u", json_object_new_double(placement->offsetU));
    json_object_object_add(placement_obj, "offset_v", json_object_new_double(placement->offsetV));
    json_object_object_add(placement_obj, "scale", json_object_new_double(placement->scale));
    json_object_object_add(placement_obj, "strength", json_object_new_double(placement->strength));
    json_object_object_add(placement_obj, "rotation", json_object_new_double(placement->rotation));
    return placement_obj;
}

static json_object* scene_editor_runtime_scene_material_stack_json(int scene_object_index) {
    RuntimeMaterialTextureStack stack = RuntimeMaterialTextureStackEmpty();
    json_object* stack_obj = NULL;
    json_object* layers = NULL;
    if (scene_object_index < 0 || scene_object_index >= sceneSettings.objectCount) return NULL;
    if (!SceneEditorMaterialStackGetEffectiveObjectStack(&sceneSettings.sceneObjects[scene_object_index],
                                                         scene_object_index,
                                                         &stack)) {
        return NULL;
    }
    stack = RuntimeMaterialTextureStackNormalize(stack);
    if (stack.layerCount <= 1 &&
        stack.layers[0].kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_SOLID &&
        !SceneEditorMaterialStackHasObjectStack(scene_object_index)) {
        return NULL;
    }
    stack_obj = json_object_new_object();
    layers = json_object_new_array();
    if (!stack_obj || !layers) {
        if (stack_obj) json_object_put(stack_obj);
        if (layers) json_object_put(layers);
        return NULL;
    }
    json_object_object_add(stack_obj, "version", json_object_new_int(1));
    for (int i = 0; i < stack.layerCount && i < RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS; ++i) {
        RuntimeMaterialTextureLayer layer = RuntimeMaterialTextureLayerNormalize(stack.layers[i]);
        json_object* layer_obj = NULL;
        json_object* parameters = NULL;
        if (!layer.enabled || layer.kind == RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_NONE) continue;
        layer_obj = json_object_new_object();
        parameters = scene_editor_runtime_scene_texture_params_json(layer.params);
        if (!layer_obj || !parameters) {
            if (layer_obj) json_object_put(layer_obj);
            if (parameters) json_object_put(parameters);
            json_object_put(layers);
            json_object_put(stack_obj);
            return NULL;
        }
        json_object_object_add(layer_obj, "id", json_object_new_string(layer.layerId));
        json_object_object_add(layer_obj, "name", json_object_new_string(layer.displayName));
        json_object_object_add(layer_obj,
                               "role",
                               json_object_new_string(layer.role == RUNTIME_MATERIAL_TEXTURE_LAYER_ROLE_BASE
                                                          ? "base"
                                                          : "overlay"));
        json_object_object_add(layer_obj,
                               "kind",
                               json_object_new_string(RuntimeMaterialTextureLayerKindStableId(layer.kind)));
        json_object_object_add(layer_obj,
                               "blend",
                               json_object_new_string(
                                   RuntimeMaterialTextureLayerBlendModeStableId(layer.blendMode)));
        json_object_object_add(layer_obj, "enabled", json_object_new_boolean(layer.enabled));
        json_object_object_add(layer_obj, "opacity", json_object_new_double(layer.opacity));
        json_object_object_add(layer_obj,
                               "placement",
                               scene_editor_runtime_scene_texture_placement_json(&layer.placement));
        json_object_object_add(layer_obj, "parameters", parameters);
        json_object_object_add(layer_obj,
                               "roughness_influence",
                               json_object_new_double(layer.roughnessInfluence));
        json_object_object_add(layer_obj,
                               "reflectivity_influence",
                               json_object_new_double(layer.reflectivityInfluence));
        json_object_object_add(layer_obj,
                               "specular_influence",
                               json_object_new_double(layer.specularInfluence));
        json_object_object_add(layer_obj,
                               "diffuse_influence",
                               json_object_new_double(layer.diffuseInfluence));
        json_object_object_add(layer_obj,
                               "transparency_influence",
                               json_object_new_double(layer.transparencyInfluence));
        json_object_array_add(layers, layer_obj);
    }
    json_object_object_add(stack_obj, "layers", layers);
    return stack_obj;
}

static json_object* scene_editor_runtime_scene_material_graph_json(int scene_object_index) {
    RuntimeMaterialGraphDocument document;
    if (scene_object_index < 0 || scene_object_index >= sceneSettings.objectCount) return NULL;
    if (!SceneEditorMaterialGraphGetObjectGraph(scene_object_index, &document)) {
        return NULL;
    }
    return RuntimeMaterialGraphDocumentToJsonObject(&document, true);
}

static json_object* scene_editor_runtime_scene_build_object_materials_json(void) {
    json_object* object_materials = json_object_new_array();
    int i = 0;
    if (!object_materials) return NULL;
    for (i = 0; i < sceneSettings.objectCount; ++i) {
        char object_id[64];
        json_object* entry = NULL;
        if (!runtime_scene_bridge_get_last_object_id_for_scene_index(i, object_id, sizeof(object_id))) {
            continue;
        }
        entry = json_object_new_object();
        if (!entry) {
            json_object_put(object_materials);
            return NULL;
        }
        json_object_object_add(entry, "object_id", json_object_new_string(object_id));
        json_object_object_add(entry, "material_id", json_object_new_int(sceneSettings.sceneObjects[i].material_id));
        if (!SceneObjectIsGuideOnly(&sceneSettings.sceneObjects[i])) {
            json_object_object_add(entry,
                                   "object_color",
                                   json_object_new_int(sceneSettings.sceneObjects[i].color & 0xFFFFFF));
        }
        json_object_object_add(entry,
                               "alpha",
                               json_object_new_double(sceneSettings.sceneObjects[i].alpha));
        json_object_object_add(entry,
                               "emissive_strength",
                               json_object_new_double(sceneSettings.sceneObjects[i].emissiveStrength));
        {
            char manifest_path[RUNTIME_MATERIAL_AUTHORED_TEXTURE_PATH_CAPACITY];
            char binding_mode[RUNTIME_MATERIAL_AUTHORED_TEXTURE_MODE_CAPACITY];
            char invalid_reason[RUNTIME_MATERIAL_AUTHORED_TEXTURE_REASON_CAPACITY];
            int face_count = 0;
            if (RuntimeMaterialAuthoredTextureGetBinding(i,
                                                         manifest_path,
                                                         sizeof(manifest_path),
                                                         binding_mode,
                                                         sizeof(binding_mode),
                                                         &face_count)) {
                json_object* authored_texture = json_object_new_object();
                if (!authored_texture) {
                    json_object_put(object_materials);
                    json_object_put(entry);
                    return NULL;
                }
                scene_editor_runtime_scene_authored_texture_add_path_fields(authored_texture,
                                                                            animSettings.runtimeScenePath,
                                                                            manifest_path);
                json_object_object_add(authored_texture,
                                       "binding_mode",
                                       json_object_new_string(binding_mode[0] ? binding_mode
                                                                              : "override"));
                json_object_object_add(authored_texture, "face_count", json_object_new_int(face_count));
                json_object_object_add(entry, "authored_texture", authored_texture);
            } else if (RuntimeMaterialAuthoredTextureGetInvalidBinding(i,
                                                                       manifest_path,
                                                                       sizeof(manifest_path),
                                                                       binding_mode,
                                                                       sizeof(binding_mode),
                                                                       invalid_reason,
                                                                       sizeof(invalid_reason))) {
                json_object* authored_texture = json_object_new_object();
                if (!authored_texture) {
                    json_object_put(object_materials);
                    json_object_put(entry);
                    return NULL;
                }
                scene_editor_runtime_scene_authored_texture_add_path_fields(authored_texture,
                                                                            animSettings.runtimeScenePath,
                                                                            manifest_path);
                json_object_object_add(authored_texture,
                                       "binding_mode",
                                       json_object_new_string(binding_mode[0] ? binding_mode
                                                                              : "override"));
                if (invalid_reason[0]) {
                    json_object_object_add(authored_texture,
                                           "invalid_reason",
                                           json_object_new_string(invalid_reason));
                }
                json_object_object_add(entry, "authored_texture", authored_texture);
            } else {
                json_object_object_add(entry, "authored_texture", json_object_new_null());
            }
        }
        {
            int face_count = SceneEditorMaterialFacePlacementOverrideCountForObject(i);
            bool has_material_stack = SceneEditorMaterialStackHasObjectStack(i);
            bool has_procedural_texture =
                has_material_stack ||
                sceneSettings.sceneObjects[i].textureId != 0 ||
                sceneSettings.sceneObjects[i].textureOffsetU != 0.0 ||
                sceneSettings.sceneObjects[i].textureOffsetV != 0.0 ||
                sceneSettings.sceneObjects[i].textureScale != 1.0 ||
                sceneSettings.sceneObjects[i].textureStrength != 0.0 ||
                sceneSettings.sceneObjects[i].texturePatternMode != 0 ||
                sceneSettings.sceneObjects[i].textureCoverage != 0.5 ||
                sceneSettings.sceneObjects[i].textureGrain != 0.5 ||
                sceneSettings.sceneObjects[i].textureEdgeSoftness != 0.5 ||
                sceneSettings.sceneObjects[i].textureContrast != 0.5 ||
                sceneSettings.sceneObjects[i].textureFlow != 0.0 ||
                sceneSettings.sceneObjects[i].textureColorDepth != 0.5 ||
                sceneSettings.sceneObjects[i].textureSurfaceDamage != 0.5 ||
                sceneSettings.sceneObjects[i].textureSeed != 0 ||
                face_count > 0;
            json_object* procedural_texture = NULL;
            json_object* face_placements = NULL;
            json_object* material_graph = NULL;
            json_object* material_texture_stack = NULL;
            bool has_material_graph = SceneEditorMaterialGraphHasObjectGraph(i);
            if (!has_procedural_texture && !has_material_graph) {
                json_object_array_add(object_materials, entry);
                continue;
            }
            procedural_texture = json_object_new_object();
            face_placements = json_object_new_array();
            if (!procedural_texture || !face_placements) {
                if (procedural_texture) json_object_put(procedural_texture);
                if (face_placements) json_object_put(face_placements);
                json_object_put(object_materials);
                json_object_put(entry);
                return NULL;
            }
            material_texture_stack = scene_editor_runtime_scene_material_stack_json(i);
            material_graph = scene_editor_runtime_scene_material_graph_json(i);
            if (material_graph) {
                json_object_object_add(entry, "material_graph", material_graph);
            }
            if (material_texture_stack) {
                json_object_object_add(entry, "material_texture_stack", material_texture_stack);
            }
            json_object_object_add(procedural_texture,
                                   "texture_id",
                                   json_object_new_int(sceneSettings.sceneObjects[i].textureId));
            json_object_object_add(procedural_texture,
                                   "offset_u",
                                   json_object_new_double(sceneSettings.sceneObjects[i].textureOffsetU));
            json_object_object_add(procedural_texture,
                                   "offset_v",
                                   json_object_new_double(sceneSettings.sceneObjects[i].textureOffsetV));
            json_object_object_add(procedural_texture,
                                   "scale",
                                   json_object_new_double(sceneSettings.sceneObjects[i].textureScale));
            json_object_object_add(procedural_texture,
                                   "strength",
                                   json_object_new_double(sceneSettings.sceneObjects[i].textureStrength));
            {
                json_object* parameters =
                    scene_editor_runtime_scene_texture_params_json(
                        RuntimeMaterialTexture3DParamsFromObject(&sceneSettings.sceneObjects[i]));
                if (!parameters) {
                    json_object_put(face_placements);
                    json_object_put(procedural_texture);
                    json_object_put(object_materials);
                    json_object_put(entry);
                    return NULL;
                }
                json_object_object_add(procedural_texture, "parameters", parameters);
            }
            for (int face_i = 0; face_i < face_count; ++face_i) {
                SceneEditorMaterialFacePlacement placement;
                json_object* face_entry = NULL;
                if (!SceneEditorMaterialFacePlacementGetOverrideForObject(i, face_i, &placement)) {
                    continue;
                }
                face_entry = json_object_new_object();
                if (!face_entry) {
                    json_object_put(face_placements);
                    json_object_put(procedural_texture);
                    json_object_put(object_materials);
                    json_object_put(entry);
                    return NULL;
                }
                json_object_object_add(face_entry,
                                       "face_group_index",
                                       json_object_new_int(placement.faceGroupIndex));
                if (placement.layerIndex >= 0) {
                    json_object_object_add(face_entry,
                                           "layer_index",
                                           json_object_new_int(placement.layerIndex));
                }
                if (placement.layerId[0]) {
                    json_object_object_add(face_entry,
                                           "layer_id",
                                           json_object_new_string(placement.layerId));
                }
                json_object_object_add(face_entry, "texture_id", json_object_new_int(placement.textureId));
                json_object_object_add(face_entry, "offset_u", json_object_new_double(placement.offsetU));
                json_object_object_add(face_entry, "offset_v", json_object_new_double(placement.offsetV));
                json_object_object_add(face_entry, "scale", json_object_new_double(placement.scale));
                json_object_object_add(face_entry, "strength", json_object_new_double(placement.strength));
                json_object_object_add(face_entry, "rotation", json_object_new_double(placement.rotation));
                {
                    json_object* parameters =
                        scene_editor_runtime_scene_texture_params_json(placement.params);
                    if (!parameters) {
                        json_object_put(face_entry);
                        json_object_put(face_placements);
                        json_object_put(procedural_texture);
                        json_object_put(object_materials);
                        json_object_put(entry);
                        return NULL;
                    }
                    json_object_object_add(face_entry, "parameters", parameters);
                }
                json_object_array_add(face_placements, face_entry);
            }
            json_object_object_add(procedural_texture, "face_placements", face_placements);
            json_object_object_add(entry, "procedural_texture", procedural_texture);
        }
        json_object_array_add(object_materials, entry);
    }
    return object_materials;
}

static char* scene_editor_runtime_scene_build_overlay_json(double world_scale,
                                                           long long logical_clock,
                                                           char* out_diagnostics,
                                                           size_t out_diagnostics_size) {
    json_object* overlay_root = NULL;
    json_object* overlay_meta = NULL;
    json_object* extensions = NULL;
    json_object* ray_tracing = NULL;
    json_object* authoring = NULL;
    json_object* environment = NULL;
    json_object* light_path = NULL;
    json_object* light_path_depth = NULL;
    json_object* light_settings = NULL;
    json_object* camera_path = NULL;
    json_object* camera_path_depth = NULL;
    json_object* object_materials = NULL;
    const char* serialized = NULL;
    char* out = NULL;
    Path saved_light_path = {0};
    CameraPath3D saved_light_path3d = {0};
    Path saved_camera_path = {0};
    CameraPath3D saved_camera_path3d = {0};
    double authored_to_runtime = 1.0;
    size_t out_len = 0;

    if (!(world_scale > 0.0)) {
        world_scale = 1.0;
    }
    authored_to_runtime = 1.0 / world_scale;
    scene_editor_runtime_scene_scale_path(&saved_light_path, &sceneSettings.bezierPath, authored_to_runtime);
    scene_editor_runtime_scene_scale_camera_path3d(&saved_light_path3d,
                                                   &sceneSettings.bezierPath3D,
                                                   &saved_light_path,
                                                   authored_to_runtime);
    scene_editor_runtime_scene_scale_path(&saved_camera_path, &sceneSettings.cameraPath, authored_to_runtime);
    scene_editor_runtime_scene_scale_camera_path3d(&saved_camera_path3d,
                                                   &sceneSettings.cameraPath3D,
                                                   &saved_camera_path,
                                                   authored_to_runtime);

    overlay_root = json_object_new_object();
    overlay_meta = json_object_new_object();
    extensions = json_object_new_object();
    ray_tracing = json_object_new_object();
    authoring = json_object_new_object();
    environment = json_object_new_object();
    light_path = config_scene_path_to_json_object(&saved_light_path);
    light_path_depth = CameraPath3D_ToJsonObject(&saved_light_path3d, &saved_light_path);
    light_settings = json_object_new_object();
    camera_path = config_scene_path_to_json_object(&saved_camera_path);
    camera_path_depth = CameraPath3D_ToJsonObject(&saved_camera_path3d, &saved_camera_path);
    object_materials = scene_editor_runtime_scene_build_object_materials_json();

    if (!overlay_root || !overlay_meta || !extensions || !ray_tracing || !authoring ||
        !environment ||
        !light_path || !light_path_depth || !light_settings ||
        !camera_path || !camera_path_depth || !object_materials) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "failed to build overlay json");
        if (environment) json_object_put(environment);
        if (object_materials) json_object_put(object_materials);
        if (light_path_depth) json_object_put(light_path_depth);
        if (light_settings) json_object_put(light_settings);
        if (camera_path_depth) json_object_put(camera_path_depth);
        if (camera_path) json_object_put(camera_path);
        if (light_path) json_object_put(light_path);
        if (authoring) json_object_put(authoring);
        if (ray_tracing) json_object_put(ray_tracing);
        if (extensions) json_object_put(extensions);
        if (overlay_meta) json_object_put(overlay_meta);
        if (overlay_root) json_object_put(overlay_root);
        return NULL;
    }

    json_object_object_add(overlay_meta, "producer", json_object_new_string("ray_tracing"));
    json_object_object_add(overlay_meta, "logical_clock", json_object_new_int64(logical_clock));
    json_object_object_add(overlay_root, "overlay_meta", overlay_meta);

    json_object_object_add(light_settings, "intensity", json_object_new_double(animSettings.lightIntensity));
    json_object_object_add(light_settings,
                           "radius",
                           json_object_new_double(animSettings.lightRadius));
    json_object_object_add(environment,
                           "light_mode",
                           json_object_new_int(animation_config_environment_light_mode_clamp(
                               animSettings.environmentLightMode)));
    json_object_object_add(environment,
                           "ambient_brightness",
                           json_object_new_double(animSettings.environmentBrightness));
    json_object_object_add(environment,
                           "ambient_strength",
                           json_object_new_double(fmax(
                               0.0,
                               fmin(1.0, animSettings.environmentBrightness / 255.0))));
    json_object_object_add(environment,
                           "environment_preset",
                           json_object_new_int(animation_config_environment_preset_clamp(
                               animSettings.environmentPreset)));
    json_object_object_add(environment,
                           "background_brightness_auto",
                           json_object_new_boolean(animSettings.environmentBackgroundBrightnessAuto));
    json_object_object_add(environment,
                           "background_brightness",
                           json_object_new_double(animSettings.environmentBackgroundBrightnessAuto
                                                      ? fmax(0.0,
                                                             fmin(1.0,
                                                                  animSettings.environmentBrightness /
                                                                      255.0))
                                                      : animSettings.environmentBackgroundBrightness));
    {
        json_object *background_color = json_object_new_object();
        if (background_color) {
            json_object_object_add(background_color,
                                   "r",
                                   json_object_new_double(animSettings.environmentBackgroundColorR));
            json_object_object_add(background_color,
                                   "g",
                                   json_object_new_double(animSettings.environmentBackgroundColorG));
            json_object_object_add(background_color,
                                   "b",
                                   json_object_new_double(animSettings.environmentBackgroundColorB));
            json_object_object_add(environment, "background_color", background_color);
        }
    }
    json_object_object_add(environment,
                           "top_fill_strength",
                           json_object_new_double(animSettings.topFillStrength));
    json_object_object_add(authoring, "light_path", light_path);
    json_object_object_add(authoring, "light_path_depth", light_path_depth);
    json_object_object_add(authoring, "light_settings", light_settings);
    json_object_object_add(authoring, "environment", environment);
    json_object_object_add(authoring, "camera_path", camera_path);
    json_object_object_add(authoring, "camera_path_depth", camera_path_depth);
    json_object_object_add(authoring, "object_materials", object_materials);
    json_object_object_add(ray_tracing, "authoring", authoring);
    json_object_object_add(extensions, "ray_tracing", ray_tracing);
    json_object_object_add(overlay_root, "extensions", extensions);

    serialized = json_object_to_json_string_ext(overlay_root,
                                                JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
    if (!serialized) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "failed to serialize overlay json");
        json_object_put(overlay_root);
        return NULL;
    }

    out_len = strlen(serialized);
    out = (char*)malloc(out_len + 1u);
    if (!out) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "out of memory");
        json_object_put(overlay_root);
        return NULL;
    }
    memcpy(out, serialized, out_len + 1u);
    json_object_put(overlay_root);
    scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "ok");
    return out;
}

bool SceneEditorRuntimeScenePersistAuthoring(char* out_diagnostics, size_t out_diagnostics_size) {
    RuntimeSceneBridgePreflight summary = {0};
    char* runtime_scene_json = NULL;
    char* overlay_json = NULL;
    char* merged_json = NULL;
    char persisted_runtime_scene_path[sizeof(animSettings.runtimeScenePath)];
    double world_scale = 1.0;
    long long logical_clock = 1;
    CoreResult write_result;
    bool ok = false;

    scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (animSettings.sceneSource != SCENE_SOURCE_RUNTIME_SCENE ||
        animSettings.runtimeScenePath[0] == '\0') {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "runtime scene source is not active");
        return false;
    }
    snprintf(persisted_runtime_scene_path,
             sizeof(persisted_runtime_scene_path),
             "%s",
             animSettings.runtimeScenePath);

    if (!scene_editor_runtime_scene_read_file(animSettings.runtimeScenePath,
                                              &runtime_scene_json,
                                              NULL,
                                              out_diagnostics,
                                              out_diagnostics_size)) {
        return false;
    }

    if (!scene_editor_runtime_scene_resolve_metrics(runtime_scene_json,
                                                    &world_scale,
                                                    &logical_clock,
                                                    out_diagnostics,
                                                    out_diagnostics_size)) {
        free(runtime_scene_json);
        return false;
    }

    overlay_json = scene_editor_runtime_scene_build_overlay_json(world_scale,
                                                                 logical_clock,
                                                                 out_diagnostics,
                                                                 out_diagnostics_size);
    if (!overlay_json) {
        free(runtime_scene_json);
        return false;
    }

    ok = runtime_scene_bridge_writeback_ray_overlay_json(runtime_scene_json,
                                                         overlay_json,
                                                         &merged_json,
                                                         out_diagnostics,
                                                         out_diagnostics_size);
    free(runtime_scene_json);
    free(overlay_json);
    if (!ok || !merged_json) {
        free(merged_json);
        return false;
    }

    write_result = core_io_write_all(animSettings.runtimeScenePath, merged_json, strlen(merged_json));
    free(merged_json);
    if (write_result.code != CORE_OK) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "failed to write merged runtime scene");
        return false;
    }

    if (!runtime_scene_bridge_apply_file_defer_mesh_assets(animSettings.runtimeScenePath, &summary)) {
        scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, summary.diagnostics);
        return false;
    }
    animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
    snprintf(animSettings.runtimeScenePath,
             sizeof(animSettings.runtimeScenePath),
             "%s",
             persisted_runtime_scene_path);

    scene_editor_runtime_scene_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

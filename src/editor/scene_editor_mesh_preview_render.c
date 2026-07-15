#include "editor/scene_editor_mesh_preview_render.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "editor/editor_mode_router.h"
#include "editor/scene_editor_digest_overlay_internal.h"
#include "editor/scene_editor_mesh_preview_store.h"
#include "import/runtime_mesh_asset_loader.h"
#include "render/render_helper.h"

#if USE_VULKAN
#include "vk_renderer.h"
#endif

#define SCENE_EDITOR_MESH_PREVIEW_GPU_FRAME_SLOTS 4
#define SCENE_EDITOR_MESH_PREVIEW_BUTTON_W 78
#define SCENE_EDITOR_MESH_PREVIEW_BUTTON_H 28
#define SCENE_EDITOR_MESH_PREVIEW_BUTTON_GAP 6

typedef struct SceneEditorMeshPreviewPoint3 {
    double x;
    double y;
    double z;
} SceneEditorMeshPreviewPoint3;

typedef struct SceneEditorMeshPreviewGpuSlot {
    bool valid;
    uint64_t signature;
#if USE_VULKAN
    VkRendererTriMesh solid_mesh;
    VkRendererTriMesh material_mesh;
    VkRendererLineMesh wire_mesh;
#endif
    size_t triangle_count;
    size_t wire_segment_count;
} SceneEditorMeshPreviewGpuSlot;

typedef struct SceneEditorMeshPreviewInstanceCache {
    SceneEditorMeshPreviewGpuSlot slots[SCENE_EDITOR_MESH_PREVIEW_GPU_FRAME_SLOTS];
} SceneEditorMeshPreviewInstanceCache;

typedef struct SceneEditorMeshPreviewDepthTriangle {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    double depth;
} SceneEditorMeshPreviewDepthTriangle;

typedef struct SceneEditorMeshPreviewEdgeCandidate {
    uint32_t a;
    uint32_t b;
    double facing;
    SceneEditorMeshPreviewPoint3 normal;
} SceneEditorMeshPreviewEdgeCandidate;

static SceneEditorMeshDisplayMode g_mesh_preview_mode = SCENE_EDITOR_MESH_DISPLAY_SOLID;
static SceneEditorMeshPreviewInstanceCache
    g_mesh_preview_gpu[RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES];

static uint64_t scene_editor_mesh_preview_hash_bytes(uint64_t hash,
                                                     const void* bytes,
                                                     size_t size) {
    const uint8_t* data = (const uint8_t*)bytes;
    for (size_t i = 0u; i < size; ++i) {
        hash ^= (uint64_t)data[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t scene_editor_mesh_preview_signature(
    const SceneEditorDigestOverlayProjector* projector,
    const RayTracingRuntimeMeshAssetInstance* instance,
    const CoreMeshPreviewLodMesh* lod,
    int scene_object_index) {
    uint64_t hash = UINT64_C(1469598103934665603);
    hash = scene_editor_mesh_preview_hash_bytes(hash, projector, sizeof(*projector));
    hash = scene_editor_mesh_preview_hash_bytes(hash,
                                                &instance->asset_index,
                                                sizeof(instance->asset_index));
    hash = scene_editor_mesh_preview_hash_bytes(hash,
                                                &instance->scene_object_index,
                                                sizeof(instance->scene_object_index));
    hash = scene_editor_mesh_preview_hash_bytes(hash,
                                                &instance->position_x,
                                                sizeof(instance->position_x) * 9u);
    hash = scene_editor_mesh_preview_hash_bytes(hash,
                                                &instance->rotation_pivot_policy,
                                                sizeof(instance->rotation_pivot_policy));
    hash = scene_editor_mesh_preview_hash_bytes(hash,
                                                &instance->rotation_pivot_x,
                                                sizeof(instance->rotation_pivot_x) * 3u);
    hash = scene_editor_mesh_preview_hash_bytes(hash, &lod->vertex_count, sizeof(lod->vertex_count));
    hash = scene_editor_mesh_preview_hash_bytes(hash, &lod->triangle_count, sizeof(lod->triangle_count));
    hash = scene_editor_mesh_preview_hash_bytes(hash, &scene_object_index, sizeof(scene_object_index));
    if (scene_object_index >= 0 && scene_object_index < sceneSettings.objectCount) {
        hash = scene_editor_mesh_preview_hash_bytes(
            hash,
            &sceneSettings.sceneObjects[scene_object_index].color,
            sizeof(sceneSettings.sceneObjects[scene_object_index].color));
    }
    return hash;
}

static SceneEditorMeshPreviewPoint3 scene_editor_mesh_preview_rotate(
    SceneEditorMeshPreviewPoint3 point,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    const double cx = cos(instance->rotation_x);
    const double sx = sin(instance->rotation_x);
    const double cy = cos(instance->rotation_y);
    const double sy = sin(instance->rotation_y);
    const double cz = cos(instance->rotation_z);
    const double sz = sin(instance->rotation_z);
    SceneEditorMeshPreviewPoint3 rotated = point;
    double value = 0.0;

    value = rotated.y * cx - rotated.z * sx;
    rotated.z = rotated.y * sx + rotated.z * cx;
    rotated.y = value;
    value = rotated.x * cy + rotated.z * sy;
    rotated.z = -rotated.x * sy + rotated.z * cy;
    rotated.x = value;
    value = rotated.x * cz - rotated.y * sz;
    rotated.y = rotated.x * sz + rotated.y * cz;
    rotated.x = value;
    return rotated;
}

static SceneEditorMeshPreviewPoint3 scene_editor_mesh_preview_pivot(
    const CoreMeshAssetRuntimeContract* contract,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    SceneEditorMeshPreviewPoint3 pivot = {0.0, 0.0, 0.0};
    if (!contract || !instance) return pivot;
    if (instance->rotation_pivot_policy == RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_CUSTOM) {
        pivot.x = instance->rotation_pivot_x * instance->scale_x;
        pivot.y = instance->rotation_pivot_y * instance->scale_y;
        pivot.z = instance->rotation_pivot_z * instance->scale_z;
    } else if (instance->rotation_pivot_policy ==
               RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_BOUNDS_CENTER) {
        pivot.x = (contract->local_bounds.min.x +
                   contract->local_bounds.max.x) * 0.5 * instance->scale_x;
        pivot.y = (contract->local_bounds.min.y +
                   contract->local_bounds.max.y) * 0.5 * instance->scale_y;
        pivot.z = (contract->local_bounds.min.z +
                   contract->local_bounds.max.z) * 0.5 * instance->scale_z;
    }
    return pivot;
}

static SceneEditorMeshPreviewPoint3 scene_editor_mesh_preview_world_point(
    CoreObjectVec3 local,
    const CoreMeshAssetRuntimeContract* contract,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    const SceneEditorMeshPreviewPoint3 pivot =
        scene_editor_mesh_preview_pivot(contract, instance);
    SceneEditorMeshPreviewPoint3 point = {
        local.x * instance->scale_x - pivot.x,
        local.y * instance->scale_y - pivot.y,
        local.z * instance->scale_z - pivot.z
    };
    point = scene_editor_mesh_preview_rotate(point, instance);
    point.x += pivot.x + instance->position_x;
    point.y += pivot.y + instance->position_y;
    point.z += pivot.z + instance->position_z;
    return point;
}

static double scene_editor_mesh_preview_view_depth(
    const SceneEditorDigestOverlayProjector* projector,
    SceneEditorMeshPreviewPoint3 point) {
    const double dx = point.x - projector->center_x;
    const double dy = point.y - projector->center_y;
    const double dz = point.z - projector->center_z;
    const double cy = cos(projector->yaw_rad);
    const double sy = sin(projector->yaw_rad);
    const double cp = cos(projector->pitch_rad);
    const double sp = sin(projector->pitch_rad);
    const double x1 = dx * cy - dy * sy;
    const double y1 = dx * sy + dy * cy;
    (void)x1;
    return y1 * cp - dz * sp;
}

static bool scene_editor_mesh_preview_project(
    const SceneEditorDigestOverlayProjector* projector,
    SceneEditorMeshPreviewPoint3 point,
    SDL_FPoint* out_point) {
    int x = 0;
    int y = 0;
    if (!out_point ||
        !SceneEditorDigestOverlayProjectPoint(projector, point.x, point.y, point.z, &x, &y)) {
        return false;
    }
    out_point->x = (float)x;
    out_point->y = (float)y;
    return true;
}

static int scene_editor_mesh_preview_compare_depth(const void* lhs, const void* rhs) {
    const SceneEditorMeshPreviewDepthTriangle* a =
        (const SceneEditorMeshPreviewDepthTriangle*)lhs;
    const SceneEditorMeshPreviewDepthTriangle* b =
        (const SceneEditorMeshPreviewDepthTriangle*)rhs;
    if (a->depth < b->depth) return 1;
    if (a->depth > b->depth) return -1;
    return 0;
}

static int scene_editor_mesh_preview_compare_edge(const void* lhs, const void* rhs) {
    const SceneEditorMeshPreviewEdgeCandidate* a =
        (const SceneEditorMeshPreviewEdgeCandidate*)lhs;
    const SceneEditorMeshPreviewEdgeCandidate* b =
        (const SceneEditorMeshPreviewEdgeCandidate*)rhs;
    if (a->a < b->a) return -1;
    if (a->a > b->a) return 1;
    if (a->b < b->b) return -1;
    if (a->b > b->b) return 1;
    return 0;
}

static SceneEditorMeshPreviewPoint3 scene_editor_mesh_preview_face_normal(
    SceneEditorMeshPreviewPoint3 a,
    SceneEditorMeshPreviewPoint3 b,
    SceneEditorMeshPreviewPoint3 c) {
    const SceneEditorMeshPreviewPoint3 ab = {b.x - a.x, b.y - a.y, b.z - a.z};
    const SceneEditorMeshPreviewPoint3 ac = {c.x - a.x, c.y - a.y, c.z - a.z};
    SceneEditorMeshPreviewPoint3 normal = {
        ab.y * ac.z - ab.z * ac.y,
        ab.z * ac.x - ab.x * ac.z,
        ab.x * ac.y - ab.y * ac.x
    };
    const double length = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (length > DBL_EPSILON) {
        normal.x /= length;
        normal.y /= length;
        normal.z /= length;
    }
    return normal;
}

static void scene_editor_mesh_preview_add_edge_candidate(
    SceneEditorMeshPreviewEdgeCandidate* edges,
    size_t* edge_count,
    uint32_t a,
    uint32_t b,
    double facing,
    SceneEditorMeshPreviewPoint3 normal) {
    SceneEditorMeshPreviewEdgeCandidate* edge = NULL;
    if (!edges || !edge_count || a == b) return;
    edge = &edges[(*edge_count)++];
    edge->a = a < b ? a : b;
    edge->b = a < b ? b : a;
    edge->facing = facing;
    edge->normal = normal;
}

static bool scene_editor_mesh_preview_edge_is_structural(
    const SceneEditorMeshPreviewEdgeCandidate* edges,
    size_t count) {
    const double feature_cosine = 0.8191520442889918; /* cos(35 degrees) */
    if (!edges || count == 0u) return false;
    if (count != 2u) return true;
    if ((edges[0].facing < 0.0) != (edges[1].facing < 0.0)) return true;
    return edges[0].normal.x * edges[1].normal.x +
               edges[0].normal.y * edges[1].normal.y +
               edges[0].normal.z * edges[1].normal.z < feature_cosine;
}

static SDL_Color scene_editor_mesh_preview_material_color(int scene_object_index) {
    int packed = 0x7396b8;
    if (scene_object_index >= 0 && scene_object_index < sceneSettings.objectCount) {
        packed = sceneSettings.sceneObjects[scene_object_index].color;
    }
    return (SDL_Color){(Uint8)((packed >> 16) & 0xFF),
                       (Uint8)((packed >> 8) & 0xFF),
                       (Uint8)(packed & 0xFF),
                       255};
}

#if USE_VULKAN
static void scene_editor_mesh_preview_destroy_slot(VkRenderer* renderer,
                                                   SceneEditorMeshPreviewGpuSlot* slot) {
    if (!renderer || !slot) return;
    vk_renderer_destroy_tri_mesh(renderer, &slot->solid_mesh);
    vk_renderer_destroy_tri_mesh(renderer, &slot->material_mesh);
    vk_renderer_destroy_line_mesh(renderer, &slot->wire_mesh);
    memset(slot, 0, sizeof(*slot));
}

static bool scene_editor_mesh_preview_build_slot(
    VkRenderer* renderer,
    SceneEditorMeshPreviewGpuSlot* slot,
    uint64_t signature,
    const SceneEditorDigestOverlayProjector* projector,
    const RayTracingRuntimeMeshAssetInstance* instance,
    const CoreMeshAssetRuntimeContract* contract,
    const CoreMeshPreviewLodMesh* lod,
    int scene_object_index) {
    SDL_FPoint* vertices = NULL;
    SDL_FPoint* wire_vertices = NULL;
    uint32_t* sorted_indices = NULL;
    SceneEditorMeshPreviewDepthTriangle* depth_triangles = NULL;
    SceneEditorMeshPreviewEdgeCandidate* edge_candidates = NULL;
    size_t edge_candidate_count = 0u;
    size_t wire_vertex_count = 0u;
    SDL_Color material = scene_editor_mesh_preview_material_color(scene_object_index);
    bool ok = false;

    if (!renderer || !slot || !projector || !instance || !contract || !lod ||
        lod->vertex_count == 0u || lod->triangle_count == 0u || !lod->vertices || !lod->indices ||
        lod->vertex_count > UINT32_MAX || lod->triangle_count > UINT32_MAX / 6u) {
        return false;
    }
    scene_editor_mesh_preview_destroy_slot(renderer, slot);
    vertices = (SDL_FPoint*)calloc(lod->vertex_count, sizeof(*vertices));
    sorted_indices = (uint32_t*)calloc(lod->triangle_count * 3u, sizeof(*sorted_indices));
    depth_triangles = (SceneEditorMeshPreviewDepthTriangle*)calloc(
        lod->triangle_count, sizeof(*depth_triangles));
    edge_candidates = (SceneEditorMeshPreviewEdgeCandidate*)calloc(
        lod->triangle_count * 3u, sizeof(*edge_candidates));
    wire_vertices = (SDL_FPoint*)calloc(lod->triangle_count * 6u, sizeof(*wire_vertices));
    if (!vertices || !sorted_indices || !depth_triangles || !edge_candidates ||
        !wire_vertices) goto cleanup;

    for (size_t i = 0u; i < lod->vertex_count; ++i) {
        const SceneEditorMeshPreviewPoint3 world = scene_editor_mesh_preview_world_point(
            lod->vertices[i], contract, instance);
        if (!scene_editor_mesh_preview_project(projector, world, &vertices[i])) goto cleanup;
    }
    for (size_t i = 0u; i < lod->triangle_count; ++i) {
        const uint32_t a = lod->indices[i * 3u + 0u];
        const uint32_t b = lod->indices[i * 3u + 1u];
        const uint32_t c = lod->indices[i * 3u + 2u];
        SceneEditorMeshPreviewPoint3 wa;
        SceneEditorMeshPreviewPoint3 wb;
        SceneEditorMeshPreviewPoint3 wc;
        SceneEditorMeshPreviewPoint3 normal;
        double area = 0.0;
        if (a >= lod->vertex_count || b >= lod->vertex_count || c >= lod->vertex_count) {
            goto cleanup;
        }
        wa = scene_editor_mesh_preview_world_point(lod->vertices[a], contract, instance);
        wb = scene_editor_mesh_preview_world_point(lod->vertices[b], contract, instance);
        wc = scene_editor_mesh_preview_world_point(lod->vertices[c], contract, instance);
        depth_triangles[i].a = a;
        depth_triangles[i].b = b;
        depth_triangles[i].c = c;
        depth_triangles[i].depth =
            (scene_editor_mesh_preview_view_depth(projector, wa) +
             scene_editor_mesh_preview_view_depth(projector, wb) +
             scene_editor_mesh_preview_view_depth(projector, wc)) / 3.0;
        area = ((double)vertices[b].x - (double)vertices[a].x) *
                   ((double)vertices[c].y - (double)vertices[a].y) -
               ((double)vertices[b].y - (double)vertices[a].y) *
                   ((double)vertices[c].x - (double)vertices[a].x);
        normal = scene_editor_mesh_preview_face_normal(wa, wb, wc);
        scene_editor_mesh_preview_add_edge_candidate(
            edge_candidates, &edge_candidate_count, a, b, area, normal);
        scene_editor_mesh_preview_add_edge_candidate(
            edge_candidates, &edge_candidate_count, b, c, area, normal);
        scene_editor_mesh_preview_add_edge_candidate(
            edge_candidates, &edge_candidate_count, c, a, area, normal);
    }
    qsort(depth_triangles,
          lod->triangle_count,
          sizeof(*depth_triangles),
          scene_editor_mesh_preview_compare_depth);
    for (size_t i = 0u; i < lod->triangle_count; ++i) {
        sorted_indices[i * 3u + 0u] = depth_triangles[i].a;
        sorted_indices[i * 3u + 1u] = depth_triangles[i].b;
        sorted_indices[i * 3u + 2u] = depth_triangles[i].c;
    }
    qsort(edge_candidates,
          edge_candidate_count,
          sizeof(*edge_candidates),
          scene_editor_mesh_preview_compare_edge);
    for (size_t first = 0u; first < edge_candidate_count;) {
        size_t next = first + 1u;
        while (next < edge_candidate_count &&
               edge_candidates[next].a == edge_candidates[first].a &&
               edge_candidates[next].b == edge_candidates[first].b) {
            next += 1u;
        }
        if (scene_editor_mesh_preview_edge_is_structural(&edge_candidates[first], next - first)) {
            wire_vertices[wire_vertex_count++] = vertices[edge_candidates[first].a];
            wire_vertices[wire_vertex_count++] = vertices[edge_candidates[first].b];
        }
        first = next;
    }
    if (vk_renderer_create_tri_mesh(renderer,
                                    vertices,
                                    (uint32_t)lod->vertex_count,
                                    sorted_indices,
                                    (uint32_t)(lod->triangle_count * 3u),
                                    0.48f,
                                    0.53f,
                                    0.58f,
                                    1.0f,
                                    &slot->solid_mesh) != VK_SUCCESS) {
        goto cleanup;
    }
    if (vk_renderer_create_tri_mesh(renderer,
                                    vertices,
                                    (uint32_t)lod->vertex_count,
                                    sorted_indices,
                                    (uint32_t)(lod->triangle_count * 3u),
                                    (float)material.r / 255.0f,
                                    (float)material.g / 255.0f,
                                    (float)material.b / 255.0f,
                                    1.0f,
                                    &slot->material_mesh) != VK_SUCCESS) {
        goto cleanup;
    }
    if (wire_vertex_count >= 2u &&
        vk_renderer_create_line_list_mesh(renderer,
                                          wire_vertices,
                                          (uint32_t)wire_vertex_count,
                                          0.74f,
                                          0.86f,
                                          0.94f,
                                          0.92f,
                                          &slot->wire_mesh) != VK_SUCCESS) {
        goto cleanup;
    }
    slot->signature = signature;
    slot->triangle_count = lod->triangle_count;
    slot->wire_segment_count = wire_vertex_count / 2u;
    slot->valid = true;
    ok = true;

cleanup:
    free(vertices);
    free(wire_vertices);
    free(sorted_indices);
    free(depth_triangles);
    free(edge_candidates);
    if (!ok) scene_editor_mesh_preview_destroy_slot(renderer, slot);
    return ok;
}
#endif

static bool scene_editor_mesh_preview_instance_visible(int active_editor_mode,
                                                       int selected_object_index,
                                                       int scene_object_index) {
    return active_editor_mode != EDITOR_MODE_MATERIAL ||
           selected_object_index < 0 ||
           selected_object_index == scene_object_index;
}

static void scene_editor_mesh_preview_bounds_points(
    const CoreMeshAssetRuntimeContract* contract,
    const RayTracingRuntimeMeshAssetInstance* instance,
    SceneEditorMeshPreviewPoint3 out_points[8]) {
    const CoreObjectVec3 min = contract->local_bounds.min;
    const CoreObjectVec3 max = contract->local_bounds.max;
    int index = 0;
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                CoreObjectVec3 local = {
                    x ? max.x : min.x,
                    y ? max.y : min.y,
                    z ? max.z : min.z
                };
                out_points[index++] =
                    scene_editor_mesh_preview_world_point(local, contract, instance);
            }
        }
    }
}

static void scene_editor_mesh_preview_draw_bounds(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    const CoreMeshAssetRuntimeContract* contract,
    const RayTracingRuntimeMeshAssetInstance* instance,
    SDL_Color color) {
    static const int edges[12][2] = {
        {0, 1}, {0, 2}, {1, 3}, {2, 3},
        {4, 5}, {4, 6}, {5, 7}, {6, 7},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    SceneEditorMeshPreviewPoint3 points[8];
    scene_editor_mesh_preview_bounds_points(contract, instance, points);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int i = 0; i < 12; ++i) {
        SceneEditorDigestOverlayDrawLine3(renderer,
                                         projector,
                                         points[edges[i][0]].x,
                                         points[edges[i][0]].y,
                                         points[edges[i][0]].z,
                                         points[edges[i][1]].x,
                                         points[edges[i][1]].y,
                                         points[edges[i][1]].z,
                                         color);
    }
}

void SceneEditorMeshPreviewRenderReset(SDL_Renderer* renderer) {
#if USE_VULKAN
    if (renderer) {
        VkRenderer* vk = (VkRenderer*)renderer;
        vk_renderer_wait_idle(vk);
        for (int i = 0; i < RAY_TRACING_RUNTIME_MESH_ASSET_MAX_INSTANCES; ++i) {
            for (int slot = 0; slot < SCENE_EDITOR_MESH_PREVIEW_GPU_FRAME_SLOTS; ++slot) {
                scene_editor_mesh_preview_destroy_slot(vk, &g_mesh_preview_gpu[i].slots[slot]);
            }
        }
    }
#else
    (void)renderer;
#endif
    memset(g_mesh_preview_gpu, 0, sizeof(g_mesh_preview_gpu));
    g_mesh_preview_mode = SCENE_EDITOR_MESH_DISPLAY_SOLID;
}

SceneEditorMeshDisplayMode SceneEditorMeshPreviewModeGet(void) {
    return SceneEditorMeshDisplayModeClamp((int)g_mesh_preview_mode);
}

void SceneEditorMeshPreviewModeSet(SceneEditorMeshDisplayMode mode) {
    g_mesh_preview_mode = SceneEditorMeshDisplayModeClamp((int)mode);
}

void SceneEditorMeshPreviewLayoutModeButtons(
    const SDL_Rect* viewport,
    SDL_Rect out_buttons[SCENE_EDITOR_MESH_DISPLAY_COUNT]) {
    if (!out_buttons) return;
    memset(out_buttons, 0, sizeof(SDL_Rect) * SCENE_EDITOR_MESH_DISPLAY_COUNT);
    if (!viewport) return;
    for (int i = 0; i < SCENE_EDITOR_MESH_DISPLAY_COUNT; ++i) {
        out_buttons[i] = (SDL_Rect){
            viewport->x + 12 + i * (SCENE_EDITOR_MESH_PREVIEW_BUTTON_W +
                                    SCENE_EDITOR_MESH_PREVIEW_BUTTON_GAP),
            viewport->y + 12,
            SCENE_EDITOR_MESH_PREVIEW_BUTTON_W,
            SCENE_EDITOR_MESH_PREVIEW_BUTTON_H
        };
    }
}

int SceneEditorMeshPreviewModeButtonAtPoint(const SDL_Rect* viewport, int x, int y) {
    SDL_Rect buttons[SCENE_EDITOR_MESH_DISPLAY_COUNT];
    SceneEditorMeshPreviewLayoutModeButtons(viewport, buttons);
    for (int i = 0; i < SCENE_EDITOR_MESH_DISPLAY_COUNT; ++i) {
        if (x >= buttons[i].x && x < buttons[i].x + buttons[i].w &&
            y >= buttons[i].y && y < buttons[i].y + buttons[i].h) {
            return i;
        }
    }
    return -1;
}

bool SceneEditorMeshPreviewHandleModeClick(const SDL_Rect* viewport, int x, int y) {
    const int mode = SceneEditorMeshPreviewModeButtonAtPoint(viewport, x, y);
    if (mode < 0) return false;
    SceneEditorMeshPreviewModeSet((SceneEditorMeshDisplayMode)mode);
    return true;
}

bool SceneEditorMeshPreviewRenderGeometry(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    int active_editor_mode,
    int selected_object_index,
    int hover_object_index,
    SceneEditorMeshPreviewFrameStats* out_stats) {
    SceneEditorMeshPreviewFrameStats stats = {0};
    stats.mode = SceneEditorMeshPreviewModeGet();
    if (out_stats) *out_stats = stats;
    if (!renderer || !projector) return false;

    for (int i = 0; i < SceneEditorMeshPreviewStoreInstanceCount(); ++i) {
        const RayTracingRuntimeMeshAssetInstance* instance =
            SceneEditorMeshPreviewStoreGetInstance(i);
        const CoreMeshAssetRuntimeContract* contract = NULL;
        const CoreMeshPreviewLodMesh* lod = NULL;
        SDL_Color highlight = {0};
        if (!instance ||
            !scene_editor_mesh_preview_instance_visible(active_editor_mode,
                                                        selected_object_index,
                                                        instance->scene_object_index)) {
            continue;
        }
        contract = SceneEditorMeshPreviewStoreGetContract(instance->asset_index);
        lod = SceneEditorMeshPreviewStoreGet(instance->asset_index);
        if (!contract || !lod) continue;
        if (stats.mode == SCENE_EDITOR_MESH_DISPLAY_BOUNDS) {
            scene_editor_mesh_preview_draw_bounds(renderer,
                                                  projector,
                                                  contract,
                                                  instance,
                                                  (SDL_Color){112, 168, 220, 235});
            stats.rendered_bounds += 1;
            stats.rendered_instances += 1;
        } else {
#if USE_VULKAN
            VkRenderer* vk = (VkRenderer*)renderer;
            int slot_index = (int)vk->current_frame_index;
            SceneEditorMeshPreviewGpuSlot* slot = NULL;
            uint64_t signature = 0u;
            if (slot_index < 0 || slot_index >= SCENE_EDITOR_MESH_PREVIEW_GPU_FRAME_SLOTS) {
                slot_index = 0;
            }
            slot = &g_mesh_preview_gpu[i].slots[slot_index];
            signature = scene_editor_mesh_preview_signature(projector,
                                                            instance,
                                                            lod,
                                                            instance->scene_object_index);
            if ((!slot->valid || slot->signature != signature) &&
                !scene_editor_mesh_preview_build_slot(vk,
                                                      slot,
                                                      signature,
                                                      projector,
                                                      instance,
                                                      contract,
                                                      lod,
                                                      instance->scene_object_index)) {
                continue;
            }
            if (stats.mode == SCENE_EDITOR_MESH_DISPLAY_WIRE) {
                vk_renderer_draw_line_mesh(vk, &slot->wire_mesh);
                stats.rendered_wire_segments += slot->wire_segment_count;
            } else {
                vk_renderer_draw_tri_mesh(
                    vk,
                    stats.mode == SCENE_EDITOR_MESH_DISPLAY_MATERIAL
                        ? &slot->material_mesh
                        : &slot->solid_mesh);
                stats.rendered_triangles += slot->triangle_count;
                if (SceneEditorMeshDisplayModeDrawsStructuralWire(stats.mode)) {
                    vk_renderer_draw_line_mesh(vk, &slot->wire_mesh);
                    stats.rendered_wire_segments += slot->wire_segment_count;
                }
            }
            stats.rendered_instances += 1;
#else
            (void)lod;
#endif
        }
        if (instance->scene_object_index == selected_object_index) {
            highlight = (SDL_Color){255, 126, 72, 255};
        } else if (instance->scene_object_index == hover_object_index) {
            highlight = (SDL_Color){92, 228, 255, 245};
        }
        if (highlight.a != 0u) {
            scene_editor_mesh_preview_draw_bounds(renderer,
                                                  projector,
                                                  contract,
                                                  instance,
                                                  highlight);
        }
    }
    if (out_stats) *out_stats = stats;
    return stats.rendered_instances > 0;
}

void SceneEditorMeshPreviewRenderToolbar(SDL_Renderer* renderer, const SDL_Rect* viewport) {
    SDL_Rect buttons[SCENE_EDITOR_MESH_DISPLAY_COUNT];
    const SceneEditorMeshDisplayMode active = SceneEditorMeshPreviewModeGet();
    if (!renderer || !viewport) return;
    SceneEditorMeshPreviewLayoutModeButtons(viewport, buttons);
    for (int i = 0; i < SCENE_EDITOR_MESH_DISPLAY_COUNT; ++i) {
        const bool selected = i == (int)active;
        const SDL_Color fill = selected ? (SDL_Color){62, 124, 184, 235}
                                        : (SDL_Color){28, 34, 42, 220};
        const SDL_Color border = selected ? (SDL_Color){178, 224, 255, 255}
                                          : (SDL_Color){92, 108, 126, 235};
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_RenderFillRect(renderer, &buttons[i]);
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
        SDL_RenderDrawRect(renderer, &buttons[i]);
        RenderLabelTextLeft(renderer,
                            (SDL_Rect){buttons[i].x + 8,
                                       buttons[i].y + 3,
                                       buttons[i].w - 16,
                                       buttons[i].h - 6},
                            SceneEditorMeshDisplayModeName((SceneEditorMeshDisplayMode)i),
                            (SDL_Color){232, 240, 248, 255});
    }
}

static bool scene_editor_mesh_preview_point_in_triangle(double px,
                                                        double py,
                                                        SDL_FPoint a,
                                                        SDL_FPoint b,
                                                        SDL_FPoint c) {
    const double d1 = (px - b.x) * (a.y - b.y) - (a.x - b.x) * (py - b.y);
    const double d2 = (px - c.x) * (b.y - c.y) - (b.x - c.x) * (py - c.y);
    const double d3 = (px - a.x) * (c.y - a.y) - (c.x - a.x) * (py - a.y);
    const bool has_neg = d1 < 0.0 || d2 < 0.0 || d3 < 0.0;
    const bool has_pos = d1 > 0.0 || d2 > 0.0 || d3 > 0.0;
    return !(has_neg && has_pos);
}

static bool scene_editor_mesh_preview_pick_bounds(
    const SceneEditorDigestOverlayProjector* projector,
    const CoreMeshAssetRuntimeContract* contract,
    const RayTracingRuntimeMeshAssetInstance* instance,
    int screen_x,
    int screen_y,
    double* out_area) {
    SceneEditorMeshPreviewPoint3 points[8];
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;
    bool seeded = false;
    scene_editor_mesh_preview_bounds_points(contract, instance, points);
    for (int i = 0; i < 8; ++i) {
        SDL_FPoint projected;
        if (!scene_editor_mesh_preview_project(projector, points[i], &projected)) continue;
        if (!seeded) {
            min_x = max_x = (int)projected.x;
            min_y = max_y = (int)projected.y;
            seeded = true;
        } else {
            if ((int)projected.x < min_x) min_x = (int)projected.x;
            if ((int)projected.x > max_x) max_x = (int)projected.x;
            if ((int)projected.y < min_y) min_y = (int)projected.y;
            if ((int)projected.y > max_y) max_y = (int)projected.y;
        }
    }
    if (!seeded || screen_x < min_x - 5 || screen_x > max_x + 5 ||
        screen_y < min_y - 5 || screen_y > max_y + 5) {
        return false;
    }
    if (out_area) *out_area = (double)(max_x - min_x + 11) * (double)(max_y - min_y + 11);
    return true;
}

int SceneEditorMeshPreviewPickObjectIndex(
    const SceneEditorDigestOverlayProjector* projector,
    int active_editor_mode,
    int selected_object_index,
    int screen_x,
    int screen_y) {
    int picked = -1;
    double picked_depth = -DBL_MAX;
    double picked_area = DBL_MAX;
    if (!projector ||
        SceneEditorMeshPreviewModeButtonAtPoint(&projector->viewport, screen_x, screen_y) >= 0) {
        return -1;
    }
    for (int i = 0; i < SceneEditorMeshPreviewStoreInstanceCount(); ++i) {
        const RayTracingRuntimeMeshAssetInstance* instance =
            SceneEditorMeshPreviewStoreGetInstance(i);
        const CoreMeshAssetRuntimeContract* contract = NULL;
        const CoreMeshPreviewLodMesh* lod = NULL;
        if (!instance ||
            !scene_editor_mesh_preview_instance_visible(active_editor_mode,
                                                        selected_object_index,
                                                        instance->scene_object_index)) {
            continue;
        }
        contract = SceneEditorMeshPreviewStoreGetContract(instance->asset_index);
        lod = SceneEditorMeshPreviewStoreGet(instance->asset_index);
        if (!contract || !lod) continue;
        if (SceneEditorMeshPreviewModeGet() == SCENE_EDITOR_MESH_DISPLAY_BOUNDS) {
            double area = 0.0;
            if (scene_editor_mesh_preview_pick_bounds(projector,
                                                      contract,
                                                      instance,
                                                      screen_x,
                                                      screen_y,
                                                      &area) && area < picked_area) {
                picked = instance->scene_object_index;
                picked_area = area;
            }
            continue;
        }
        for (size_t t = 0u; t < lod->triangle_count; ++t) {
            const uint32_t ia = lod->indices[t * 3u + 0u];
            const uint32_t ib = lod->indices[t * 3u + 1u];
            const uint32_t ic = lod->indices[t * 3u + 2u];
            SceneEditorMeshPreviewPoint3 wa;
            SceneEditorMeshPreviewPoint3 wb;
            SceneEditorMeshPreviewPoint3 wc;
            SDL_FPoint a;
            SDL_FPoint b;
            SDL_FPoint c;
            double depth = 0.0;
            if (ia >= lod->vertex_count || ib >= lod->vertex_count || ic >= lod->vertex_count) {
                continue;
            }
            wa = scene_editor_mesh_preview_world_point(lod->vertices[ia], contract, instance);
            wb = scene_editor_mesh_preview_world_point(lod->vertices[ib], contract, instance);
            wc = scene_editor_mesh_preview_world_point(lod->vertices[ic], contract, instance);
            if (!scene_editor_mesh_preview_project(projector, wa, &a) ||
                !scene_editor_mesh_preview_project(projector, wb, &b) ||
                !scene_editor_mesh_preview_project(projector, wc, &c) ||
                !scene_editor_mesh_preview_point_in_triangle(screen_x, screen_y, a, b, c)) {
                continue;
            }
            depth = (scene_editor_mesh_preview_view_depth(projector, wa) +
                     scene_editor_mesh_preview_view_depth(projector, wb) +
                     scene_editor_mesh_preview_view_depth(projector, wc)) / 3.0;
            if (picked < 0 || depth > picked_depth) {
                picked = instance->scene_object_index;
                picked_depth = depth;
            }
        }
    }
    return picked;
}

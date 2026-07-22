#include "editor/scene_editor_mesh_preview_surface.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "core_time.h"
#include "editor/editor_mode_router.h"
#include "editor/scene_editor_mesh_preview_outline.h"
#include "editor/scene_editor_mesh_preview_shading.h"
#include "editor/scene_editor_mesh_preview_store.h"
#include "editor/scene_editor_primitive_preview_geometry.h"
#include "import/runtime_mesh_asset_loader.h"
#include "vk_renderer.h"

#define SCENE_EDITOR_MESH_SURFACE_INTERACTIVE_SCALE 0.75
#define SCENE_EDITOR_MESH_SURFACE_SETTLED_SCALE 1.00
#define SCENE_EDITOR_MESH_SURFACE_SETTLE_NS UINT64_C(150000000)

typedef struct SceneEditorMeshSurfacePoint3 {
    double x;
    double y;
    double z;
} SceneEditorMeshSurfacePoint3;

typedef struct SceneEditorMeshSurfaceVertex {
    double x;
    double y;
    double depth;
} SceneEditorMeshSurfaceVertex;

typedef struct SceneEditorMeshSurfaceCache {
    VkRenderer* renderer;
    VkRendererTexture texture;
    bool texture_valid;
    uint8_t* rgba;
    double* depth;
    int* owner;
    int width;
    int height;
    uint64_t signature;
    bool signature_valid;
    CoreTimeNs changed_at;
    bool rendered_interactive;
    bool pixels_valid;
    SceneEditorMeshPreviewFrameStats stats;
} SceneEditorMeshSurfaceCache;

static SceneEditorMeshSurfaceCache g_surface;

static uint64_t scene_editor_mesh_surface_hash(uint64_t hash,
                                               const void* bytes,
                                               size_t size) {
    const uint8_t* data = (const uint8_t*)bytes;
    for (size_t i = 0u; i < size; ++i) {
        hash ^= (uint64_t)data[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static bool scene_editor_mesh_surface_visible(int active_mode,
                                              int selected_object_index,
                                              int scene_object_index) {
    return active_mode != EDITOR_MODE_MATERIAL ||
           selected_object_index < 0 ||
           selected_object_index == scene_object_index;
}

static uint64_t scene_editor_mesh_surface_signature(
    const SceneEditorDigestOverlayProjector* projector,
    int active_mode,
    int selected_object_index,
    SceneEditorMeshDisplayMode mode) {
    RuntimeSceneBridge3DPrimitiveSeedState seeds = {0};
    uint64_t hash = UINT64_C(1469598103934665603);
    hash = scene_editor_mesh_surface_hash(hash, projector, sizeof(*projector));
    hash = scene_editor_mesh_surface_hash(hash, &active_mode, sizeof(active_mode));
    hash = scene_editor_mesh_surface_hash(hash, &selected_object_index,
                                          sizeof(selected_object_index));
    hash = scene_editor_mesh_surface_hash(hash, &mode, sizeof(mode));
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    hash = scene_editor_mesh_surface_hash(hash, &seeds.valid, sizeof(seeds.valid));
    hash = scene_editor_mesh_surface_hash(hash,
                                          &seeds.primitive_count,
                                          sizeof(seeds.primitive_count));
    for (int i = 0; seeds.valid && i < seeds.primitive_count; ++i) {
        hash = scene_editor_mesh_surface_hash(hash,
                                              &seeds.primitives[i],
                                              sizeof(seeds.primitives[i]));
        if (seeds.primitives[i].scene_object_index >= 0 &&
            seeds.primitives[i].scene_object_index < sceneSettings.objectCount) {
            hash = scene_editor_mesh_surface_hash(
                hash,
                &sceneSettings.sceneObjects[seeds.primitives[i].scene_object_index].color,
                sizeof(sceneSettings.sceneObjects[seeds.primitives[i].scene_object_index].color));
        }
    }
    for (int i = 0; i < SceneEditorMeshPreviewStoreInstanceCount(); ++i) {
        const RayTracingRuntimeMeshAssetInstance* instance =
            SceneEditorMeshPreviewStoreGetInstance(i);
        const CoreMeshPreviewLodMesh* lod = instance
                                                ? SceneEditorMeshPreviewStoreGet(
                                                      instance->asset_index)
                                                : NULL;
        if (!instance || !lod ||
            !scene_editor_mesh_surface_visible(active_mode,
                                               selected_object_index,
                                               instance->scene_object_index)) {
            continue;
        }
        hash = scene_editor_mesh_surface_hash(hash, instance, sizeof(*instance));
        hash = scene_editor_mesh_surface_hash(hash, &lod->vertex_count,
                                              sizeof(lod->vertex_count));
        hash = scene_editor_mesh_surface_hash(hash, &lod->triangle_count,
                                              sizeof(lod->triangle_count));
        if (instance->scene_object_index >= 0 &&
            instance->scene_object_index < sceneSettings.objectCount) {
            hash = scene_editor_mesh_surface_hash(
                hash,
                &sceneSettings.sceneObjects[instance->scene_object_index].color,
                sizeof(sceneSettings.sceneObjects[instance->scene_object_index].color));
        }
    }
    return hash;
}

static SceneEditorMeshSurfacePoint3 scene_editor_mesh_surface_rotate(
    SceneEditorMeshSurfacePoint3 point,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    const double cx = cos(instance->rotation_x);
    const double sx = sin(instance->rotation_x);
    const double cy = cos(instance->rotation_y);
    const double sy = sin(instance->rotation_y);
    const double cz = cos(instance->rotation_z);
    const double sz = sin(instance->rotation_z);
    double value = point.y * cx - point.z * sx;
    point.z = point.y * sx + point.z * cx;
    point.y = value;
    value = point.x * cy + point.z * sy;
    point.z = -point.x * sy + point.z * cy;
    point.x = value;
    value = point.x * cz - point.y * sz;
    point.y = point.x * sz + point.y * cz;
    point.x = value;
    return point;
}

static SceneEditorMeshSurfacePoint3 scene_editor_mesh_surface_world(
    CoreObjectVec3 local,
    const CoreMeshAssetRuntimeContract* contract,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    SceneEditorMeshSurfacePoint3 pivot = {0.0, 0.0, 0.0};
    SceneEditorMeshSurfacePoint3 point;
    if (instance->rotation_pivot_policy == RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_CUSTOM) {
        pivot = (SceneEditorMeshSurfacePoint3){instance->rotation_pivot_x * instance->scale_x,
                                               instance->rotation_pivot_y * instance->scale_y,
                                               instance->rotation_pivot_z * instance->scale_z};
    } else if (instance->rotation_pivot_policy ==
               RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_BOUNDS_CENTER) {
        pivot = (SceneEditorMeshSurfacePoint3){
            (contract->local_bounds.min.x + contract->local_bounds.max.x) *
                0.5 * instance->scale_x,
            (contract->local_bounds.min.y + contract->local_bounds.max.y) *
                0.5 * instance->scale_y,
            (contract->local_bounds.min.z + contract->local_bounds.max.z) *
                0.5 * instance->scale_z};
    }
    point = (SceneEditorMeshSurfacePoint3){local.x * instance->scale_x - pivot.x,
                                           local.y * instance->scale_y - pivot.y,
                                           local.z * instance->scale_z - pivot.z};
    point = scene_editor_mesh_surface_rotate(point, instance);
    point.x += pivot.x + instance->position_x;
    point.y += pivot.y + instance->position_y;
    point.z += pivot.z + instance->position_z;
    return point;
}

static SceneEditorMeshPreviewShadeNormal scene_editor_mesh_surface_world_normal(
    CoreObjectVec3 local,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    SceneEditorMeshSurfacePoint3 normal = {local.x, local.y, local.z};
    if (!instance) return (SceneEditorMeshPreviewShadeNormal){normal.x, normal.y, normal.z};
    if (fabs(instance->scale_x) > 1e-12) normal.x /= instance->scale_x;
    if (fabs(instance->scale_y) > 1e-12) normal.y /= instance->scale_y;
    if (fabs(instance->scale_z) > 1e-12) normal.z /= instance->scale_z;
    normal = scene_editor_mesh_surface_rotate(normal, instance);
    return (SceneEditorMeshPreviewShadeNormal){normal.x, normal.y, normal.z};
}

static double scene_editor_mesh_surface_depth(
    const SceneEditorDigestOverlayProjector* projector,
    SceneEditorMeshSurfacePoint3 point) {
    return SceneEditorDigestOverlayViewDepth(projector, point.x, point.y, point.z);
}

static bool scene_editor_mesh_surface_project(
    const SceneEditorDigestOverlayProjector* projector,
    SceneEditorMeshSurfacePoint3 world,
    double scale,
    SceneEditorMeshSurfaceVertex* out) {
    double x = 0.0;
    double y = 0.0;
    if (!out || !SceneEditorDigestOverlayProjectPointF(projector,
                                                        world.x,
                                                        world.y,
                                                        world.z,
                                                        &x,
                                                        &y)) {
        return false;
    }
    *out = (SceneEditorMeshSurfaceVertex){
        (x - (double)projector->viewport.x) * scale,
        (y - (double)projector->viewport.y) * scale,
        scene_editor_mesh_surface_depth(projector, world)};
    return true;
}

static double scene_editor_mesh_surface_edge(double ax,
                                             double ay,
                                             double bx,
                                             double by,
                                             double px,
                                             double py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static SceneEditorMeshPreviewShadeNormal scene_editor_mesh_surface_normal(
    SceneEditorMeshSurfacePoint3 a,
    SceneEditorMeshSurfacePoint3 b,
    SceneEditorMeshSurfacePoint3 c) {
    const SceneEditorMeshSurfacePoint3 ab = {b.x - a.x, b.y - a.y, b.z - a.z};
    const SceneEditorMeshSurfacePoint3 ac = {c.x - a.x, c.y - a.y, c.z - a.z};
    return (SceneEditorMeshPreviewShadeNormal){ab.y * ac.z - ab.z * ac.y,
                                               ab.z * ac.x - ab.x * ac.z,
                                               ab.x * ac.y - ab.y * ac.x};
}

static SDL_Color scene_editor_mesh_surface_base_color(SceneEditorMeshDisplayMode mode,
                                                       int scene_object_index) {
    int packed = 0x7396b8;
    if (mode == SCENE_EDITOR_MESH_DISPLAY_SOLID) {
        return (SDL_Color){104u, 166u, 218u, 255u};
    }
    if (scene_object_index >= 0 && scene_object_index < sceneSettings.objectCount) {
        packed = sceneSettings.sceneObjects[scene_object_index].color;
    }
    return (SDL_Color){(Uint8)((packed >> 16) & 0xff),
                       (Uint8)((packed >> 8) & 0xff),
                       (Uint8)(packed & 0xff),
                       255u};
}

static void scene_editor_primitive_surface_rasterize_triangle(
    const SceneEditorDigestOverlayProjector* projector,
    const SceneEditorPrimitivePreviewTriangle* triangle,
    int scene_object_index,
    SceneEditorMeshDisplayMode mode,
    double scale,
    SceneEditorMeshPreviewFrameStats* stats) {
    const SDL_Color base = scene_editor_mesh_surface_base_color(mode, scene_object_index);
    const SceneEditorMeshSurfacePoint3 wa = {triangle->a.x, triangle->a.y, triangle->a.z};
    const SceneEditorMeshSurfacePoint3 wb = {triangle->b.x, triangle->b.y, triangle->b.z};
    const SceneEditorMeshSurfacePoint3 wc = {triangle->c.x, triangle->c.y, triangle->c.z};
    SceneEditorMeshSurfaceVertex a;
    SceneEditorMeshSurfaceVertex b;
    SceneEditorMeshSurfaceVertex c;
    SceneEditorMeshPreviewShadeNormal normal;
    double area = 0.0;
    int min_x = 0;
    int max_x = 0;
    int min_y = 0;
    int max_y = 0;

    if (!projector || !triangle || !stats ||
        !scene_editor_mesh_surface_project(projector, wa, scale, &a) ||
        !scene_editor_mesh_surface_project(projector, wb, scale, &b) ||
        !scene_editor_mesh_surface_project(projector, wc, scale, &c)) {
        return;
    }
    area = scene_editor_mesh_surface_edge(a.x, a.y, b.x, b.y, c.x, c.y);
    if (!isfinite(area) || fabs(area) <= 1e-8) return;
    min_x = (int)floor(fmin(a.x, fmin(b.x, c.x)));
    max_x = (int)ceil(fmax(a.x, fmax(b.x, c.x)));
    min_y = (int)floor(fmin(a.y, fmin(b.y, c.y)));
    max_y = (int)ceil(fmax(a.y, fmax(b.y, c.y)));
    if (max_x < 0 || max_y < 0 || min_x >= g_surface.width || min_y >= g_surface.height) {
        return;
    }
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= g_surface.width) max_x = g_surface.width - 1;
    if (max_y >= g_surface.height) max_y = g_surface.height - 1;
    normal = scene_editor_mesh_surface_normal(wa, wb, wc);
    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            const double px = (double)x + 0.5;
            const double py = (double)y + 0.5;
            const double w0 = scene_editor_mesh_surface_edge(b.x, b.y, c.x, c.y, px, py) / area;
            const double w1 = scene_editor_mesh_surface_edge(c.x, c.y, a.x, a.y, px, py) / area;
            const double w2 = 1.0 - w0 - w1;
            const size_t pixel = (size_t)y * (size_t)g_surface.width + (size_t)x;
            double depth = 0.0;
            SDL_Color color;
            if (w0 < -1e-4 || w1 < -1e-4 || w2 < -1e-4) continue;
            depth = w0 * a.depth + w1 * b.depth + w2 * c.depth;
            if (!SceneEditorMeshPreviewDepthWins(depth, g_surface.depth[pixel])) continue;
            color = SceneEditorMeshPreviewShadeColor(base, normal);
            g_surface.depth[pixel] = depth;
            g_surface.owner[pixel] = scene_object_index;
            g_surface.rgba[pixel * 4u + 0u] = color.r;
            g_surface.rgba[pixel * 4u + 1u] = color.g;
            g_surface.rgba[pixel * 4u + 2u] = color.b;
            g_surface.rgba[pixel * 4u + 3u] = color.a;
        }
    }
    stats->rendered_triangles += 1u;
}

static void scene_editor_primitive_surface_rasterize(
    const SceneEditorDigestOverlayProjector* projector,
    const RuntimeSceneBridgePrimitiveSeed* primitive,
    SceneEditorMeshDisplayMode mode,
    double scale,
    SceneEditorMeshPreviewFrameStats* stats) {
    SceneEditorPrimitivePreviewTriangle
        triangles[SCENE_EDITOR_PRIMITIVE_PREVIEW_MAX_TRIANGLES];
    size_t triangle_count = 0u;
    if (!primitive || !stats ||
        !SceneEditorPrimitivePreviewBuildTriangles(primitive,
                                                   triangles,
                                                   &triangle_count)) {
        return;
    }
    for (size_t i = 0u; i < triangle_count; ++i) {
        scene_editor_primitive_surface_rasterize_triangle(projector,
                                                          &triangles[i],
                                                          primitive->scene_object_index,
                                                          mode,
                                                          scale,
                                                          stats);
    }
    stats->rendered_instances += 1;
}

static void scene_editor_mesh_surface_rasterize(
    const SceneEditorDigestOverlayProjector* projector,
    const RayTracingRuntimeMeshAssetInstance* instance,
    const CoreMeshAssetRuntimeContract* contract,
    const CoreMeshPreviewLodMesh* lod,
    SceneEditorMeshDisplayMode mode,
    double scale,
    bool interactive,
    SceneEditorMeshPreviewFrameStats* stats) {
    const SDL_Color base = scene_editor_mesh_surface_base_color(
        mode,
        instance->scene_object_index);
    for (size_t triangle = 0u; triangle < lod->triangle_count; ++triangle) {
        const uint32_t ia = lod->indices[triangle * 3u + 0u];
        const uint32_t ib = lod->indices[triangle * 3u + 1u];
        const uint32_t ic = lod->indices[triangle * 3u + 2u];
        SceneEditorMeshSurfacePoint3 wa;
        SceneEditorMeshSurfacePoint3 wb;
        SceneEditorMeshSurfacePoint3 wc;
        SceneEditorMeshSurfaceVertex a;
        SceneEditorMeshSurfaceVertex b;
        SceneEditorMeshSurfaceVertex c;
        SceneEditorMeshPreviewShadeNormal normal_a;
        SceneEditorMeshPreviewShadeNormal normal_b;
        SceneEditorMeshPreviewShadeNormal normal_c;
        SceneEditorMeshPreviewShadeNormal face_normal;
        double area = 0.0;
        int min_x = 0;
        int max_x = 0;
        int min_y = 0;
        int max_y = 0;
        SDL_Color color;
        if (ia >= lod->vertex_count || ib >= lod->vertex_count || ic >= lod->vertex_count) {
            continue;
        }
        wa = scene_editor_mesh_surface_world(lod->vertices[ia], contract, instance);
        wb = scene_editor_mesh_surface_world(lod->vertices[ib], contract, instance);
        wc = scene_editor_mesh_surface_world(lod->vertices[ic], contract, instance);
        if (!scene_editor_mesh_surface_project(projector, wa, scale, &a) ||
            !scene_editor_mesh_surface_project(projector, wb, scale, &b) ||
            !scene_editor_mesh_surface_project(projector, wc, scale, &c)) {
            continue;
        }
        area = scene_editor_mesh_surface_edge(a.x, a.y, b.x, b.y, c.x, c.y);
        if (!isfinite(area) || fabs(area) <= 1e-8) continue;
        min_x = (int)floor(fmin(a.x, fmin(b.x, c.x)));
        max_x = (int)ceil(fmax(a.x, fmax(b.x, c.x)));
        min_y = (int)floor(fmin(a.y, fmin(b.y, c.y)));
        max_y = (int)ceil(fmax(a.y, fmax(b.y, c.y)));
        if (max_x < 0 || max_y < 0 || min_x >= g_surface.width || min_y >= g_surface.height) {
            continue;
        }
        if (min_x < 0) min_x = 0;
        if (min_y < 0) min_y = 0;
        if (max_x >= g_surface.width) max_x = g_surface.width - 1;
        if (max_y >= g_surface.height) max_y = g_surface.height - 1;
        face_normal = scene_editor_mesh_surface_normal(wa, wb, wc);
        normal_a = face_normal;
        normal_b = face_normal;
        normal_c = face_normal;
        {
            CoreObjectVec3 local_normal;
            if (SceneEditorMeshPreviewStoreGetVertexNormal(
                    instance->asset_index, ia, interactive, &local_normal)) {
                normal_a = scene_editor_mesh_surface_world_normal(local_normal, instance);
            }
            if (SceneEditorMeshPreviewStoreGetVertexNormal(
                    instance->asset_index, ib, interactive, &local_normal)) {
                normal_b = scene_editor_mesh_surface_world_normal(local_normal, instance);
            }
            if (SceneEditorMeshPreviewStoreGetVertexNormal(
                    instance->asset_index, ic, interactive, &local_normal)) {
                normal_c = scene_editor_mesh_surface_world_normal(local_normal, instance);
            }
        }
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                const double px = (double)x + 0.5;
                const double py = (double)y + 0.5;
                const double w0 = scene_editor_mesh_surface_edge(b.x, b.y, c.x, c.y, px, py) / area;
                const double w1 = scene_editor_mesh_surface_edge(c.x, c.y, a.x, a.y, px, py) / area;
                const double w2 = 1.0 - w0 - w1;
                const size_t pixel = (size_t)y * (size_t)g_surface.width + (size_t)x;
                double depth = 0.0;
                if (w0 < -1e-4 || w1 < -1e-4 || w2 < -1e-4) continue;
                depth = w0 * a.depth + w1 * b.depth + w2 * c.depth;
                if (!SceneEditorMeshPreviewDepthWins(depth, g_surface.depth[pixel])) continue;
                color = SceneEditorMeshPreviewShadeColor(
                    base,
                    (SceneEditorMeshPreviewShadeNormal){w0 * normal_a.x + w1 * normal_b.x + w2 * normal_c.x,
                                                         w0 * normal_a.y + w1 * normal_b.y + w2 * normal_c.y,
                                                         w0 * normal_a.z + w1 * normal_b.z + w2 * normal_c.z});
                g_surface.depth[pixel] = depth;
                g_surface.owner[pixel] = instance->scene_object_index;
                g_surface.rgba[pixel * 4u + 0u] = color.r;
                g_surface.rgba[pixel * 4u + 1u] = color.g;
                g_surface.rgba[pixel * 4u + 2u] = color.b;
                g_surface.rgba[pixel * 4u + 3u] = color.a;
            }
        }
        stats->rendered_triangles += 1u;
    }
}

static bool scene_editor_mesh_surface_prepare(int width, int height) {
    const size_t pixels = (size_t)width * (size_t)height;
    if (width <= 0 || height <= 0 || pixels > SIZE_MAX / 4u) return false;
    if (g_surface.width != width || g_surface.height != height) {
        uint8_t* rgba = (uint8_t*)malloc(pixels * 4u);
        double* depth = (double*)malloc(pixels * sizeof(*depth));
        int* owner = (int*)malloc(pixels * sizeof(*owner));
        if (!rgba || !depth || !owner) {
            free(rgba);
            free(depth);
            free(owner);
            return false;
        }
        free(g_surface.rgba);
        free(g_surface.depth);
        free(g_surface.owner);
        g_surface.rgba = rgba;
        g_surface.depth = depth;
        g_surface.owner = owner;
        g_surface.width = width;
        g_surface.height = height;
    }
    memset(g_surface.rgba, 0, pixels * 4u);
    for (size_t i = 0u; i < pixels; ++i) {
        g_surface.depth[i] = SceneEditorMeshPreviewDepthClearValue();
        g_surface.owner[i] = -1;
    }
    return true;
}

static bool scene_editor_mesh_surface_upload(VkRenderer* renderer) {
    VkResult result;
    if (g_surface.texture_valid &&
        (g_surface.texture.width != (uint32_t)g_surface.width ||
         g_surface.texture.height != (uint32_t)g_surface.height)) {
        vk_renderer_wait_idle(renderer);
        vk_renderer_texture_destroy(renderer, &g_surface.texture);
        memset(&g_surface.texture, 0, sizeof(g_surface.texture));
        g_surface.texture_valid = false;
    }
    if (!g_surface.texture_valid) {
        result = vk_renderer_texture_create_from_rgba(renderer,
                                                      g_surface.rgba,
                                                      (uint32_t)g_surface.width,
                                                      (uint32_t)g_surface.height,
                                                      VK_FILTER_LINEAR,
                                                      &g_surface.texture);
        g_surface.texture_valid = result == VK_SUCCESS;
        return g_surface.texture_valid;
    }
    return vk_renderer_texture_update_rgba_subrect(renderer,
                                                    &g_surface.texture,
                                                    g_surface.rgba,
                                                    (size_t)g_surface.width * 4u,
                                                    0u,
                                                    0u,
                                                    (uint32_t)g_surface.width,
                                                    (uint32_t)g_surface.height) == VK_SUCCESS;
}

bool SceneEditorMeshPreviewSurfaceRender(
    SDL_Renderer* renderer,
    const SceneEditorDigestOverlayProjector* projector,
    int active_editor_mode,
    int selected_object_index,
    int hover_object_index,
    SceneEditorMeshDisplayMode mode,
    SceneEditorMeshPreviewFrameStats* out_stats) {
    VkRenderer* vk = (VkRenderer*)renderer;
    const CoreTimeNs now = core_time_now_ns();
    uint64_t signature = 0u;
    bool changed = false;
    bool interactive;
    bool reraster;
    double scale;
    SDL_Rect destination;
    if (!renderer || !projector ||
        (mode != SCENE_EDITOR_MESH_DISPLAY_SOLID &&
         mode != SCENE_EDITOR_MESH_DISPLAY_MATERIAL)) {
        return false;
    }
    signature = scene_editor_mesh_surface_signature(projector,
                                                    active_editor_mode,
                                                    selected_object_index,
                                                    mode);
    changed = !g_surface.signature_valid || signature != g_surface.signature;
    if (changed) g_surface.changed_at = now;
    interactive = now != 0u &&
                  core_time_diff_ns(now, g_surface.changed_at) <
                      SCENE_EDITOR_MESH_SURFACE_SETTLE_NS;
    reraster = changed || !g_surface.pixels_valid ||
               interactive != g_surface.rendered_interactive;
    g_surface.signature = signature;
    g_surface.signature_valid = true;
    if (reraster) {
        RuntimeSceneBridge3DPrimitiveSeedState seeds = {0};
        SceneEditorMeshPreviewFrameStats stats = {0};
        scale = interactive ? SCENE_EDITOR_MESH_SURFACE_INTERACTIVE_SCALE
                            : SCENE_EDITOR_MESH_SURFACE_SETTLED_SCALE;
        stats.mode = mode;
        if (!scene_editor_mesh_surface_prepare(
                (int)ceil((double)projector->viewport.w * scale),
                (int)ceil((double)projector->viewport.h * scale))) {
            return false;
        }
        runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
        for (int i = 0; seeds.valid && i < seeds.primitive_count; ++i) {
            const RuntimeSceneBridgePrimitiveSeed* primitive = &seeds.primitives[i];
            if (primitive->guide_only) continue;
            if (!scene_editor_mesh_surface_visible(active_editor_mode,
                                                   selected_object_index,
                                                   primitive->scene_object_index)) {
                continue;
            }
            scene_editor_primitive_surface_rasterize(projector,
                                                     primitive,
                                                     mode,
                                                     scale,
                                                     &stats);
        }
        for (int i = 0; i < SceneEditorMeshPreviewStoreInstanceCount(); ++i) {
            const RayTracingRuntimeMeshAssetInstance* instance =
                SceneEditorMeshPreviewStoreGetInstance(i);
            const CoreMeshAssetRuntimeContract* contract = instance
                                                               ? SceneEditorMeshPreviewStoreGetContract(
                                                                     instance->asset_index)
                                                               : NULL;
            const CoreMeshPreviewLodMesh* lod = instance
                                                    ? SceneEditorMeshPreviewStoreGetForQuality(
                                                          instance->asset_index,
                                                          interactive)
                                                    : NULL;
            if (!instance || !contract || !lod ||
                !scene_editor_mesh_surface_visible(active_editor_mode,
                                                   selected_object_index,
                                                   instance->scene_object_index)) {
                continue;
            }
            scene_editor_mesh_surface_rasterize(projector,
                                                instance,
                                                contract,
                                                lod,
                                                mode,
                                                scale,
                                                interactive,
                                                &stats);
            stats.rendered_instances += 1;
        }
        stats.rendered_outline_pixels = SceneEditorMeshPreviewApplyOutlines(
            g_surface.rgba,
            g_surface.depth,
            g_surface.owner,
            g_surface.width,
            g_surface.height,
            selected_object_index,
            hover_object_index);
        if (!scene_editor_mesh_surface_upload(vk)) return false;
        g_surface.renderer = vk;
        g_surface.stats = stats;
        g_surface.rendered_interactive = interactive;
        g_surface.pixels_valid = true;
    }
    if (!g_surface.texture_valid || g_surface.stats.rendered_instances == 0) return false;
    destination = projector->viewport;
    vk_renderer_draw_texture(vk, &g_surface.texture, NULL, &destination);
    if (out_stats) *out_stats = g_surface.stats;
    return true;
}

void SceneEditorMeshPreviewSurfaceReset(SDL_Renderer* renderer) {
    VkRenderer* vk = (VkRenderer*)renderer;
    if (g_surface.texture_valid && vk) {
        vk_renderer_wait_idle(vk);
        vk_renderer_texture_destroy(vk, &g_surface.texture);
    }
    free(g_surface.rgba);
    free(g_surface.depth);
    free(g_surface.owner);
    memset(&g_surface, 0, sizeof(g_surface));
}

#include "app/preview_retained_scene_mesh.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "config/config_manager.h"
#include "import/runtime_mesh_asset_loader.h"

#define PREVIEW_RETAINED_SCENE_MAX_SILHOUETTE_EDGE_REFS 65536u
#define PREVIEW_RETAINED_SCENE_MAX_SILHOUETTE_TRIANGLES 32768u

typedef struct PreviewRetainedSceneMeshEdgeRef {
    size_t a;
    size_t b;
    bool front_facing;
    double ax;
    double ay;
    double az;
    double bx;
    double by;
    double bz;
    SDL_Color color;
} PreviewRetainedSceneMeshEdgeRef;

bool PreviewRetainedSceneMeshShouldBuildSilhouetteForTriangleCount(size_t triangle_count) {
    return triangle_count > 0u &&
           triangle_count <= PREVIEW_RETAINED_SCENE_MAX_SILHOUETTE_TRIANGLES;
}

static PreviewRetainedSceneLineSegment preview_retained_scene_mesh_make_segment(double ax,
                                                                                double ay,
                                                                                double az,
                                                                                double bx,
                                                                                double by,
                                                                                double bz,
                                                                                SDL_Color color) {
    PreviewRetainedSceneLineSegment segment = {0};
    segment.ax = ax;
    segment.ay = ay;
    segment.az = az;
    segment.bx = bx;
    segment.by = by;
    segment.bz = bz;
    segment.color = color;
    return segment;
}

static SDL_Color preview_retained_scene_mesh_primitive_color(int primitive_index) {
    static const SDL_Color k_palette[] = {
        {240, 240, 240, 255},
        {255, 208, 120, 255},
        {120, 198, 255, 255},
        {144, 232, 180, 255}
    };
    const int palette_count = (int)(sizeof(k_palette) / sizeof(k_palette[0]));
    int index = primitive_index;
    if (index >= 0 && index < sceneSettings.objectCount) {
        SceneObject* obj = &sceneSettings.sceneObjects[index];
        return (SDL_Color){
            SceneObjectColorR(obj),
            SceneObjectColorG(obj),
            SceneObjectColorB(obj),
            SceneObjectAlphaByte(obj)
        };
    }
    if (index < 0) index = 0;
    return k_palette[index % palette_count];
}

static SDL_Color preview_retained_scene_mesh_silhouette_color(SDL_Color base) {
    int r = (int)base.r + 96;
    int g = (int)base.g + 96;
    int b = (int)base.b + 96;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return (SDL_Color){(Uint8)r, (Uint8)g, (Uint8)b, 255};
}

static SDL_Color preview_retained_scene_mesh_preview_color(int instance_index) {
    static const SDL_Color k_mesh_palette[] = {
        {255, 170, 82, 255},
        {96, 214, 255, 255},
        {176, 232, 120, 255},
        {236, 132, 255, 255}
    };
    const int palette_count = (int)(sizeof(k_mesh_palette) / sizeof(k_mesh_palette[0]));
    if (instance_index < 0) instance_index = 0;
    return k_mesh_palette[instance_index % palette_count];
}

static void preview_retained_scene_mesh_append_segment(
    PreviewRetainedSceneLineSegment* segments,
    int max_segments,
    int* io_count,
    double ax,
    double ay,
    double az,
    double bx,
    double by,
    double bz,
    SDL_Color color) {
    PreviewRetainedSceneLineSegment* segment = NULL;
    if (!segments || !io_count || *io_count < 0) return;
    if (*io_count >= max_segments) return;
    segment = &segments[*io_count];
    *segment = preview_retained_scene_mesh_make_segment(ax, ay, az, bx, by, bz, color);
    *io_count += 1;
}

static void preview_retained_scene_mesh_accumulate_extents(double x,
                                                           double y,
                                                           double z,
                                                           bool* seeded,
                                                           double* min_x,
                                                           double* min_y,
                                                           double* min_z,
                                                           double* max_x,
                                                           double* max_y,
                                                           double* max_z) {
    if (!seeded || !min_x || !min_y || !min_z || !max_x || !max_y || !max_z) return;
    if (!*seeded) {
        *min_x = *max_x = x;
        *min_y = *max_y = y;
        *min_z = *max_z = z;
        *seeded = true;
        return;
    }
    if (x < *min_x) *min_x = x;
    if (x > *max_x) *max_x = x;
    if (y < *min_y) *min_y = y;
    if (y > *max_y) *max_y = y;
    if (z < *min_z) *min_z = z;
    if (z > *max_z) *max_z = z;
}

static void preview_retained_scene_mesh_append_bounds(
    PreviewRetainedSceneLineSegment* segments,
    int max_segments,
    int* io_count,
    double min_x,
    double min_y,
    double min_z,
    double max_x,
    double max_y,
    double max_z,
    SDL_Color color) {
    static const int k_edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };
    double corners[8][3];
    if (!segments || !io_count) return;

    corners[0][0] = min_x; corners[0][1] = min_y; corners[0][2] = min_z;
    corners[1][0] = max_x; corners[1][1] = min_y; corners[1][2] = min_z;
    corners[2][0] = max_x; corners[2][1] = max_y; corners[2][2] = min_z;
    corners[3][0] = min_x; corners[3][1] = max_y; corners[3][2] = min_z;
    corners[4][0] = min_x; corners[4][1] = min_y; corners[4][2] = max_z;
    corners[5][0] = max_x; corners[5][1] = min_y; corners[5][2] = max_z;
    corners[6][0] = max_x; corners[6][1] = max_y; corners[6][2] = max_z;
    corners[7][0] = min_x; corners[7][1] = max_y; corners[7][2] = max_z;

    for (int i = 0; i < 12; ++i) {
        int a = k_edges[i][0];
        int b = k_edges[i][1];
        preview_retained_scene_mesh_append_segment(segments,
                                                   max_segments,
                                                   io_count,
                                                   corners[a][0],
                                                   corners[a][1],
                                                   corners[a][2],
                                                   corners[b][0],
                                                   corners[b][1],
                                                   corners[b][2],
                                                   color);
    }
}

static void preview_retained_scene_mesh_rotate_point(
    double* io_x,
    double* io_y,
    double* io_z,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    double x = io_x ? *io_x : 0.0;
    double y = io_y ? *io_y : 0.0;
    double z = io_z ? *io_z : 0.0;
    double cx = 0.0;
    double sx = 0.0;
    double cy = 0.0;
    double sy = 0.0;
    double cz = 0.0;
    double sz = 0.0;
    double ty = 0.0;
    double tz = 0.0;
    double tx = 0.0;

    if (!io_x || !io_y || !io_z || !instance) return;
    cx = cos(instance->rotation_x);
    sx = sin(instance->rotation_x);
    cy = cos(instance->rotation_y);
    sy = sin(instance->rotation_y);
    cz = cos(instance->rotation_z);
    sz = sin(instance->rotation_z);

    ty = y * cx - z * sx;
    tz = y * sx + z * cx;
    y = ty;
    z = tz;

    tx = x * cy + z * sy;
    tz = -x * sy + z * cy;
    x = tx;
    z = tz;

    tx = x * cz - y * sz;
    ty = x * sz + y * cz;
    x = tx;
    y = ty;

    *io_x = x;
    *io_y = y;
    *io_z = z;
}

static void preview_retained_scene_mesh_transform_vertex(
    const CoreMeshAssetRuntimeVertex* vertex,
    const RayTracingRuntimeMeshAssetInstance* instance,
    double* out_x,
    double* out_y,
    double* out_z) {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    if (out_x) *out_x = 0.0;
    if (out_y) *out_y = 0.0;
    if (out_z) *out_z = 0.0;
    if (!vertex || !instance) return;

    x = vertex->position.x * instance->scale_x;
    y = vertex->position.y * instance->scale_y;
    z = vertex->position.z * instance->scale_z;
    preview_retained_scene_mesh_rotate_point(&x, &y, &z, instance);
    x += instance->position_x;
    y += instance->position_y;
    z += instance->position_z;

    if (out_x) *out_x = x;
    if (out_y) *out_y = y;
    if (out_z) *out_z = z;
}

static double preview_retained_scene_mesh_dot3(double ax,
                                               double ay,
                                               double az,
                                               double bx,
                                               double by,
                                               double bz) {
    return ax * bx + ay * by + az * bz;
}

static void preview_retained_scene_mesh_cross3(double ax,
                                               double ay,
                                               double az,
                                               double bx,
                                               double by,
                                               double bz,
                                               double* out_x,
                                               double* out_y,
                                               double* out_z) {
    if (out_x) *out_x = ay * bz - az * by;
    if (out_y) *out_y = az * bx - ax * bz;
    if (out_z) *out_z = ax * by - ay * bx;
}

static int preview_retained_scene_mesh_compare_edge_refs(const void* a, const void* b) {
    const PreviewRetainedSceneMeshEdgeRef* ea = (const PreviewRetainedSceneMeshEdgeRef*)a;
    const PreviewRetainedSceneMeshEdgeRef* eb = (const PreviewRetainedSceneMeshEdgeRef*)b;
    if (ea->a < eb->a) return -1;
    if (ea->a > eb->a) return 1;
    if (ea->b < eb->b) return -1;
    if (ea->b > eb->b) return 1;
    return 0;
}

static void preview_retained_scene_mesh_add_edge_ref(
    PreviewRetainedSceneMeshEdgeRef* refs,
    size_t max_refs,
    size_t* io_count,
    size_t a,
    size_t b,
    bool front_facing,
    double ax,
    double ay,
    double az,
    double bx,
    double by,
    double bz,
    SDL_Color color) {
    PreviewRetainedSceneMeshEdgeRef* ref = NULL;
    if (!refs || !io_count || *io_count >= max_refs) return;
    ref = &refs[*io_count];
    if (a <= b) {
        ref->a = a;
        ref->b = b;
        ref->ax = ax;
        ref->ay = ay;
        ref->az = az;
        ref->bx = bx;
        ref->by = by;
        ref->bz = bz;
    } else {
        ref->a = b;
        ref->b = a;
        ref->ax = bx;
        ref->ay = by;
        ref->az = bz;
        ref->bx = ax;
        ref->by = ay;
        ref->bz = az;
    }
    ref->front_facing = front_facing;
    ref->color = color;
    *io_count += 1u;
}

bool PreviewRetainedSceneMeshResolveExtents(bool* seeded,
                                            double* min_x,
                                            double* min_y,
                                            double* min_z,
                                            double* max_x,
                                            double* max_y,
                                            double* max_z) {
    const RayTracingRuntimeMeshAssetSet* mesh_assets = ray_tracing_runtime_mesh_assets_last();
    bool any = false;
    if (!mesh_assets || !seeded || !min_x || !min_y || !min_z || !max_x || !max_y || !max_z) {
        return false;
    }
    for (int i = 0; i < mesh_assets->instance_count; ++i) {
        const RayTracingRuntimeMeshAssetInstance* instance = &mesh_assets->instances[i];
        const CoreMeshAssetRuntimeDocument* document = NULL;
        if (instance->asset_index < 0 || instance->asset_index >= mesh_assets->asset_count) {
            continue;
        }
        document = &mesh_assets->assets[instance->asset_index].document;
        for (size_t j = 0; j < document->vertex_count; ++j) {
            double x = 0.0;
            double y = 0.0;
            double z = 0.0;
            preview_retained_scene_mesh_transform_vertex(&document->vertices[j],
                                                         instance,
                                                         &x,
                                                         &y,
                                                         &z);
            preview_retained_scene_mesh_accumulate_extents(x,
                                                           y,
                                                           z,
                                                           seeded,
                                                           min_x,
                                                           min_y,
                                                           min_z,
                                                           max_x,
                                                           max_y,
                                                           max_z);
            any = true;
        }
    }
    return any;
}

void PreviewRetainedSceneMeshAppendEdges(PreviewRetainedSceneLineSegment* segments,
                                         int max_segments,
                                         int* io_count) {
    const RayTracingRuntimeMeshAssetSet* mesh_assets = ray_tracing_runtime_mesh_assets_last();
    if (!segments || !io_count || !mesh_assets || max_segments <= 0) return;
    for (int i = 0; i < mesh_assets->instance_count && *io_count < max_segments; ++i) {
        const RayTracingRuntimeMeshAssetInstance* instance = &mesh_assets->instances[i];
        const CoreMeshAssetRuntimeDocument* document = NULL;
        SDL_Color color = preview_retained_scene_mesh_preview_color(i);
        bool has_bounds = false;
        double min_x = 0.0;
        double min_y = 0.0;
        double min_z = 0.0;
        double max_x = 0.0;
        double max_y = 0.0;
        double max_z = 0.0;
        int remaining_segments = max_segments - *io_count;
        size_t triangle_stride = 1u;
        size_t max_triangles_to_draw = 0u;

        if (instance->asset_index < 0 || instance->asset_index >= mesh_assets->asset_count) {
            continue;
        }
        document = &mesh_assets->assets[instance->asset_index].document;
        if (!document->vertices || !document->triangles || document->triangle_count == 0u) {
            continue;
        }
        max_triangles_to_draw = (size_t)(remaining_segments / 3);
        if (max_triangles_to_draw == 0u) return;
        if (document->triangle_count > max_triangles_to_draw) {
            triangle_stride = (document->triangle_count + max_triangles_to_draw - 1u) /
                              max_triangles_to_draw;
            if (triangle_stride == 0u) triangle_stride = 1u;
        }

        for (size_t j = 0; j < document->triangle_count && *io_count + 3 <= max_segments;
             j += triangle_stride) {
            const CoreMeshAssetRuntimeTriangle* tri = &document->triangles[j];
            double ax = 0.0;
            double ay = 0.0;
            double az = 0.0;
            double bx = 0.0;
            double by = 0.0;
            double bz = 0.0;
            double cx = 0.0;
            double cy = 0.0;
            double cz = 0.0;
            if (tri->a >= document->vertex_count || tri->b >= document->vertex_count ||
                tri->c >= document->vertex_count) {
                continue;
            }
            preview_retained_scene_mesh_transform_vertex(&document->vertices[tri->a],
                                                         instance,
                                                         &ax,
                                                         &ay,
                                                         &az);
            preview_retained_scene_mesh_transform_vertex(&document->vertices[tri->b],
                                                         instance,
                                                         &bx,
                                                         &by,
                                                         &bz);
            preview_retained_scene_mesh_transform_vertex(&document->vertices[tri->c],
                                                         instance,
                                                         &cx,
                                                         &cy,
                                                         &cz);
            preview_retained_scene_mesh_accumulate_extents(ax,
                                                           ay,
                                                           az,
                                                           &has_bounds,
                                                           &min_x,
                                                           &min_y,
                                                           &min_z,
                                                           &max_x,
                                                           &max_y,
                                                           &max_z);
            preview_retained_scene_mesh_accumulate_extents(bx,
                                                           by,
                                                           bz,
                                                           &has_bounds,
                                                           &min_x,
                                                           &min_y,
                                                           &min_z,
                                                           &max_x,
                                                           &max_y,
                                                           &max_z);
            preview_retained_scene_mesh_accumulate_extents(cx,
                                                           cy,
                                                           cz,
                                                           &has_bounds,
                                                           &min_x,
                                                           &min_y,
                                                           &min_z,
                                                           &max_x,
                                                           &max_y,
                                                           &max_z);
            segments[*io_count] =
                preview_retained_scene_mesh_make_segment(ax, ay, az, bx, by, bz, color);
            *io_count += 1;
            segments[*io_count] =
                preview_retained_scene_mesh_make_segment(bx, by, bz, cx, cy, cz, color);
            *io_count += 1;
            segments[*io_count] =
                preview_retained_scene_mesh_make_segment(cx, cy, cz, ax, ay, az, color);
            *io_count += 1;
        }
        if (has_bounds) {
            preview_retained_scene_mesh_append_bounds(segments,
                                                      max_segments,
                                                      io_count,
                                                      min_x,
                                                      min_y,
                                                      min_z,
                                                      max_x,
                                                      max_y,
                                                      max_z,
                                                      color);
        }
    }
}

int PreviewRetainedSceneBuildSilhouetteSegments(const RuntimeSceneBridge3DDigestState* digest,
                                                const PreviewCameraProjector* projector,
                                                PreviewRetainedSceneLineSegment* out_segments,
                                                int max_segments) {
    const RayTracingRuntimeMeshAssetSet* mesh_assets = ray_tracing_runtime_mesh_assets_last();
    int count = 0;

    if (out_segments && max_segments > 0) {
        memset(out_segments, 0, sizeof(*out_segments) * (size_t)max_segments);
    }
    if (!digest || !digest->valid || !projector || !out_segments || max_segments <= 0) return 0;
    if (!mesh_assets || mesh_assets->instance_count <= 0) return 0;

    for (int i = 0; i < mesh_assets->instance_count && count < max_segments; ++i) {
        const RayTracingRuntimeMeshAssetInstance* instance = &mesh_assets->instances[i];
        const CoreMeshAssetRuntimeDocument* document = NULL;
        PreviewRetainedSceneMeshEdgeRef* edge_refs = NULL;
        size_t max_refs = 0u;
        size_t edge_ref_count = 0u;
        size_t max_triangles_to_scan = 0u;
        size_t triangle_stride = 1u;

        if (instance->asset_index < 0 || instance->asset_index >= mesh_assets->asset_count) {
            continue;
        }
        document = &mesh_assets->assets[instance->asset_index].document;
        if (!document->vertices || !document->triangles || document->triangle_count == 0u) {
            continue;
        }
        if (!PreviewRetainedSceneMeshShouldBuildSilhouetteForTriangleCount(
                document->triangle_count)) {
            continue;
        }

        max_refs = document->triangle_count * 3u;
        if (max_refs > PREVIEW_RETAINED_SCENE_MAX_SILHOUETTE_EDGE_REFS) {
            max_refs = PREVIEW_RETAINED_SCENE_MAX_SILHOUETTE_EDGE_REFS;
        }
        if (max_refs < 3u) continue;
        max_triangles_to_scan = max_refs / 3u;
        if (document->triangle_count > max_triangles_to_scan) {
            triangle_stride = (document->triangle_count + max_triangles_to_scan - 1u) /
                              max_triangles_to_scan;
            if (triangle_stride == 0u) triangle_stride = 1u;
        }

        edge_refs = (PreviewRetainedSceneMeshEdgeRef*)calloc(max_refs, sizeof(*edge_refs));
        if (!edge_refs) {
            continue;
        }

        for (size_t j = 0; j < document->triangle_count && edge_ref_count + 3u <= max_refs;
             j += triangle_stride) {
            const CoreMeshAssetRuntimeTriangle* tri = &document->triangles[j];
            SDL_Color color =
                preview_retained_scene_mesh_silhouette_color(
                    preview_retained_scene_mesh_primitive_color(instance->scene_object_index));
            double ax = 0.0;
            double ay = 0.0;
            double az = 0.0;
            double bx = 0.0;
            double by = 0.0;
            double bz = 0.0;
            double cx = 0.0;
            double cy = 0.0;
            double cz = 0.0;
            double ux = 0.0;
            double uy = 0.0;
            double uz = 0.0;
            double vx = 0.0;
            double vy = 0.0;
            double vz = 0.0;
            double nx = 0.0;
            double ny = 0.0;
            double nz = 0.0;
            double center_x = 0.0;
            double center_y = 0.0;
            double center_z = 0.0;
            double normal_len_sq = 0.0;
            bool front_facing = false;

            if (tri->a >= document->vertex_count || tri->b >= document->vertex_count ||
                tri->c >= document->vertex_count) {
                continue;
            }

            preview_retained_scene_mesh_transform_vertex(&document->vertices[tri->a],
                                                         instance,
                                                         &ax,
                                                         &ay,
                                                         &az);
            preview_retained_scene_mesh_transform_vertex(&document->vertices[tri->b],
                                                         instance,
                                                         &bx,
                                                         &by,
                                                         &bz);
            preview_retained_scene_mesh_transform_vertex(&document->vertices[tri->c],
                                                         instance,
                                                         &cx,
                                                         &cy,
                                                         &cz);

            ux = bx - ax;
            uy = by - ay;
            uz = bz - az;
            vx = cx - ax;
            vy = cy - ay;
            vz = cz - az;
            preview_retained_scene_mesh_cross3(ux, uy, uz, vx, vy, vz, &nx, &ny, &nz);
            normal_len_sq = preview_retained_scene_mesh_dot3(nx, ny, nz, nx, ny, nz);
            if (normal_len_sq <= 1e-16) {
                continue;
            }

            center_x = (ax + bx + cx) / 3.0;
            center_y = (ay + by + cy) / 3.0;
            center_z = (az + bz + cz) / 3.0;
            front_facing =
                preview_retained_scene_mesh_dot3(nx,
                                                 ny,
                                                 nz,
                                                 projector->origin_x - center_x,
                                                 projector->origin_y - center_y,
                                                 projector->origin_z - center_z) >= 0.0;

            preview_retained_scene_mesh_add_edge_ref(edge_refs,
                                                     max_refs,
                                                     &edge_ref_count,
                                                     tri->a,
                                                     tri->b,
                                                     front_facing,
                                                     ax,
                                                     ay,
                                                     az,
                                                     bx,
                                                     by,
                                                     bz,
                                                     color);
            preview_retained_scene_mesh_add_edge_ref(edge_refs,
                                                     max_refs,
                                                     &edge_ref_count,
                                                     tri->b,
                                                     tri->c,
                                                     front_facing,
                                                     bx,
                                                     by,
                                                     bz,
                                                     cx,
                                                     cy,
                                                     cz,
                                                     color);
            preview_retained_scene_mesh_add_edge_ref(edge_refs,
                                                     max_refs,
                                                     &edge_ref_count,
                                                     tri->c,
                                                     tri->a,
                                                     front_facing,
                                                     cx,
                                                     cy,
                                                     cz,
                                                     ax,
                                                     ay,
                                                     az,
                                                     color);
        }

        qsort(edge_refs,
              edge_ref_count,
              sizeof(*edge_refs),
              preview_retained_scene_mesh_compare_edge_refs);

        for (size_t start = 0u; start < edge_ref_count && count < max_segments;) {
            size_t end = start + 1u;
            bool saw_front = edge_refs[start].front_facing;
            bool saw_back = !edge_refs[start].front_facing;
            while (end < edge_ref_count && edge_refs[end].a == edge_refs[start].a &&
                   edge_refs[end].b == edge_refs[start].b) {
                if (edge_refs[end].front_facing) {
                    saw_front = true;
                } else {
                    saw_back = true;
                }
                end += 1u;
            }
            if ((end - start) == 1u || (saw_front && saw_back)) {
                preview_retained_scene_mesh_append_segment(out_segments,
                                                           max_segments,
                                                           &count,
                                                           edge_refs[start].ax,
                                                           edge_refs[start].ay,
                                                           edge_refs[start].az,
                                                           edge_refs[start].bx,
                                                           edge_refs[start].by,
                                                           edge_refs[start].bz,
                                                           edge_refs[start].color);
            }
            start = end;
        }

        free(edge_refs);
    }

    return count;
}

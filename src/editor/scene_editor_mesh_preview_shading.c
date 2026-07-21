#include "editor/scene_editor_mesh_preview_shading.h"

#include <math.h>
#include <string.h>

static double scene_editor_mesh_preview_clamp_dot(double value) {
    if (value < -1.0) return -1.0;
    if (value > 1.0) return 1.0;
    return value;
}

static bool scene_editor_mesh_preview_accumulate_corner(
    CoreObjectVec3* destination,
    CoreObjectVec3 face_normal,
    CoreObjectVec3 edge_a,
    CoreObjectVec3 edge_b) {
    const double edge_a_length = sqrt(edge_a.x * edge_a.x +
                                      edge_a.y * edge_a.y +
                                      edge_a.z * edge_a.z);
    const double edge_b_length = sqrt(edge_b.x * edge_b.x +
                                      edge_b.y * edge_b.y +
                                      edge_b.z * edge_b.z);
    double angle = 0.0;
    if (!destination || !(edge_a_length > 1e-12) || !(edge_b_length > 1e-12) ||
        !isfinite(edge_a_length) || !isfinite(edge_b_length)) {
        return false;
    }
    angle = acos(scene_editor_mesh_preview_clamp_dot(
        (edge_a.x * edge_b.x + edge_a.y * edge_b.y + edge_a.z * edge_b.z) /
        (edge_a_length * edge_b_length)));
    if (!isfinite(angle)) return false;
    destination->x += face_normal.x * angle;
    destination->y += face_normal.y * angle;
    destination->z += face_normal.z * angle;
    return true;
}

bool SceneEditorMeshPreviewBuildSmoothNormals(
    const CoreMeshPreviewLodMesh* lod,
    CoreObjectVec3* out_normals,
    size_t normal_capacity) {
    if (!lod || !out_normals || !lod->vertices || !lod->indices ||
        lod->vertex_count == 0u || lod->triangle_count == 0u ||
        normal_capacity < lod->vertex_count) {
        return false;
    }
    memset(out_normals, 0, lod->vertex_count * sizeof(*out_normals));
    for (size_t triangle = 0u; triangle < lod->triangle_count; ++triangle) {
        const uint32_t ia = lod->indices[triangle * 3u + 0u];
        const uint32_t ib = lod->indices[triangle * 3u + 1u];
        const uint32_t ic = lod->indices[triangle * 3u + 2u];
        CoreObjectVec3 ab;
        CoreObjectVec3 ac;
        CoreObjectVec3 ba;
        CoreObjectVec3 bc;
        CoreObjectVec3 ca;
        CoreObjectVec3 cb;
        CoreObjectVec3 normal;
        double length = 0.0;
        if (ia >= lod->vertex_count || ib >= lod->vertex_count ||
            ic >= lod->vertex_count) {
            return false;
        }
        ab = (CoreObjectVec3){lod->vertices[ib].x - lod->vertices[ia].x,
                              lod->vertices[ib].y - lod->vertices[ia].y,
                              lod->vertices[ib].z - lod->vertices[ia].z};
        ac = (CoreObjectVec3){lod->vertices[ic].x - lod->vertices[ia].x,
                              lod->vertices[ic].y - lod->vertices[ia].y,
                              lod->vertices[ic].z - lod->vertices[ia].z};
        normal = (CoreObjectVec3){ab.y * ac.z - ab.z * ac.y,
                                  ab.z * ac.x - ab.x * ac.z,
                                  ab.x * ac.y - ab.y * ac.x};
        length = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (!(length > 1e-12) || !isfinite(length)) continue;
        normal.x /= length;
        normal.y /= length;
        normal.z /= length;
        ba = (CoreObjectVec3){-ab.x, -ab.y, -ab.z};
        bc = (CoreObjectVec3){lod->vertices[ic].x - lod->vertices[ib].x,
                              lod->vertices[ic].y - lod->vertices[ib].y,
                              lod->vertices[ic].z - lod->vertices[ib].z};
        ca = (CoreObjectVec3){-ac.x, -ac.y, -ac.z};
        cb = (CoreObjectVec3){-bc.x, -bc.y, -bc.z};
        (void)scene_editor_mesh_preview_accumulate_corner(&out_normals[ia], normal, ab, ac);
        (void)scene_editor_mesh_preview_accumulate_corner(&out_normals[ib], normal, ba, bc);
        (void)scene_editor_mesh_preview_accumulate_corner(&out_normals[ic], normal, ca, cb);
    }
    for (size_t i = 0u; i < lod->vertex_count; ++i) {
        const double length = sqrt(out_normals[i].x * out_normals[i].x +
                                   out_normals[i].y * out_normals[i].y +
                                   out_normals[i].z * out_normals[i].z);
        if (!(length > 1e-12) || !isfinite(length)) return false;
        out_normals[i].x /= length;
        out_normals[i].y /= length;
        out_normals[i].z /= length;
    }
    return true;
}

static Uint8 scene_editor_mesh_preview_shade_channel(double value) {
    if (!isfinite(value) || value <= 0.0) return 0u;
    if (value >= 255.0) return 255u;
    return (Uint8)lround(value);
}

double SceneEditorMeshPreviewShadeFactor(SceneEditorMeshPreviewShadeNormal normal) {
    static const SceneEditorMeshPreviewShadeNormal light = {
        0.3836486121626103,
        -0.4240337281797272,
        -0.8204351898687576
    };
    const double length = sqrt(normal.x * normal.x +
                               normal.y * normal.y +
                               normal.z * normal.z);
    double facing = 0.0;
    if (!(length > 1e-12) || !isfinite(length)) return 0.36;
    normal.x /= length;
    normal.y /= length;
    normal.z /= length;
    facing = fabs(normal.x * light.x + normal.y * light.y + normal.z * light.z);
    if (!isfinite(facing)) return 0.36;
    if (facing > 1.0) facing = 1.0;
    return 0.36 + 0.64 * facing;
}

SDL_Color SceneEditorMeshPreviewShadeColor(SDL_Color base,
                                           SceneEditorMeshPreviewShadeNormal normal) {
    const double factor = SceneEditorMeshPreviewShadeFactor(normal);
    return (SDL_Color){scene_editor_mesh_preview_shade_channel((double)base.r * factor),
                       scene_editor_mesh_preview_shade_channel((double)base.g * factor),
                       scene_editor_mesh_preview_shade_channel((double)base.b * factor),
                       base.a};
}

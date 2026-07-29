#include "app/preview_mesh_instance_bounds.h"

#include <math.h>
#include <string.h>

static bool preview_mesh_instance_bounds_finite(
    const CoreMeshAssetBounds3* bounds,
    const RayTracingRuntimeMeshAssetInstance* instance) {
    return bounds && instance &&
           isfinite(bounds->min.x) && isfinite(bounds->min.y) &&
           isfinite(bounds->min.z) && isfinite(bounds->max.x) &&
           isfinite(bounds->max.y) && isfinite(bounds->max.z) &&
           bounds->min.x <= bounds->max.x &&
           bounds->min.y <= bounds->max.y &&
           bounds->min.z <= bounds->max.z &&
           isfinite(instance->position_x) && isfinite(instance->position_y) &&
           isfinite(instance->position_z) && isfinite(instance->rotation_x) &&
           isfinite(instance->rotation_y) && isfinite(instance->rotation_z) &&
           isfinite(instance->scale_x) && isfinite(instance->scale_y) &&
           isfinite(instance->scale_z) &&
           isfinite(instance->rotation_pivot_x) &&
           isfinite(instance->rotation_pivot_y) &&
           isfinite(instance->rotation_pivot_z);
}

static PreviewMeshInstancePoint3 preview_mesh_instance_rotate(
    PreviewMeshInstancePoint3 point,
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
    {
        const double rotated_x = point.x * cz - point.y * sz;
        const double rotated_y = point.x * sz + point.y * cz;
        point.x = rotated_x;
        point.y = rotated_y;
    }
    return point;
}

bool PreviewMeshInstanceTransformPoint(
    CoreObjectVec3 local,
    const CoreMeshAssetBounds3* local_bounds,
    const RayTracingRuntimeMeshAssetInstance* instance,
    PreviewMeshInstancePoint3* out_point) {
    PreviewMeshInstancePoint3 pivot = {0.0, 0.0, 0.0};
    PreviewMeshInstancePoint3 point = {0.0, 0.0, 0.0};
    if (out_point) memset(out_point, 0, sizeof(*out_point));
    if (!out_point ||
        !preview_mesh_instance_bounds_finite(local_bounds, instance) ||
        !isfinite(local.x) || !isfinite(local.y) || !isfinite(local.z)) {
        return false;
    }
    if (instance->rotation_pivot_policy ==
        RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_CUSTOM) {
        pivot = (PreviewMeshInstancePoint3){
            instance->rotation_pivot_x * instance->scale_x,
            instance->rotation_pivot_y * instance->scale_y,
            instance->rotation_pivot_z * instance->scale_z};
    } else if (instance->rotation_pivot_policy ==
               RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_BOUNDS_CENTER) {
        pivot = (PreviewMeshInstancePoint3){
            (local_bounds->min.x + local_bounds->max.x) *
                0.5 * instance->scale_x,
            (local_bounds->min.y + local_bounds->max.y) *
                0.5 * instance->scale_y,
            (local_bounds->min.z + local_bounds->max.z) *
                0.5 * instance->scale_z};
    }
    point = (PreviewMeshInstancePoint3){
        local.x * instance->scale_x - pivot.x,
        local.y * instance->scale_y - pivot.y,
        local.z * instance->scale_z - pivot.z};
    point = preview_mesh_instance_rotate(point, instance);
    point.x += pivot.x + instance->position_x;
    point.y += pivot.y + instance->position_y;
    point.z += pivot.z + instance->position_z;
    if (!isfinite(point.x) || !isfinite(point.y) || !isfinite(point.z)) {
        return false;
    }
    *out_point = point;
    return true;
}

bool PreviewMeshInstanceBuildBoundsCorners(
    const CoreMeshAssetBounds3* local_bounds,
    const RayTracingRuntimeMeshAssetInstance* instance,
    PreviewMeshInstancePoint3 out_corners[8]) {
    CoreObjectVec3 local[8];
    if (out_corners) memset(out_corners, 0, sizeof(*out_corners) * 8u);
    if (!out_corners ||
        !preview_mesh_instance_bounds_finite(local_bounds, instance)) {
        return false;
    }
    local[0] = (CoreObjectVec3){
        local_bounds->min.x, local_bounds->min.y, local_bounds->min.z};
    local[1] = (CoreObjectVec3){
        local_bounds->max.x, local_bounds->min.y, local_bounds->min.z};
    local[2] = (CoreObjectVec3){
        local_bounds->max.x, local_bounds->max.y, local_bounds->min.z};
    local[3] = (CoreObjectVec3){
        local_bounds->min.x, local_bounds->max.y, local_bounds->min.z};
    local[4] = (CoreObjectVec3){
        local_bounds->min.x, local_bounds->min.y, local_bounds->max.z};
    local[5] = (CoreObjectVec3){
        local_bounds->max.x, local_bounds->min.y, local_bounds->max.z};
    local[6] = (CoreObjectVec3){
        local_bounds->max.x, local_bounds->max.y, local_bounds->max.z};
    local[7] = (CoreObjectVec3){
        local_bounds->min.x, local_bounds->max.y, local_bounds->max.z};
    for (int i = 0; i < 8; ++i) {
        if (!PreviewMeshInstanceTransformPoint(
                local[i], local_bounds, instance, &out_corners[i])) {
            memset(out_corners, 0, sizeof(*out_corners) * 8u);
            return false;
        }
    }
    return true;
}

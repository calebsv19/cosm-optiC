#include "render/runtime_scene_curve_3d.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool runtime_scene_curve_finite_vec3(Vec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static Vec3 runtime_scene_curve_min(Vec3 a, Vec3 b) {
    return vec3(fmin(a.x, b.x), fmin(a.y, b.y), fmin(a.z, b.z));
}

static Vec3 runtime_scene_curve_max(Vec3 a, Vec3 b) {
    return vec3(fmax(a.x, b.x), fmax(a.y, b.y), fmax(a.z, b.z));
}

static Vec3 runtime_scene_curve_rotate(Vec3 p, Vec3 rotation) {
    const double cx = cos(rotation.x);
    const double sx = sin(rotation.x);
    const double cy = cos(rotation.y);
    const double sy = sin(rotation.y);
    const double cz = cos(rotation.z);
    const double sz = sin(rotation.z);
    Vec3 q = p;
    Vec3 r = q;
    q.y = r.y * cx - r.z * sx;
    q.z = r.y * sx + r.z * cx;
    r = q;
    q.x = r.x * cy + r.z * sy;
    q.z = -r.x * sy + r.z * cy;
    r = q;
    q.x = r.x * cz - r.y * sz;
    q.y = r.x * sz + r.y * cz;
    return q;
}

static Vec3 runtime_scene_curve_inverse_rotate(Vec3 p, Vec3 rotation) {
    const Vec3 inverse = vec3(-rotation.x, -rotation.y, -rotation.z);
    const double cx = cos(inverse.x);
    const double sx = sin(inverse.x);
    const double cy = cos(inverse.y);
    const double sy = sin(inverse.y);
    const double cz = cos(inverse.z);
    const double sz = sin(inverse.z);
    Vec3 q = p;
    Vec3 r = q;
    q.x = r.x * cz - r.y * sz;
    q.y = r.x * sz + r.y * cz;
    r = q;
    q.x = r.x * cy + r.z * sy;
    q.z = -r.x * sy + r.z * cy;
    r = q;
    q.y = r.y * cx - r.z * sx;
    q.z = r.y * sx + r.z * cx;
    return q;
}

static Vec3 runtime_scene_curve_transform_point(
    const RuntimeCurveSceneInstance3D* instance,
    Vec3 local) {
    return vec3_add(
        runtime_scene_curve_rotate(
            vec3_scale(local, instance->uniformScale), instance->rotation),
        instance->position);
}

static Vec3 runtime_scene_curve_inverse_point(
    const RuntimeCurveSceneInstance3D* instance,
    Vec3 world) {
    return vec3_scale(
        runtime_scene_curve_inverse_rotate(
            vec3_sub(world, instance->position), instance->rotation),
        1.0 / instance->uniformScale);
}

void RuntimeScene3D_ClearCurveInstances(RuntimeScene3D* scene) {
    if (!scene) return;
    for (int i = 0; i < scene->curveInstanceCount; ++i) {
        RuntimeCurveAsset3D_Free(&scene->curveInstances[i].asset);
    }
    free(scene->curveInstances);
    scene->curveInstances = NULL;
    scene->curveInstanceCount = 0;
    scene->curveInstanceCapacity = 0;
}

bool RuntimeScene3D_AddCurveInstance(
    RuntimeScene3D* scene,
    const RuntimeCurveAsset3D* asset,
    const RuntimeCurveSceneInstance3DDescriptor* descriptor) {
    RuntimeCurveSceneInstance3D* instances = NULL;
    RuntimeCurveSceneInstance3D* instance = NULL;
    int next_capacity = 0;
    if (!scene || !asset || !descriptor || !descriptor->assetId ||
        !descriptor->objectId || descriptor->assetId[0] == '\0' ||
        descriptor->objectId[0] == '\0' ||
        strnlen(descriptor->assetId, RUNTIME_CURVE_SCENE_3D_MAX_ASSET_ID) >=
            RUNTIME_CURVE_SCENE_3D_MAX_ASSET_ID ||
        strnlen(descriptor->objectId, RUNTIME_SCENE_3D_MAX_OBJECT_ID) >=
            RUNTIME_SCENE_3D_MAX_OBJECT_ID ||
        descriptor->sceneObjectIndex < 0 ||
        !runtime_scene_curve_finite_vec3(descriptor->position) ||
        !runtime_scene_curve_finite_vec3(descriptor->rotation) ||
        !isfinite(descriptor->uniformScale) ||
        !(descriptor->uniformScale > 0.0) ||
        !RuntimeCurveAsset3D_HasReadyBLAS(asset)) {
        return false;
    }
    if (scene->curveInstanceCount >= scene->curveInstanceCapacity) {
        next_capacity =
            scene->curveInstanceCapacity > 0 ? scene->curveInstanceCapacity * 2 : 4;
        instances = realloc(
            scene->curveInstances, (size_t)next_capacity * sizeof(*instances));
        if (!instances) return false;
        scene->curveInstances = instances;
        scene->curveInstanceCapacity = next_capacity;
    }
    instance = &scene->curveInstances[scene->curveInstanceCount];
    memset(instance, 0, sizeof(*instance));
    RuntimeCurveAsset3D_Init(&instance->asset);
    if (!RuntimeCurveAsset3D_CopyFrom(&instance->asset, asset)) return false;
    snprintf(instance->assetId, sizeof(instance->assetId), "%s", descriptor->assetId);
    snprintf(instance->source.objectId,
             sizeof(instance->source.objectId),
             "%s",
             descriptor->objectId);
    instance->source.sceneObjectIndex = descriptor->sceneObjectIndex;
    instance->source.kind = RUNTIME_PRIMITIVE_3D_KIND_CURVE;
    instance->position = descriptor->position;
    instance->rotation = descriptor->rotation;
    instance->uniformScale = descriptor->uniformScale;
    scene->scope.curveEnabled = true;
    scene->curveInstanceCount += 1;
    return true;
}

bool RuntimeScene3D_CopyCurveInstances(RuntimeScene3D* dst,
                                       const RuntimeScene3D* src) {
    if (!dst || !src) return false;
    for (int i = 0; i < src->curveInstanceCount; ++i) {
        const RuntimeCurveSceneInstance3D* source = &src->curveInstances[i];
        RuntimeCurveSceneInstance3DDescriptor descriptor = {
            .assetId = source->assetId,
            .objectId = source->source.objectId,
            .sceneObjectIndex = source->source.sceneObjectIndex,
            .position = source->position,
            .rotation = source->rotation,
            .uniformScale = source->uniformScale};
        if (!RuntimeScene3D_AddCurveInstance(dst, &source->asset, &descriptor)) {
            RuntimeScene3D_ClearCurveInstances(dst);
            return false;
        }
    }
    return true;
}

bool RuntimeSceneCurve3D_InstanceWorldBounds(
    const RuntimeCurveSceneInstance3D* instance,
    Vec3* out_min,
    Vec3* out_max) {
    Vec3 local_min = vec3(DBL_MAX, DBL_MAX, DBL_MAX);
    Vec3 local_max = vec3(-DBL_MAX, -DBL_MAX, -DBL_MAX);
    Vec3 world_min = vec3(DBL_MAX, DBL_MAX, DBL_MAX);
    Vec3 world_max = vec3(-DBL_MAX, -DBL_MAX, -DBL_MAX);
    if (!instance || !out_min || !out_max ||
        !RuntimeCurveAsset3D_HasReadyBLAS(&instance->asset)) {
        return false;
    }
    for (size_t i = 0u; i < instance->asset.primitiveCount; ++i) {
        const RuntimeCurvePrimitive3D* primitive = &instance->asset.primitives[i];
        const double radius = fmax(primitive->radius0, primitive->radius1);
        const Vec3 pad = vec3(radius, radius, radius);
        local_min = runtime_scene_curve_min(
            local_min,
            vec3_sub(runtime_scene_curve_min(primitive->p0, primitive->p1), pad));
        local_max = runtime_scene_curve_max(
            local_max,
            vec3_add(runtime_scene_curve_max(primitive->p0, primitive->p1), pad));
    }
    for (int corner = 0; corner < 8; ++corner) {
        const Vec3 local = vec3(
            (corner & 1) ? local_max.x : local_min.x,
            (corner & 2) ? local_max.y : local_min.y,
            (corner & 4) ? local_max.z : local_min.z);
        const Vec3 world = runtime_scene_curve_transform_point(instance, local);
        world_min = runtime_scene_curve_min(world_min, world);
        world_max = runtime_scene_curve_max(world_max, world);
    }
    *out_min = world_min;
    *out_max = world_max;
    return true;
}

bool RuntimeSceneCurve3D_TraceInstance(
    const RuntimeCurveSceneInstance3D* instance,
    int curve_scene_instance_index,
    const Ray3D* world_ray,
    double t_min,
    double t_max,
    HitInfo3D* out_hit) {
    Ray3D local_ray;
    HitInfo3D local_hit;
    double direction_scale = 0.0;
    if (!instance || !world_ray || !out_hit ||
        !RuntimeCurveAsset3D_HasReadyBLAS(&instance->asset)) {
        return false;
    }
    direction_scale = 1.0 / instance->uniformScale;
    local_ray.origin = runtime_scene_curve_inverse_point(instance, world_ray->origin);
    local_ray.direction =
        runtime_scene_curve_inverse_rotate(world_ray->direction, instance->rotation);
    HitInfo3D_Reset(&local_hit);
    if (!RuntimeCurveBLAS3D_TraceFirstHit(
            &instance->asset,
            &local_ray,
            t_min * direction_scale,
            t_max * direction_scale,
            &local_hit)) {
        HitInfo3D_Reset(out_hit);
        return false;
    }
    *out_hit = local_hit;
    out_hit->t = local_hit.t / direction_scale;
    out_hit->position = runtime_scene_curve_transform_point(instance, local_hit.position);
    out_hit->geometricNormal = vec3_normalize(
        runtime_scene_curve_rotate(local_hit.geometricNormal, instance->rotation));
    out_hit->shadingNormal = out_hit->geometricNormal;
    out_hit->normal = out_hit->geometricNormal;
    out_hit->curveTangent = vec3_normalize(
        runtime_scene_curve_rotate(local_hit.curveTangent, instance->rotation));
    out_hit->curveRadius = local_hit.curveRadius * instance->uniformScale;
    out_hit->curveSceneInstanceIndex = curve_scene_instance_index;
    out_hit->primitiveIndex = -1;
    out_hit->sceneObjectIndex = instance->source.sceneObjectIndex;
    out_hit->source = instance->source;
    return true;
}

bool RuntimeSceneCurve3D_TraceAllInstances(
    const RuntimeScene3D* scene,
    const Ray3D* world_ray,
    double t_min,
    double t_max,
    HitInfo3D* out_hit) {
    HitInfo3D best;
    bool found = false;
    if (!scene || !world_ray || !out_hit) return false;
    HitInfo3D_Reset(&best);
    for (int i = 0; i < scene->curveInstanceCount; ++i) {
        HitInfo3D candidate;
        if (!RuntimeSceneCurve3D_TraceInstance(
                &scene->curveInstances[i],
                i,
                world_ray,
                t_min,
                found ? best.t : t_max,
                &candidate)) {
            continue;
        }
        if (!found || candidate.t < best.t - 1.0e-9 ||
            (fabs(candidate.t - best.t) <= 1.0e-9 &&
             candidate.curveSceneInstanceIndex <
                 best.curveSceneInstanceIndex)) {
            best = candidate;
            found = true;
        }
    }
    if (!found) {
        HitInfo3D_Reset(out_hit);
        return false;
    }
    *out_hit = best;
    return true;
}

bool RuntimeSceneCurve3D_ResolveMaterial(
    const HitInfo3D* curve_hit,
    RuntimeMaterialPayload3D* out_payload) {
    if (!curve_hit || !out_payload || !curve_hit->hasCurveTangent ||
        curve_hit->curveSceneInstanceIndex < 0 ||
        curve_hit->source.kind != RUNTIME_PRIMITIVE_3D_KIND_CURVE) {
        return false;
    }
    return RuntimeMaterialPayload3D_ResolveFromHit(curve_hit, out_payload);
}

static uint64_t runtime_scene_curve_hash(uint64_t hash,
                                         const void* data,
                                         size_t size) {
    const unsigned char* bytes = data;
    for (size_t i = 0u; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

uint64_t RuntimeSceneCurve3D_GeometrySignature(const RuntimeScene3D* scene) {
    uint64_t hash = 1469598103934665603ull;
    if (!scene) return 0u;
    hash = runtime_scene_curve_hash(
        hash, &scene->curveInstanceCount, sizeof(scene->curveInstanceCount));
    for (int i = 0; i < scene->curveInstanceCount; ++i) {
        const RuntimeCurveSceneInstance3D* instance = &scene->curveInstances[i];
        hash = runtime_scene_curve_hash(
            hash, instance->assetId, strnlen(instance->assetId, sizeof(instance->assetId)));
        hash = runtime_scene_curve_hash(
            hash,
            instance->source.objectId,
            strnlen(instance->source.objectId, sizeof(instance->source.objectId)));
        hash = runtime_scene_curve_hash(
            hash, &instance->source.sceneObjectIndex, sizeof(int));
        hash = runtime_scene_curve_hash(hash, &instance->position, sizeof(Vec3));
        hash = runtime_scene_curve_hash(hash, &instance->rotation, sizeof(Vec3));
        hash = runtime_scene_curve_hash(
            hash, &instance->uniformScale, sizeof(instance->uniformScale));
        hash = runtime_scene_curve_hash(
            hash, &instance->asset.primitiveCount, sizeof(instance->asset.primitiveCount));
        hash = runtime_scene_curve_hash(
            hash,
            instance->asset.primitives,
            instance->asset.primitiveCount * sizeof(*instance->asset.primitives));
    }
    return hash;
}

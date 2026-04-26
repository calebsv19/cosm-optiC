#include "render/runtime_scene_3d_samples.h"

#include "config/config_manager.h"

static double runtime_scene_3d_samples_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static Point runtime_scene_3d_samples_point_at_t(const Path* path, double normalized_t) {
    if (!path || path->numPoints <= 0) {
        return (Point){0.0, 0.0};
    }
    if (path->numPoints >= 2) {
        return GetPositionAlongPathNormalized((Path*)path,
                                              runtime_scene_3d_samples_clamp01(normalized_t));
    }
    return path->points[0];
}

static double runtime_scene_3d_samples_rotation_at_t(const Path* path, double normalized_t) {
    if (!path || path->numPoints <= 0) {
        return 0.0;
    }
    if (path->numPoints >= 2) {
        return GetRotationAlongPathNormalized((Path*)path,
                                             runtime_scene_3d_samples_clamp01(normalized_t));
    }
    return path->rotations[0];
}

static double runtime_scene_3d_samples_z_at_t(const Path* path,
                                              const CameraPath3D* path3d,
                                              double normalized_t,
                                              double fallback_z) {
    if (!path || path->numPoints <= 0 || !path3d) {
        return fallback_z;
    }
    if (path->numPoints >= 2) {
        return CameraPath3D_GetPositionZNormalized(path,
                                                   path3d,
                                                   runtime_scene_3d_samples_clamp01(normalized_t));
    }
    return path3d->point_z[0];
}

static double runtime_scene_3d_samples_pitch_at_t(const Path* path,
                                                  const CameraPath3D* path3d,
                                                  double normalized_t) {
    int segment = 0;
    int next = 0;
    double local_t = 0.0;
    double pitch0 = 0.0;
    double pitch1 = 0.0;

    if (!path || !path3d || path->numPoints <= 0) {
        return 0.0;
    }
    if (path->numPoints == 1) {
        return path3d->point_pitch[0];
    }

    PathMapNormalizedT(path,
                       runtime_scene_3d_samples_clamp01(normalized_t),
                       &segment,
                       &local_t);
    if (segment < 0) segment = 0;
    if (segment >= path->numPoints) segment = path->numPoints - 1;
    next = (segment + 1 < path->numPoints) ? (segment + 1) : segment;
    pitch0 = path3d->point_pitch[segment];
    pitch1 = path3d->point_pitch[next];
    return pitch0 + (pitch1 - pitch0) * local_t;
}

bool RuntimeScene3DSampleAuthoredLight(double normalized_t, RuntimeLight3D* out_light) {
    RuntimeLight3D light = {0};
    Point position = {0.0, 0.0};

    if (!out_light || sceneSettings.bezierPath.numPoints <= 0) {
        return false;
    }

    position = runtime_scene_3d_samples_point_at_t(&sceneSettings.bezierPath, normalized_t);
    light.position = vec3(position.x,
                          position.y,
                          runtime_scene_3d_samples_z_at_t(&sceneSettings.bezierPath,
                                                          &sceneSettings.bezierPath3D,
                                                          normalized_t,
                                                          animSettings.lightHeight));
    light.radius = 10.0;
    light.intensity = animSettings.lightIntensity;
    light.falloffDistance = animSettings.forwardDecay;
    light.falloffMode = animSettings.forwardFalloffMode;

    *out_light = light;
    return true;
}

bool RuntimeScene3DSampleAuthoredCamera(double normalized_t, RuntimeCamera3D* out_camera) {
    RuntimeCamera3D camera = {0};
    Point position = {0.0, 0.0};

    if (!out_camera) {
        return false;
    }

    camera.position = vec3(sceneSettings.camera.x, sceneSettings.camera.y, sceneSettings.cameraZ);
    camera.rotation = sceneSettings.camera.rotation;
    camera.lookPitch = 0.0;
    camera.zoom = (sceneSettings.camera.zoom > 0.0) ? sceneSettings.camera.zoom : 1.0;
    camera.nearPlane = 0.1;

    if (sceneSettings.cameraPath.numPoints > 0) {
        position = runtime_scene_3d_samples_point_at_t(&sceneSettings.cameraPath, normalized_t);
        camera.position = vec3(position.x,
                               position.y,
                               runtime_scene_3d_samples_z_at_t(&sceneSettings.cameraPath,
                                                               &sceneSettings.cameraPath3D,
                                                               normalized_t,
                                                               sceneSettings.cameraZ));
        camera.rotation = runtime_scene_3d_samples_rotation_at_t(&sceneSettings.cameraPath,
                                                                 normalized_t);
        camera.lookPitch = runtime_scene_3d_samples_pitch_at_t(&sceneSettings.cameraPath,
                                                               &sceneSettings.cameraPath3D,
                                                               normalized_t);
    }

    *out_camera = camera;
    return true;
}

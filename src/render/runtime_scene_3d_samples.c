#include "render/runtime_scene_3d_samples.h"

#include "config/config_manager.h"
#include "import/runtime_scene_bridge.h"

#include <math.h>

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
    return path->rotationSet[0] ? path->rotations[0] : sceneSettings.camera.rotation;
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

static void runtime_scene_3d_samples_apply_focus_target(RuntimeCamera3D* io_camera,
                                                        double target_x,
                                                        double target_y,
                                                        double target_z) {
    Vec3 to_target = vec3(0.0, 0.0, 0.0);
    double horizontal = 0.0;
    double pitch = 0.0;
    const double max_pitch = 70.0 * M_PI / 180.0;

    if (!io_camera) return;
    to_target = vec3_sub(vec3(target_x, target_y, target_z), io_camera->position);
    horizontal = hypot(to_target.x, to_target.y);
    if (!(horizontal > 1e-9) && !(fabs(to_target.z) > 1e-9)) {
        return;
    }

    if (horizontal > 1e-9) {
        io_camera->rotation = atan2(to_target.x, -to_target.y);
    }
    pitch = atan2(to_target.z, horizontal);
    if (pitch > max_pitch) pitch = max_pitch;
    if (pitch < -max_pitch) pitch = -max_pitch;
    io_camera->lookPitch = pitch;
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
    light.radius = animSettings.lightRadius;
    light.intensity = animSettings.lightIntensity;
    light.falloffDistance = animSettings.forwardDecay;
    light.falloffMode = animSettings.forwardFalloffMode;

    *out_light = light;
    return true;
}

bool RuntimeScene3DSampleAuthoredCamera(double normalized_t, RuntimeCamera3D* out_camera) {
    RuntimeCamera3D camera = {0};
    RuntimeSceneBridge3DScaffoldState scaffold = {0};
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

    runtime_scene_bridge_get_last_3d_scaffold_state(&scaffold);
    if (scaffold.has_camera_focus_target) {
        runtime_scene_3d_samples_apply_focus_target(&camera,
                                                    scaffold.camera_focus_target_x,
                                                    scaffold.camera_focus_target_y,
                                                    scaffold.camera_focus_target_z);
    }

    *out_camera = camera;
    return true;
}

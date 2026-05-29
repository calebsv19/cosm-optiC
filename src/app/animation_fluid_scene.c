#include "app/animation.h"

#include "camera/camera_path_3d.h"
#include "config/config_manager.h"
#include "core_space.h"
#include "geo/shape_adapter.h"
#include "geo/shape_asset.h"
#include "import/fluid_import.h"
#include "import/fluid_volume_import_3d.h"
#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_volume_defaults.h"
#include "import/scene_bundle_import.h"
#include "import/shape_import.h"
#include "render/fluid/fluid_state.h"

#include <json-c/json.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <stdlib.h>
#include <string.h>

static int ClampEvenInt(int value, int min_value, int max_value) {
    if (value < min_value) value = min_value;
    if (value > max_value) value = max_value;
    if (value & 1) value += 1;
    if (value > max_value) value -= 2;
    if (value < min_value) value = min_value;
    return value;
}

static double ClampDoubleLocal(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double FluidSceneZeroLength(void) {
    double zero [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    return zero;
}

static double FluidSceneLengthEpsilon(void) {
    double epsilon [[fisics::dim(length)]] [[fisics::unit(meter)]] = 1e-4;
    return epsilon;
}

static double FluidSceneUnitLength(void) {
    double unit_length [[fisics::dim(length)]] [[fisics::unit(meter)]] = 1.0;
    return unit_length;
}

static double FluidSceneLengthCenter(double min_value [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                     double span [[fisics::dim(length)]] [[fisics::unit(meter)]]) {
    double half_span [[fisics::dim(length)]] [[fisics::unit(meter)]] = span * 0.5;
    return min_value + half_span;
}

static double FluidSceneLengthInterpolate(
    double min_value [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double span [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double normalized) {
    double delta [[fisics::dim(length)]] [[fisics::unit(meter)]] = span * normalized;
    return min_value + delta;
}

static void ResetPathLocal(Path *path, BezierMode mode) {
    if (!path) return;
    memset(path, 0, sizeof(*path));
    path->mode = mode;
}

static void ApplyFluidWindowAndCameraFit(double min_x [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                         double min_y [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                         double max_x [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                         double max_y [[fisics::dim(length)]] [[fisics::unit(meter)]]) {
    double grid_w_world [[fisics::dim(length)]] [[fisics::unit(meter)]] = max_x - min_x;
    double grid_h_world [[fisics::dim(length)]] [[fisics::unit(meter)]] = max_y - min_y;
    double epsilon = FluidSceneLengthEpsilon();
    if (grid_w_world <= epsilon || grid_h_world <= epsilon) return;

    const int target_long_edge = 1200;
    const int min_w = 320, min_h = 200;
    const int max_w = 2200, max_h = 1400;
    double aspect = grid_w_world / grid_h_world;
    int target_w = 0;
    int target_h = 0;
    if (aspect >= 1.0) {
        target_w = target_long_edge;
        target_h = (int)lround((double)target_w / aspect);
    } else {
        target_h = target_long_edge;
        target_w = (int)lround((double)target_h * aspect);
    }
    sceneSettings.windowWidth = ClampEvenInt(target_w / 2, min_w, max_w);
    sceneSettings.windowHeight = ClampEvenInt(target_h / 2, min_h, max_h);

    sceneSettings.camera.x = FluidSceneLengthCenter(min_x, grid_w_world);
    sceneSettings.camera.y = FluidSceneLengthCenter(min_y, grid_h_world);
    sceneSettings.camera.rotation = 0.0;
    double padded_w [[fisics::dim(length)]] [[fisics::unit(meter)]] = grid_w_world * 1.10;
    double padded_h [[fisics::dim(length)]] [[fisics::unit(meter)]] = grid_h_world * 1.10;
    double zoom_x = (padded_w > epsilon) ? ((double)sceneSettings.windowWidth / padded_w) : 1.0;
    double zoom_y = (padded_h > epsilon) ? ((double)sceneSettings.windowHeight / padded_h) : 1.0;
    sceneSettings.camera.zoom = ClampDoubleLocal(fmin(zoom_x, zoom_y), 0.01, 100.0);

    double margin_cap = fmin((double)sceneSettings.windowWidth, (double)sceneSettings.windowHeight) * 0.45;
    if (margin_cap < 0.0) margin_cap = 0.0;
    sceneSettings.cameraMargin = ClampDoubleLocal(sceneSettings.cameraMargin, 0.0, margin_cap);
}

static void BuildDefaultFluidPaths(double min_x [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                   double min_y [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                   double max_x [[fisics::dim(length)]] [[fisics::unit(meter)]],
                                   double max_y [[fisics::dim(length)]] [[fisics::unit(meter)]]) {
    double grid_w_world [[fisics::dim(length)]] [[fisics::unit(meter)]] = max_x - min_x;
    double grid_h_world [[fisics::dim(length)]] [[fisics::unit(meter)]] = max_y - min_y;
    double epsilon = FluidSceneLengthEpsilon();
    if (grid_w_world <= epsilon || grid_h_world <= epsilon) return;

    ResetPathLocal(&sceneSettings.cameraPath, BEZIER_CUBIC);
    CameraPath3D_Reset(&sceneSettings.cameraPath3D);

    ResetPathLocal(&sceneSettings.bezierPath, BEZIER_CUBIC);
    CameraPath3D_Reset(&sceneSettings.bezierPath3D);
    double cx [[fisics::dim(length)]] [[fisics::unit(meter)]] =
        FluidSceneLengthCenter(min_x, grid_w_world);
    double cy [[fisics::dim(length)]] [[fisics::unit(meter)]] =
        FluidSceneLengthCenter(min_y, grid_h_world);
    double orbit_rx [[fisics::dim(length)]] [[fisics::unit(meter)]] =
        fmax(grid_w_world * 0.30, grid_w_world * 0.08);
    double orbit_ry [[fisics::dim(length)]] [[fisics::unit(meter)]] =
        fmax(grid_h_world * 0.30, grid_h_world * 0.08);
    if (orbit_rx <= epsilon) orbit_rx = FluidSceneUnitLength();
    if (orbit_ry <= epsilon) orbit_ry = FluidSceneUnitLength();

    sceneSettings.bezierPath.numPoints = 4;
    sceneSettings.bezierPath.points[0] = (Point){cx - orbit_rx, cy};
    sceneSettings.bezierPath.points[1] = (Point){cx, cy - orbit_ry};
    sceneSettings.bezierPath.points[2] = (Point){cx + orbit_rx, cy};
    sceneSettings.bezierPath.points[3] = (Point){cx, cy + orbit_ry};
    for (int i = 0; i < sceneSettings.bezierPath.numPoints; ++i) {
        sceneSettings.bezierPath.rotations[i] = 0.0;
        sceneSettings.bezierPath.rotationSet[i] = true;
        sceneSettings.bezierPath.handleLink[i] = true;
        sceneSettings.bezierPath3D.point_z[i] = animSettings.lightHeight;
    }
}

static bool LoadJsonObjectFromFile(const char *path, struct json_object **out_obj) {
    if (!path || !*path || !out_obj) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        return false;
    }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) {
        free(buf);
        return false;
    }
    buf[sz] = '\0';
    struct json_object *obj = json_tokener_parse(buf);
    free(buf);
    if (!obj) return false;
    *out_obj = obj;
    return true;
}

static void BuildAssetBaseName(const char *path, char *out_base, size_t out_size) {
    if (!out_base || out_size == 0) return;
    out_base[0] = '\0';
    if (!path || !*path) return;
    const char *slash = strrchr(path, '/');
    const char *file = slash ? (slash + 1) : path;
    snprintf(out_base, out_size, "%s", file);
    char *dot = strrchr(out_base, '.');
    if (dot) *dot = '\0';
}

static bool LoadShapelibReplacementAsset(const char *asset_path, ShapeAsset *out_asset) {
    if (!asset_path || !*asset_path || !out_asset) return false;
    char base[256];
    BuildAssetBaseName(asset_path, base, sizeof(base));
    if (!base[0]) return false;

    const char *dirs[] = {"import", "../line_drawing/export", "../physics_sim/import"};
    char path_try[512];
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
        snprintf(path_try, sizeof(path_try), "%s/%s.json", dirs[i], base);
        ShapeDocument doc = {0};
        if (!shape_import_load(path_try, &doc) || doc.shapeCount <= 0) {
            ShapeDocument_Free(&doc);
            continue;
        }
        ShapeAsset candidate = {0};
        bool ok = shape_asset_from_shapelib_shape(&doc.shapes[0], 0.1f, &candidate);
        ShapeDocument_Free(&doc);
        if (!ok) continue;
        *out_asset = candidate;
        return true;
    }
    return false;
}

static bool LoadImportShapeAsset(const char *asset_path, ShapeAsset *out_asset) {
    if (!asset_path || !*asset_path || !out_asset) return false;

    ShapeAsset loaded = {0};
    bool ok = shape_asset_load_file(asset_path, &loaded);
    if (ok) {
        ShapeAsset replacement = {0};
        if (LoadShapelibReplacementAsset(asset_path, &replacement)) {
            shape_asset_free(&loaded);
            *out_asset = replacement;
            return true;
        }
        *out_asset = loaded;
        return true;
    }
    return LoadShapelibReplacementAsset(asset_path, out_asset);
}

static bool LoadPathFromJsonObject(struct json_object *path_obj, Path *out_path) {
    if (!path_obj || !out_path || !json_object_is_type(path_obj, json_type_object)) return false;
    struct json_object *points_obj = NULL;
    if (!json_object_object_get_ex(path_obj, "points", &points_obj) ||
        !json_object_is_type(points_obj, json_type_array)) {
        return false;
    }

    struct json_object *mode_obj = NULL;
    BezierMode mode = BEZIER_CUBIC;
    if (json_object_object_get_ex(path_obj, "mode", &mode_obj) && json_object_is_type(mode_obj, json_type_string)) {
        const char *mode_str = json_object_get_string(mode_obj);
        if (mode_str && strcmp(mode_str, "BEZIER_QUADRATIC") == 0) mode = BEZIER_QUADRATIC;
    }

    ResetPathLocal(out_path, BEZIER_CUBIC);
    out_path->mode = mode;
    int count = json_object_array_length(points_obj);
    if (count > MAX_BEZIER_POINTS) count = MAX_BEZIER_POINTS;
    if (count < 1) return false;
    out_path->numPoints = count;

    for (int i = 0; i < count; ++i) {
        struct json_object *pt = json_object_array_get_idx(points_obj, i);
        if (!pt || !json_object_is_type(pt, json_type_object)) continue;

        struct json_object *x_obj = NULL, *y_obj = NULL;
        if (json_object_object_get_ex(pt, "x", &x_obj) && json_object_object_get_ex(pt, "y", &y_obj)) {
            out_path->points[i].x = json_object_get_double(x_obj);
            out_path->points[i].y = json_object_get_double(y_obj);
        }

        struct json_object *rot_obj = NULL;
        if (json_object_object_get_ex(pt, "rotation", &rot_obj)) {
            out_path->rotations[i] = json_object_get_double(rot_obj);
            out_path->rotationSet[i] = true;
        }

        struct json_object *link_obj = NULL;
        if (json_object_object_get_ex(pt, "handleLink", &link_obj)) {
            out_path->handleLink[i] = json_object_get_boolean(link_obj);
        } else {
            out_path->handleLink[i] = true;
        }

        if (i < count - 1) {
            struct json_object *v1_obj = NULL;
            if (json_object_object_get_ex(pt, "velocity1", &v1_obj) &&
                json_object_is_type(v1_obj, json_type_object)) {
                struct json_object *vx = NULL, *vy = NULL;
                if (json_object_object_get_ex(v1_obj, "vx", &vx)) {
                    out_path->handles[i][0].vx = json_object_get_double(vx);
                }
                if (json_object_object_get_ex(v1_obj, "vy", &vy)) {
                    out_path->handles[i][0].vy = json_object_get_double(vy);
                }
            }
        }
        if (i > 0) {
            struct json_object *v2_obj = NULL;
            if (json_object_object_get_ex(pt, "velocity2", &v2_obj) &&
                json_object_is_type(v2_obj, json_type_object)) {
                struct json_object *vx = NULL, *vy = NULL;
                if (json_object_object_get_ex(v2_obj, "vx", &vx)) {
                    out_path->handles[i - 1][1].vx = json_object_get_double(vx);
                }
                if (json_object_object_get_ex(v2_obj, "vy", &vy)) {
                    out_path->handles[i - 1][1].vy = json_object_get_double(vy);
                }
            }
        }
    }

    return true;
}

static bool ApplyCameraConfigFromJsonObject(struct json_object *root_obj) {
    if (!root_obj || !json_object_is_type(root_obj, json_type_object)) return false;
    struct json_object *camera_obj = NULL;
    if (!json_object_object_get_ex(root_obj, "camera", &camera_obj) ||
        !json_object_is_type(camera_obj, json_type_object)) {
        return false;
    }

    struct json_object *x_obj = NULL, *y_obj = NULL, *zoom_obj = NULL, *rot_obj = NULL, *margin_obj = NULL;
    if (json_object_object_get_ex(camera_obj, "x", &x_obj)) {
        sceneSettings.camera.x = json_object_get_double(x_obj);
    }
    if (json_object_object_get_ex(camera_obj, "y", &y_obj)) {
        sceneSettings.camera.y = json_object_get_double(y_obj);
    }
    if (json_object_object_get_ex(camera_obj, "zoom", &zoom_obj)) {
        sceneSettings.camera.zoom = ClampDoubleLocal(json_object_get_double(zoom_obj), 0.01, 100.0);
    }
    if (json_object_object_get_ex(camera_obj, "rotation", &rot_obj)) {
        sceneSettings.camera.rotation = json_object_get_double(rot_obj);
    }
    if (json_object_object_get_ex(camera_obj, "margin", &margin_obj)) {
        double margin_cap = fmin((double)sceneSettings.windowWidth, (double)sceneSettings.windowHeight) * 0.45;
        sceneSettings.cameraMargin = ClampDoubleLocal(json_object_get_double(margin_obj), 0.0, margin_cap);
    }
    return true;
}

static void ApplyBundleSceneMetadata(const SceneBundleImportResult *bundle_info) {
    if (!bundle_info) return;

    if (bundle_info->has_camera_path && bundle_info->camera_path[0]) {
        struct json_object *root = NULL;
        if (LoadJsonObjectFromFile(bundle_info->camera_path, &root)) {
            ApplyCameraConfigFromJsonObject(root);
            struct json_object *camera_path_obj = NULL;
            if (json_object_object_get_ex(root, "cameraPath", &camera_path_obj)) {
                LoadPathFromJsonObject(camera_path_obj, &sceneSettings.cameraPath);
            } else {
                LoadPathFromJsonObject(root, &sceneSettings.cameraPath);
            }
            json_object_put(root);
        }
    }

    if (bundle_info->has_light_path && bundle_info->light_path[0]) {
        struct json_object *root = NULL;
        if (LoadJsonObjectFromFile(bundle_info->light_path, &root)) {
            struct json_object *light_path_obj = NULL;
            if (json_object_object_get_ex(root, "path", &light_path_obj)) {
                LoadPathFromJsonObject(light_path_obj, &sceneSettings.bezierPath);
            } else {
                LoadPathFromJsonObject(root, &sceneSettings.bezierPath);
            }
            json_object_put(root);
        }
    }
}

bool AnimationUseFluidScene(void) {
    return animation_config_scene_source_is_fluid(animSettings.sceneSource) &&
           animSettings.fluidManifest[0];
}

void AnimationClearFluidGrid(void) {
    g_fluidGrid.valid = false;
    g_fluidGrid.min_x = g_fluidGrid.min_y = 0.0f;
    g_fluidGrid.max_x = g_fluidGrid.max_y = 0.0f;
}

typedef struct {
    int source;
    bool use_fluid_scene;
    char fluid_manifest[sizeof(animSettings.fluidManifest)];
    char runtime_scene_path[sizeof(animSettings.runtimeScenePath)];
    bool volume_interaction_enabled;
    int volume_source_kind;
    char volume_source_path[sizeof(animSettings.volumeSourcePath)];
} AnimationSceneSourceSnapshot;

static void animation_scene_source_snapshot_capture(AnimationSceneSourceSnapshot *out_snapshot) {
    if (!out_snapshot) return;
    out_snapshot->source = animation_config_scene_source_clamp(animSettings.sceneSource);
    out_snapshot->use_fluid_scene = animSettings.useFluidScene;
    strncpy(out_snapshot->fluid_manifest,
            animSettings.fluidManifest,
            sizeof(out_snapshot->fluid_manifest) - 1);
    out_snapshot->fluid_manifest[sizeof(out_snapshot->fluid_manifest) - 1] = '\0';
    strncpy(out_snapshot->runtime_scene_path,
            animSettings.runtimeScenePath,
            sizeof(out_snapshot->runtime_scene_path) - 1);
    out_snapshot->runtime_scene_path[sizeof(out_snapshot->runtime_scene_path) - 1] = '\0';
    out_snapshot->volume_interaction_enabled = animSettings.volumeInteractionEnabled;
    out_snapshot->volume_source_kind = animSettings.volumeSourceKind;
    strncpy(out_snapshot->volume_source_path,
            animSettings.volumeSourcePath,
            sizeof(out_snapshot->volume_source_path) - 1);
    out_snapshot->volume_source_path[sizeof(out_snapshot->volume_source_path) - 1] = '\0';
}

static void animation_scene_source_snapshot_restore(const AnimationSceneSourceSnapshot *snapshot) {
    if (!snapshot) return;
    animSettings.sceneSource = (SceneSource)animation_config_scene_source_clamp(snapshot->source);
    animSettings.useFluidScene = snapshot->use_fluid_scene;
    strncpy(animSettings.fluidManifest,
            snapshot->fluid_manifest,
            sizeof(animSettings.fluidManifest) - 1);
    animSettings.fluidManifest[sizeof(animSettings.fluidManifest) - 1] = '\0';
    strncpy(animSettings.runtimeScenePath,
            snapshot->runtime_scene_path,
            sizeof(animSettings.runtimeScenePath) - 1);
    animSettings.runtimeScenePath[sizeof(animSettings.runtimeScenePath) - 1] = '\0';
    animSettings.volumeInteractionEnabled = snapshot->volume_interaction_enabled;
    animSettings.volumeSourceKind = snapshot->volume_source_kind;
    strncpy(animSettings.volumeSourcePath,
            snapshot->volume_source_path,
            sizeof(animSettings.volumeSourcePath) - 1);
    animSettings.volumeSourcePath[sizeof(animSettings.volumeSourcePath) - 1] = '\0';
}

static bool animation_scene_source_assign_candidate(int source, const char *path) {
    char previous_runtime_scene_path[sizeof(animSettings.runtimeScenePath)] = {0};
    source = animation_config_scene_source_clamp(source);
    if (source == SCENE_SOURCE_CONFIG_2D) {
        animSettings.sceneSource = SCENE_SOURCE_CONFIG_2D;
        animSettings.useFluidScene = false;
        animSettings.fluidManifest[0] = '\0';
        animSettings.runtimeScenePath[0] = '\0';
        return true;
    }
    if (!path || !path[0]) {
        return false;
    }
    if (source == SCENE_SOURCE_FLUID_MANIFEST) {
        animSettings.sceneSource = SCENE_SOURCE_FLUID_MANIFEST;
        animSettings.useFluidScene = true;
        strncpy(animSettings.fluidManifest, path, sizeof(animSettings.fluidManifest) - 1);
        animSettings.fluidManifest[sizeof(animSettings.fluidManifest) - 1] = '\0';
        animSettings.runtimeScenePath[0] = '\0';
        return true;
    }
    if (source == SCENE_SOURCE_RUNTIME_SCENE) {
        snprintf(previous_runtime_scene_path,
                 sizeof(previous_runtime_scene_path),
                 "%s",
                 animSettings.runtimeScenePath);
        animSettings.sceneSource = SCENE_SOURCE_RUNTIME_SCENE;
        animSettings.useFluidScene = false;
        strncpy(animSettings.runtimeScenePath, path, sizeof(animSettings.runtimeScenePath) - 1);
        animSettings.runtimeScenePath[sizeof(animSettings.runtimeScenePath) - 1] = '\0';
        animSettings.fluidManifest[0] = '\0';
        runtime_scene_volume_defaults_apply_transition(&animSettings,
                                                       previous_runtime_scene_path,
                                                       animSettings.runtimeScenePath);
        return true;
    }
    return false;
}

static RuntimeVolume3DSourceKind animation_volume_source_kind_to_runtime_kind(int kind) {
    switch (animation_config_volume_source_kind_clamp(kind)) {
        case VOLUME_SOURCE_MANIFEST:
            return RUNTIME_VOLUME_3D_SOURCE_MANIFEST;
        case VOLUME_SOURCE_RAW_VF3D:
            return RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D;
        case VOLUME_SOURCE_PACK:
            return RUNTIME_VOLUME_3D_SOURCE_PACK;
        case VOLUME_SOURCE_NONE:
        default:
            return RUNTIME_VOLUME_3D_SOURCE_NONE;
    }
}

bool AnimationSelectVolumeSource(int kind, const char *path, bool apply_immediately) {
    AnimationSceneSourceSnapshot snapshot = {0};
    RuntimeVolumeAttachment3D attachment = {0};
    RuntimeVolume3DSourceKind runtime_kind = RUNTIME_VOLUME_3D_SOURCE_NONE;
    char diagnostics[128] = {0};

    kind = animation_config_volume_source_kind_clamp(kind);
    if (kind == VOLUME_SOURCE_NONE || !path || !path[0]) {
        return false;
    }

    animation_scene_source_snapshot_capture(&snapshot);
    animSettings.volumeSourceKind = kind;
    strncpy(animSettings.volumeSourcePath, path, sizeof(animSettings.volumeSourcePath) - 1);
    animSettings.volumeSourcePath[sizeof(animSettings.volumeSourcePath) - 1] = '\0';
    animSettings.volumeInteractionEnabled = true;

    if (!apply_immediately) {
        return true;
    }

    runtime_kind = animation_volume_source_kind_to_runtime_kind(kind);
    if (runtime_kind == RUNTIME_VOLUME_3D_SOURCE_NONE ||
        !fluid_volume_import_3d_load_source(path,
                                            runtime_kind,
                                            &attachment,
                                            diagnostics,
                                            sizeof(diagnostics))) {
        animation_scene_source_snapshot_restore(&snapshot);
        RuntimeVolumeAttachment3D_Reset(&attachment);
        return false;
    }

    RuntimeVolumeAttachment3D_Reset(&attachment);
    return true;
}

void AnimationClearVolumeSource(void) {
    animSettings.volumeInteractionEnabled = false;
    animSettings.volumeSourceKind = VOLUME_SOURCE_NONE;
    animSettings.volumeSourcePath[0] = '\0';
}

bool AnimationApplyFluidScene(const char *manifest_path) {
    if (!manifest_path || !*manifest_path) return false;

    SceneBundleImportResult bundle_info;
    memset(&bundle_info, 0, sizeof(bundle_info));
    bool has_bundle = scene_bundle_import_resolve_fluid_source(manifest_path, &bundle_info);
    const char *source_path = (has_bundle && bundle_info.fluid_source_path[0]) ? bundle_info.fluid_source_path : manifest_path;

    FluidManifest manifest = {0};
    if (!fluid_manifest_load(source_path, &manifest)) {
        printf("[menu] failed to load manifest %s\n", source_path);
        return false;
    }

    g_fluidGrid.valid = true;
    g_fluidGrid.min_x = manifest.origin_x;
    g_fluidGrid.min_y = manifest.origin_y;
    g_fluidGrid.max_x = manifest.origin_x + manifest.cell_size * (float)manifest.grid_w;
    g_fluidGrid.max_y = manifest.origin_y + manifest.cell_size * (float)manifest.grid_h;

    sceneSettings.objectCount = 0;
    double grid_w_world [[fisics::dim(length)]] [[fisics::unit(meter)]] =
        g_fluidGrid.max_x - g_fluidGrid.min_x;
    double grid_h_world [[fisics::dim(length)]] [[fisics::unit(meter)]] =
        g_fluidGrid.max_y - g_fluidGrid.min_y;
    ApplyFluidWindowAndCameraFit(g_fluidGrid.min_x, g_fluidGrid.min_y,
                                 g_fluidGrid.max_x, g_fluidGrid.max_y);
    BuildDefaultFluidPaths(g_fluidGrid.min_x, g_fluidGrid.min_y,
                           g_fluidGrid.max_x, g_fluidGrid.max_y);
    if (has_bundle) {
        ApplyBundleSceneMetadata(&bundle_info);
    }
    CoreSpaceDesc space_desc;
    bool use_core_space = false;
    if (core_space_desc_default_from_grid((int)manifest.grid_w,
                                          (int)manifest.grid_h,
                                          manifest.origin_x,
                                          manifest.origin_y,
                                          manifest.cell_size,
                                          &space_desc).code == CORE_OK) {
        if (manifest.space_author_window_w > 0) {
            space_desc.author_window_w = manifest.space_author_window_w;
        }
        if (manifest.space_author_window_h > 0) {
            space_desc.author_window_h = manifest.space_author_window_h;
        }
        if (manifest.space_desired_fit > 0.0f) {
            space_desc.desired_fit = manifest.space_desired_fit;
        }
        use_core_space = true;
    }

    for (size_t i = 0; i < manifest.import_count; ++i) {
        const FluidImportShape *imp = &manifest.imports[i];
        if (!imp->path) continue;
        double world_x [[fisics::dim(length)]] [[fisics::unit(meter)]] =
            FluidSceneLengthInterpolate(g_fluidGrid.min_x, grid_w_world, imp->pos_x_norm);
        double world_y [[fisics::dim(length)]] [[fisics::unit(meter)]] =
            FluidSceneLengthInterpolate(g_fluidGrid.min_y, grid_h_world, imp->pos_y_norm);
        ShapeAsset asset = {0};
        bool loaded = LoadImportShapeAsset(imp->path, &asset);
        double angle = imp->rotation_deg * M_PI / 180.0;
        double scale = (imp->scale > 0.0f) ? imp->scale : 1.0;
        if (use_core_space) {
            float asset_max_dim = 1.0f;
            if (loaded) {
                ShapeAssetBounds bnds;
                if (shape_asset_bounds(&asset, &bnds) && bnds.valid) {
                    float dx = bnds.max_x - bnds.min_x;
                    float dy = bnds.max_y - bnds.min_y;
                    asset_max_dim = (dx > dy) ? dx : dy;
                    if (asset_max_dim <= 0.0001f) asset_max_dim = 1.0f;
                }
            }

            CoreSpaceImport import_in;
            CoreSpaceWorldTransform world_out;
            memset(&import_in, 0, sizeof(import_in));
            memset(&world_out, 0, sizeof(world_out));
            import_in.pos_x_raw = imp->pos_x_norm;
            import_in.pos_y_raw = imp->pos_y_norm;
            import_in.rotation_deg = imp->rotation_deg;
            import_in.scale = (imp->scale > 0.0f) ? imp->scale : 1.0f;
            import_in.asset_max_dim = asset_max_dim;
            if (core_space_import_to_world(&space_desc, &import_in, &world_out).code == CORE_OK) {
                world_x = world_out.x;
                world_y = world_out.y;
                scale = world_out.scale;
            }
        }
        if (loaded) {
            int before = sceneSettings.objectCount;
            ShapeToSceneOptions opts = {.scale = scale, .offset_x = world_x, .offset_y = world_y};
            shape_asset_append_to_scene(&asset, &opts);
            int after = sceneSettings.objectCount;
            for (int oi = before; oi < after; ++oi) {
                sceneSettings.sceneObjects[oi].rotation = angle;
                sceneSettings.sceneObjects[oi].dirty = true;
            }
            shape_asset_free(&asset);
        }
    }

    animSettings.sceneSource = SCENE_SOURCE_FLUID_MANIFEST;
    animSettings.useFluidScene = true;
    animSettings.runtimeScenePath[0] = '\0';
    if (manifest_path != animSettings.fluidManifest) {
        strncpy(animSettings.fluidManifest, manifest_path, sizeof(animSettings.fluidManifest) - 1);
        animSettings.fluidManifest[sizeof(animSettings.fluidManifest) - 1] = '\0';
    }
    printf("[fluid] scene applied source=%s grid=%ux%u window=%dx%d camera=(%.2f,%.2f) zoom=%.3f\n",
           source_path,
           manifest.grid_w,
           manifest.grid_h,
           sceneSettings.windowWidth,
           sceneSettings.windowHeight,
           sceneSettings.camera.x,
           sceneSettings.camera.y,
           sceneSettings.camera.zoom);
    fluid_manifest_free(&manifest);
    return true;
}

bool AnimationApplyActiveSceneSource(void) {
    int source = animation_config_scene_source_clamp(animSettings.sceneSource);
    animSettings.sceneSource = (SceneSource)source;

    if (source == SCENE_SOURCE_FLUID_MANIFEST) {
        if (animSettings.fluidManifest[0] == '\0') {
            animSettings.sceneSource = SCENE_SOURCE_CONFIG_2D;
            animSettings.useFluidScene = false;
            animSettings.fluidManifest[0] = '\0';
            animSettings.runtimeScenePath[0] = '\0';
            AnimationClearFluidGrid();
            return false;
        }
        if (!AnimationApplyFluidScene(animSettings.fluidManifest)) {
            fprintf(stderr,
                    "[fluid-scene] failed to apply fluid scene source '%s'\n",
                    animSettings.fluidManifest);
            animSettings.sceneSource = SCENE_SOURCE_CONFIG_2D;
            animSettings.useFluidScene = false;
            animSettings.fluidManifest[0] = '\0';
            animSettings.runtimeScenePath[0] = '\0';
            AnimationClearFluidGrid();
            return false;
        }
        return true;
    }

    if (source == SCENE_SOURCE_RUNTIME_SCENE) {
        RuntimeSceneBridgePreflight summary;
        if (animSettings.runtimeScenePath[0] == '\0') {
            animSettings.sceneSource = SCENE_SOURCE_CONFIG_2D;
            animSettings.useFluidScene = false;
            animSettings.fluidManifest[0] = '\0';
            animSettings.runtimeScenePath[0] = '\0';
            AnimationClearFluidGrid();
            return false;
        }
        if (!runtime_scene_bridge_apply_file(animSettings.runtimeScenePath, &summary)) {
            fprintf(stderr,
                    "[runtime-scene] failed to apply runtime scene source '%s': %s\n",
                    animSettings.runtimeScenePath,
                    summary.diagnostics);
            animSettings.sceneSource = SCENE_SOURCE_CONFIG_2D;
            animSettings.useFluidScene = false;
            animSettings.fluidManifest[0] = '\0';
            animSettings.runtimeScenePath[0] = '\0';
            AnimationClearFluidGrid();
            return false;
        }
        animSettings.useFluidScene = false;
        return true;
    }

    animSettings.useFluidScene = false;
    animSettings.fluidManifest[0] = '\0';
    animSettings.runtimeScenePath[0] = '\0';
    AnimationClearFluidGrid();
    return true;
}

bool AnimationSelectSceneSource(int source, const char *path, bool apply_immediately) {
    AnimationSceneSourceSnapshot snapshot = {0};
    animation_scene_source_snapshot_capture(&snapshot);
    if (!animation_scene_source_assign_candidate(source, path)) {
        return false;
    }
    if (!apply_immediately) {
        return true;
    }
    if (AnimationApplyActiveSceneSource()) {
        return true;
    }
    animation_scene_source_snapshot_restore(&snapshot);
    return false;
}

bool AnimationRestoreActiveSceneSource(bool persist_on_failure) {
    bool ok = AnimationApplyActiveSceneSource();
    if (!ok && persist_on_failure) {
        SaveAnimationConfig();
    }
    return ok;
}

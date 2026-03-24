#ifndef MAIN_DRIVER
#define MAIN_DRIVER
#endif

#include "ui/sdl_menu.h"
#include "tools/make_video.h"
#include "app/animation.h"
#include "app/runtime_time.h"
#include "config/config_manager.h"
#include "scene/object_manager.h"
#include "render/ray_tracing2.h"
#include "editor/bezier_editor.h"
#include "path/path_system.h"
#include "render/timer_hud_api.h"
#include "render/timer_hud_adapter.h"
#include "camera/camera.h"
#include "render/render_helper.h"
#include "engine/Render/render_pipeline.h"
#include "import/fluid_import.h"
#include "import/scene_bundle_import.h"
#include "import/shape_import.h"
#include "export/render_metrics_dataset.h"
#include "core_space.h"
#include "geo/shape_asset.h"
#include "geo/shape_adapter.h"
#include "render/vk_shared_device.h"
#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>  // For mkdir()
#include <sys/types.h>

#define SCENE_CONFIG_FILE "Configs/scene_config.json"
#define ANIMATION_CONFIG_FILE "Configs/animation_config.json"

// Global animation settings (Loaded from config)
int WINDOW_WIDTH;
int WINDOW_HEIGHT;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
#if USE_VULKAN
static VkRenderer renderer_storage;
#endif
SceneObject sceneObjects[10];  // Define object array storage
int objectCount = 0;  // Define object count

bool running;
double accumulator;
double currentTime;
int frameCounter;
int loopCount;
static bool quitRequested = false;

double t_increment;
double t_param = 0.0;  // Parameter (0 to 1) for interpolation along the path.
int direction = 1;      // +1 for forward, -1 for reverse.
static const double kPreviewBg = 60.0;

static const char* s_fluidManifestOverride = NULL;
#include "render/fluid_state.h"

static bool EnvEnabled(const char *name) {
    const char *v = getenv(name);
    if (!v || !v[0]) return false;
    return strcmp(v, "1") == 0 || strcmp(v, "true") == 0 || strcmp(v, "TRUE") == 0 ||
           strcmp(v, "yes") == 0 || strcmp(v, "on") == 0;
}

static void ExportRenderMetricsDatasetIfEnabled(void) {
    if (!EnvEnabled("RAY_TRACING_EXPORT_RENDER_METRICS_DATASET")) return;

    const char *out_path = getenv("RAY_TRACING_RENDER_METRICS_DATASET_PATH");
    RayTracingRenderMetricsSnapshot snapshot = {0};
    snapshot.frames_rendered = frameCounter;
    snapshot.loops_completed = loopCount;
    snapshot.runtime_seconds = currentTime;
    snapshot.scene_object_count = sceneSettings.objectCount;
    snapshot.scene_rays = sceneSettings.rays;
    snapshot.target_fps = animSettings.fps;
    snapshot.frame_duration_seconds = animSettings.frameDuration;
    snapshot.integrator_mode = animSettings.integratorMode;
    snapshot.bounce_limit = animSettings.bounceLimit;
    snapshot.path_samples_per_pixel = animSettings.pathSamplesPerPixel;
    snapshot.path_max_depth = animSettings.pathMaxDepth;
    snapshot.use_tiled_renderer = animSettings.useTiledRenderer;
    snapshot.tile_size = animSettings.tileSize;
    snapshot.light_intensity = animSettings.lightIntensity;
    snapshot.cache_variance_cutoff = animSettings.cacheVarianceCutoff;
    snapshot.cache_halo_radius = animSettings.cacheHaloRadius;
    snapshot.environment_brightness = animSettings.environmentBrightness;
    snapshot.interactive_mode = animSettings.interactiveMode;
    snapshot.deep_render_mode = animSettings.deepRenderMode;
    snapshot.bounce_mode = animSettings.bounceMode;

    if (!ray_tracing_render_metrics_dataset_export_json(&snapshot, out_path)) {
        fprintf(stderr, "[render_metrics] failed to export dataset json\n");
        return;
    }

    if (out_path && out_path[0]) {
        printf("[render_metrics] dataset exported: %s\n", out_path);
    } else {
        printf("[render_metrics] dataset exported: Configs/render_metrics.dataset.json\n");
    }
}

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

static void ResetPathLocal(Path *path, BezierMode mode) {
    if (!path) return;
    memset(path, 0, sizeof(*path));
    path->mode = mode;
}

static void ApplyFluidWindowAndCameraFit(double min_x, double min_y,
                                         double max_x, double max_y) {
    double grid_w_world = max_x - min_x;
    double grid_h_world = max_y - min_y;
    if (grid_w_world <= 1e-4 || grid_h_world <= 1e-4) return;

    // Fluid scenes should not inherit tiny test windows from old configs.
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
    // Keep imported scene preview lighter-weight for editor/runtime iteration.
    sceneSettings.windowWidth = ClampEvenInt(target_w / 2, min_w, max_w);
    sceneSettings.windowHeight = ClampEvenInt(target_h / 2, min_h, max_h);

    sceneSettings.camera.x = min_x + grid_w_world * 0.5;
    sceneSettings.camera.y = min_y + grid_h_world * 0.5;
    sceneSettings.camera.rotation = 0.0;
    double padded_w = grid_w_world * 1.10;
    double padded_h = grid_h_world * 1.10;
    double zoom_x = (padded_w > 1e-4) ? ((double)sceneSettings.windowWidth / padded_w) : 1.0;
    double zoom_y = (padded_h > 1e-4) ? ((double)sceneSettings.windowHeight / padded_h) : 1.0;
    sceneSettings.camera.zoom = ClampDoubleLocal(fmin(zoom_x, zoom_y), 0.01, 100.0);

    double margin_cap = fmin((double)sceneSettings.windowWidth, (double)sceneSettings.windowHeight) * 0.45;
    if (margin_cap < 0.0) margin_cap = 0.0;
    sceneSettings.cameraMargin = ClampDoubleLocal(sceneSettings.cameraMargin, 0.0, margin_cap);
}

static void BuildDefaultFluidPaths(double min_x, double min_y,
                                   double max_x, double max_y) {
    double grid_w_world = max_x - min_x;
    double grid_h_world = max_y - min_y;
    if (grid_w_world <= 1e-4 || grid_h_world <= 1e-4) return;

    ResetPathLocal(&sceneSettings.cameraPath, BEZIER_CUBIC);
    sceneSettings.cameraPath.numPoints = 1;
    sceneSettings.cameraPath.points[0].x = sceneSettings.camera.x;
    sceneSettings.cameraPath.points[0].y = sceneSettings.camera.y;
    sceneSettings.cameraPath.rotations[0] = sceneSettings.camera.rotation;
    sceneSettings.cameraPath.rotationSet[0] = true;
    sceneSettings.cameraPath.handleLink[0] = true;

    ResetPathLocal(&sceneSettings.bezierPath, BEZIER_CUBIC);
    double cx = min_x + grid_w_world * 0.5;
    double cy = min_y + grid_h_world * 0.5;
    double orbit_rx = fmax(grid_w_world * 0.30, grid_w_world * 0.08);
    double orbit_ry = fmax(grid_h_world * 0.30, grid_h_world * 0.08);
    if (orbit_rx <= 1e-4) orbit_rx = 1.0;
    if (orbit_ry <= 1e-4) orbit_ry = 1.0;

    sceneSettings.bezierPath.numPoints = 4;
    sceneSettings.bezierPath.points[0] = (Point){cx - orbit_rx, cy};
    sceneSettings.bezierPath.points[1] = (Point){cx, cy - orbit_ry};
    sceneSettings.bezierPath.points[2] = (Point){cx + orbit_rx, cy};
    sceneSettings.bezierPath.points[3] = (Point){cx, cy + orbit_ry};
    for (int i = 0; i < sceneSettings.bezierPath.numPoints; ++i) {
        sceneSettings.bezierPath.rotations[i] = 0.0;
        sceneSettings.bezierPath.rotationSet[i] = true;
        sceneSettings.bezierPath.handleLink[i] = true;
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
    struct json_object *points_array = NULL;
    if (!json_object_object_get_ex(path_obj, "points", &points_array) ||
        !json_object_is_type(points_array, json_type_array)) {
        return false;
    }
    int count = json_object_array_length(points_array);
    if (count < 1) return false;
    if (count > MAX_BEZIER_POINTS) count = MAX_BEZIER_POINTS;

    ResetPathLocal(out_path, BEZIER_CUBIC);
    out_path->numPoints = count;

    struct json_object *mode_obj = NULL;
    if (json_object_object_get_ex(path_obj, "mode", &mode_obj) &&
        json_object_is_type(mode_obj, json_type_string)) {
        const char *mode_s = json_object_get_string(mode_obj);
        if (mode_s && (strcmp(mode_s, "BEZIER_QUADRATIC") == 0 || strcmp(mode_s, "Quadratic") == 0)) {
            out_path->mode = BEZIER_QUADRATIC;
        }
    }

    for (int i = 0; i < count; ++i) {
        struct json_object *pt = json_object_array_get_idx(points_array, i);
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
    return animSettings.useFluidScene && animSettings.fluidManifest[0];
}

void AnimationClearFluidGrid(void) {
    g_fluidGrid.valid = false;
    g_fluidGrid.min_x = g_fluidGrid.min_y = 0.0f;
    g_fluidGrid.max_x = g_fluidGrid.max_y = 0.0f;
}

static void HandleFluidOverlayKey(SDL_Keycode key) {
    if (key == SDLK_f) {
        g_fluidOverlayEnabled = !g_fluidOverlayEnabled;
        printf("[fluid] overlay %s\n", g_fluidOverlayEnabled ? "enabled" : "disabled");
    } else if (key == SDLK_LEFTBRACKET) {
        if (g_fluidFrameIndex > 0) g_fluidFrameIndex--;
        printf("[fluid] frame %d\n", g_fluidFrameIndex);
    } else if (key == SDLK_RIGHTBRACKET) {
        g_fluidFrameIndex++;
        printf("[fluid] frame %d\n", g_fluidFrameIndex);
    } else if (key == SDLK_v) {
        if (g_fluidOverlayMode == FLUID_OVERLAY_MODE_DENSITY) {
            g_fluidOverlayMode = FLUID_OVERLAY_MODE_DENSITY_VELOCITY;
        } else if (g_fluidOverlayMode == FLUID_OVERLAY_MODE_DENSITY_VELOCITY) {
            g_fluidOverlayMode = FLUID_OVERLAY_MODE_VELOCITY_HEATMAP;
        } else {
            g_fluidOverlayMode = FLUID_OVERLAY_MODE_DENSITY;
        }
        printf("[fluid] overlay mode %s\n",
               g_fluidOverlayMode == FLUID_OVERLAY_MODE_DENSITY
                   ? "density"
                   : (g_fluidOverlayMode == FLUID_OVERLAY_MODE_DENSITY_VELOCITY
                          ? "density+velocity"
                          : "velocity-heatmap+velocity"));
    }
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

    // Fit grid
    g_fluidGrid.valid = true;
    g_fluidGrid.min_x = manifest.origin_x;
    g_fluidGrid.min_y = manifest.origin_y;
    g_fluidGrid.max_x = manifest.origin_x + manifest.cell_size * (float)manifest.grid_w;
    g_fluidGrid.max_y = manifest.origin_y + manifest.cell_size * (float)manifest.grid_h;

    // Reset scene and place imports
    sceneSettings.objectCount = 0;
    double grid_w_world = g_fluidGrid.max_x - g_fluidGrid.min_x;
    double grid_h_world = g_fluidGrid.max_y - g_fluidGrid.min_y;
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
        double world_x = g_fluidGrid.min_x + (grid_w_world) * imp->pos_x_norm;
        double world_y = g_fluidGrid.min_y + (grid_h_world) * imp->pos_y_norm;
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

    animSettings.useFluidScene = true;
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

char loopMode[16] = "stop";  // Increased buffer size for safety
int maxLoopCount = 1;  // Default to 1 loop if not set

static void EnsureDirectoryExists(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);  // Create directory with full permissions
    }
}
        
void SaveFrame(int frameNumber) {
#if USE_VULKAN
    EnsureDirectoryExists(animSettings.frameDir);

    char filename[256];
    snprintf(filename, sizeof(filename), "%s/frame_%04d.bmp", animSettings.frameDir, frameNumber);

    VkResult capture_result = vk_renderer_request_capture((VkRenderer*)renderer, filename);
    if (capture_result != VK_SUCCESS) {
        fprintf(stderr, "SaveFrame failed to request capture: %d\n", capture_result);
    }
    return;
#else
    // Ensure the frame directory exists
    EnsureDirectoryExists(animSettings.frameDir);
    
    // Generate filename for output frame
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/frame_%04d.bmp", animSettings.frameDir, frameNumber);
                              
    printf("Saving frame to: %s\n", filename);
        
    // Retrieve window dimensions from scene settings
    int width = sceneSettings.windowWidth;
    int height = sceneSettings.windowHeight;

    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 32,
                                                0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!surface) { 
        fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
        return;
    }
        
    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                             surface->pixels, surface->pitch) != 0) {
        fprintf(stderr, "SDL_RenderReadPixels failed: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }
        
    if (SDL_SaveBMP(surface, filename) != 0) {
        fprintf(stderr, "SDL_SaveBMP failed: %s\n", SDL_GetError());
    } else {
        printf("Saved %s\n", filename);
    }

    SDL_FreeSurface(surface);
#endif
}


int AnimationInit(void) {
    LoadAnimationConfig();
    if (s_fluidManifestOverride && s_fluidManifestOverride[0]) {
        strncpy(animSettings.fluidManifest, s_fluidManifestOverride, sizeof(animSettings.fluidManifest) - 1);
        animSettings.fluidManifest[sizeof(animSettings.fluidManifest) - 1] = '\0';
        animSettings.useFluidScene = true;
    }
    LoadSceneConfig();
    if (animSettings.useFluidScene && animSettings.fluidManifest[0]) {
        AnimationApplyFluidScene(animSettings.fluidManifest);
    } else {
        AnimationClearFluidGrid();
    }
    UpdateObjects();
    WINDOW_WIDTH = sceneSettings.windowWidth;       
    WINDOW_HEIGHT = sceneSettings.windowHeight; 
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return -1;
    }

    // Create window
    window = SDL_CreateWindow("Raytracing Animation",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

#if USE_VULKAN
    VkRendererConfig cfg;
    vk_renderer_config_set_defaults(&cfg);
    cfg.enable_validation = SDL_FALSE;
    cfg.clear_color[0] = 0.0f;
    cfg.clear_color[1] = 0.0f;
    cfg.clear_color[2] = 0.0f;
    cfg.clear_color[3] = 1.0f;

    if (!vk_shared_device_init(window, &cfg)) {
        fprintf(stderr, "vk_shared_device_init failed.\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    VkRendererDevice* shared_device = vk_shared_device_get();
    if (!shared_device) {
        fprintf(stderr, "vk_shared_device_get failed.\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    VkResult init = vk_renderer_init_with_device(&renderer_storage, shared_device, window, &cfg);
    if (init != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_init failed: %d\n", init);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    renderer = (SDL_Renderer*)&renderer_storage;
    vk_renderer_set_logical_size((VkRenderer*)renderer, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
#else
    // Create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
#endif

    timer_hud_register_backend();
    ts_init();
    setRenderContext(renderer, window, WINDOW_WIDTH, WINDOW_HEIGHT);

    // Validate Bézier path
    if (sceneSettings.bezierPath.numPoints < 2) {
        fprintf(stderr, "Error: Bézier path is uninitialized or invalid. Check scene_config.json.\n");
        AnimationCleanup();
        return -1;
    }
    // Initialize ray tracing scene
    InitRayTracingScene();
    printf("Completed initialization in animation.c\n");

    return 0;
}



void AnimationCleanup(void) {   
    if (renderer) {
#if USE_VULKAN
        vk_renderer_wait_idle((VkRenderer*)renderer);
        vk_renderer_shutdown_surface((VkRenderer*)renderer);
#else
        SDL_DestroyRenderer(renderer);
#endif
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
#if USE_VULKAN
    vk_shared_device_shutdown();
#endif
    SDL_Quit();
}

void HandleEvents(bool* running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            *running = false; // return to menu instead of killing app
        } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            *running = false; // escape exits loop, menu decides next step
        } else if (event.type == SDL_KEYDOWN) {
            HandleFluidOverlayKey(event.key.keysym.sym);
        } else if (animSettings.interactiveMode && (event.type == SDL_MOUSEMOTION ||
                                event.type == SDL_MOUSEBUTTONDOWN)) {
            ProcessRayTracingEvent(&event);
        }
    }
}

void UpdateSimulation(double* accumulator, double* currentTime, int* loopCount) {
    uint64_t now_ns = runtime_time_now_ns();
    static uint64_t prev_ns = 0;
    if (prev_ns == 0) {
        prev_ns = now_ns;
    }
    double deltaTime = runtime_time_diff_seconds(now_ns, prev_ns);
    prev_ns = now_ns;
    *accumulator += deltaTime;

    // Only process if enough time has passed for one frame
    if (*accumulator < animSettings.frameDuration) {
        return;  // Not enough time passed, exit early
    }

    // Move forward on the Bézier path
    t_param += t_increment * direction;

    // Bounce mode logic
    if (animSettings.bounceMode) {
        if (t_param >= 1.0) {
            t_param = 1.0;
            direction = -1;
            (*loopCount)++;
        } else if (t_param <= 0.0) {
            t_param = 0.0;
            direction = 1;
            (*loopCount)++;   
        }
    } else { // Normal path following
        if (t_param > 1.0) {
            if (strcmp(animSettings.loopMode, "loop") == 0) {
                t_param = 0.0;
                (*loopCount)++;
            } else {
                t_param = 1.0;
            }
        }
    }

    printf("DEBUG: t_param = %.3f\n", t_param);

    *currentTime += animSettings.frameDuration;
    *accumulator -= animSettings.frameDuration;  // Reset accumulator after processing one frame
}


void UpdateLightPosition(double* lightX, double* lightY) {
    if (animSettings.interactiveMode) {
        GetCurrentLightPosition(lightX, lightY);
    } else {
        Point new_position = GetPositionAlongPathNormalized(&sceneSettings.bezierPath, t_param);
        *lightX = new_position.x;
        *lightY = new_position.y;
    }
}

static void UpdateCameraPosition(double t) {
    if (animSettings.interactiveMode) {
        return;
    }
    if (sceneSettings.cameraPath.numPoints < 1) {
        return;
    }
    Point p = (sceneSettings.cameraPath.numPoints >= 2)
                  ? GetPositionAlongPathNormalized(&sceneSettings.cameraPath, t)
                  : sceneSettings.cameraPath.points[0];
    double rot = (sceneSettings.cameraPath.numPoints >= 2)
                     ? GetRotationAlongPathNormalized(&sceneSettings.cameraPath, t)
                     : sceneSettings.cameraPath.rotations[0];
    sceneSettings.camera.x = p.x;
    sceneSettings.camera.y = p.y;
    sceneSettings.camera.rotation = rot;
}

static void DrawPreviewMarker(SDL_Renderer* r, Point world, SDL_Color col, int radius) {
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    CameraPoint s = CameraWorldToScreen(&sceneSettings.camera, world.x, world.y, sceneSettings.windowWidth, sceneSettings.windowHeight);
    for (int dx = -radius; dx <= radius; dx++) {
        for (int dy = -radius; dy <= radius; dy++) {
            if (dx * dx + dy * dy <= radius * radius) {
                SDL_RenderDrawPoint(r, (int)lround(s.x) + dx, (int)lround(s.y) + dy);
            }
        }
    }
}

static void RunPreviewInternal(bool standalone) {
    bool didInit = false;
    if (standalone) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "SDL_Init Error (preview): %s\n", SDL_GetError());
            return;
        }
        didInit = true;
    }

    WINDOW_WIDTH = sceneSettings.windowWidth;
    WINDOW_HEIGHT = sceneSettings.windowHeight;

    SDL_Window* pWindow = SDL_CreateWindow("Preview",
                                           SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED,
                                           WINDOW_WIDTH, WINDOW_HEIGHT,
                                           SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    if (!pWindow) {
        fprintf(stderr, "SDL_CreateWindow Error (preview): %s\n", SDL_GetError());
        if (didInit) SDL_Quit();
        return;
    }

#if USE_VULKAN
    VkRendererConfig preview_cfg;
    vk_renderer_config_set_defaults(&preview_cfg);
    preview_cfg.enable_validation = SDL_FALSE;
    preview_cfg.clear_color[0] = 0.0f;
    preview_cfg.clear_color[1] = 0.0f;
    preview_cfg.clear_color[2] = 0.0f;
    preview_cfg.clear_color[3] = 1.0f;

    if (!vk_shared_device_init(pWindow, &preview_cfg)) {
        fprintf(stderr, "vk_shared_device_init failed (preview).\n");
        SDL_DestroyWindow(pWindow);
        if (didInit) SDL_Quit();
        return;
    }

    VkRendererDevice* shared_device = vk_shared_device_get();
    if (!shared_device) {
        fprintf(stderr, "vk_shared_device_get failed (preview).\n");
        SDL_DestroyWindow(pWindow);
        if (didInit) SDL_Quit();
        return;
    }

    VkRenderer preview_storage;
    VkResult preview_init = vk_renderer_init_with_device(&preview_storage, shared_device, pWindow, &preview_cfg);
    if (preview_init != VK_SUCCESS) {
        fprintf(stderr, "vk_renderer_init failed (preview): %d\n", preview_init);
        SDL_DestroyWindow(pWindow);
        if (didInit) SDL_Quit();
        return;
    }
    SDL_Renderer* pRenderer = (SDL_Renderer*)&preview_storage;
    vk_renderer_set_logical_size((VkRenderer*)pRenderer, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
#else
    SDL_Renderer* pRenderer = SDL_CreateRenderer(pWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!pRenderer) {
        fprintf(stderr, "SDL_CreateRenderer Error (preview): %s\n", SDL_GetError());
        SDL_DestroyWindow(pWindow);
        if (didInit) SDL_Quit();
        return;
    }
#endif

    double duration = (animSettings.previewDuration > 0.1) ? animSettings.previewDuration : 5.0;
    uint64_t prev_ns = runtime_time_now_ns();
    double elapsed = 0.0;
    bool runningPreview = true;
    Camera savedCam = sceneSettings.camera;

    while (runningPreview && !quitRequested) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT ||
                (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
                runningPreview = false;
                quitRequested = true;
            }
            if (e.type == SDL_KEYDOWN) {
                HandleFluidOverlayKey(e.key.keysym.sym);
            }
        }

        uint64_t now_ns = runtime_time_now_ns();
        double dt = runtime_time_diff_seconds(now_ns, prev_ns);
        prev_ns = now_ns;
        elapsed += dt;

        double t = fmod(elapsed, duration) / duration;

        // Update camera + light along paths
        Point lightP = (sceneSettings.bezierPath.numPoints >= 2)
                           ? GetPositionAlongPathNormalized(&sceneSettings.bezierPath, t)
                           : sceneSettings.bezierPath.points[0];
        Point camP = (sceneSettings.cameraPath.numPoints >= 2)
                         ? GetPositionAlongPathNormalized(&sceneSettings.cameraPath, t)
                         : sceneSettings.cameraPath.points[0];
        double camRot = (sceneSettings.cameraPath.numPoints >= 2)
                            ? GetRotationAlongPathNormalized(&sceneSettings.cameraPath, t)
                            : sceneSettings.cameraPath.rotations[0];
        sceneSettings.camera.x = camP.x;
        sceneSettings.camera.y = camP.y;
        sceneSettings.camera.rotation = camRot;

        // Render preview
        setRenderContext(pRenderer, pWindow, sceneSettings.windowWidth, sceneSettings.windowHeight);
        render_set_clear_color(pRenderer, (Uint8)kPreviewBg, (Uint8)kPreviewBg, (Uint8)kPreviewBg + 5, 255);
        if (!render_begin_frame()) {
            if (render_device_lost()) {
                runningPreview = false;
            }
            SDL_Delay(10);
            continue;
        }

        SDL_Color pathColor = {90, 120, 90, 180};
        SDL_Color camPathColor = {60, 140, 220, 220};
        SDL_Color selectColor = {255, 255, 160, 255};
        RenderBezierPathCameraStyled(pRenderer, &sceneSettings.bezierPath, false, &sceneSettings.camera, pathColor, (SDL_Color){0,0,0,0}, -1, selectColor, 3);
        RenderBezierPathCameraStyled(pRenderer, &sceneSettings.cameraPath, false, &sceneSettings.camera, camPathColor, (SDL_Color){0,0,0,0}, -1, selectColor, 4);

        SDL_SetRenderDrawColor(pRenderer, 220, 220, 220, 255);
        RenderSceneObjects(pRenderer, !AnimationUseFluidScene());

        DrawPreviewMarker(pRenderer, lightP, (SDL_Color){255, 230, 120, 255}, 6);
        DrawPreviewMarker(pRenderer, camP, (SDL_Color){120, 200, 255, 255}, 6);

        render_end_frame();
    }

    sceneSettings.camera = savedCam;
#if USE_VULKAN
    vk_renderer_wait_idle((VkRenderer*)pRenderer);
    vk_renderer_shutdown_surface((VkRenderer*)pRenderer);
#else
    SDL_DestroyRenderer(pRenderer);
#endif
    SDL_DestroyWindow(pWindow);
    if (didInit) SDL_Quit();
}

void RunPreviewMode(void) {
    RunPreviewInternal(true);
}

void RunPreviewModeEmbedded(void) {
    RunPreviewInternal(false);
}


void RenderFrame(double lightX, double lightY, int* frameCounter, bool* running) {
    if (quitRequested) {
        *running = false;
        return;
    }
    ts_frame_start();
    setRenderContext(renderer, window, sceneSettings.windowWidth, sceneSettings.windowHeight);
    render_set_clear_color(renderer, 0, 0, 0, 255);
    if (!render_begin_frame()) {
        if (render_device_lost()) {
            *running = false;
        }
        ts_frame_end();
        return;
    }
        
    // Render scene objects
    SetLightPosition(lightX, lightY);
    RenderRayTracingScene(renderer);
        
    // Handle deep render mode frame saving
    if (animSettings.deepRenderMode) {
        ts_start_timer("Frame Save");
        SaveFrame((*frameCounter)++);
        ts_stop_timer("Frame Save");
        if (*frameCounter >= animSettings.frameLimit) {
            printf("Deep render mode complete. Final frame saved.\n");
            SDL_Delay(500);
            *running = false;
        }
    }

    ts_render();
    render_end_frame();
    ts_frame_end();
}

void CheckLoopConditions(bool* running, int loopCount, int frameCounter) {
    if (quitRequested) {
        *running = false;
        return;
    }
    if (loopCount >= animSettings.maxLoopCount && strcmp(animSettings.loopMode, "loop") == 0) {
        *running = false;
    }
    
    // Stop when the animation reaches the last frame
    if (!animSettings.interactiveMode && !animSettings.deepRenderMode && t_param >= 1.0) {
        *running = false;
    }
    
    // Stop deep render mode when we reach the frame limit
    if (animSettings.deepRenderMode && frameCounter >= animSettings.frameLimit) {
        *running = false;
        printf("Deep render mode reached frame limit: %d/%d\n", frameCounter, animSettings.frameLimit);
    }
}

void RunMainLoop(void) {
    running = true;
    accumulator = 0.0;
    currentTime = 0.0;
    frameCounter = 0;
    loopCount = 0;
    t_param = 1.0 / animSettings.framesForTravel;
    
    printf("DEBUG: RunMainLoop started with interactiveMode=%d, deepRenderMode=%d\n",
           animSettings.interactiveMode, animSettings.deepRenderMode);
    
    while (running && !quitRequested) {
        HandleEvents(&running);
        if (!animSettings.interactiveMode || animSettings.deepRenderMode) {
            UpdateSimulation(&accumulator, &currentTime, &loopCount);
        }
        
        double lightX, lightY;
        UpdateLightPosition(&lightX, &lightY);
        UpdateCameraPosition(t_param);
        RenderFrame(lightX, lightY, &frameCounter, &running);
        CheckLoopConditions(&running, loopCount, currentTime);

        SDL_Delay(16);  // Prevent CPU overload, ~60FPS
    }
 
    if (animSettings.deepRenderMode && !quitRequested) {
        printf("Deep render mode complete. Press close on the window to exit.\n");
        bool waitingForExit = true;
        SDL_Event event; 
        while (waitingForExit && !quitRequested) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                waitingForExit = false; // return to menu
            } else if (event.type == SDL_KEYDOWN) {
                HandleFluidOverlayKey(event.key.keysym.sym);
            }
        }
        SDL_Delay(10);
    }
    }
    ExportRenderMetricsDatasetIfEnabled();
    CleanupRayTracing();    
#if USE_VULKAN
    vk_renderer_wait_idle((VkRenderer*)renderer);
    vk_renderer_shutdown_surface((VkRenderer*)renderer);
#else
    SDL_DestroyRenderer(renderer);
#endif
    SDL_DestroyWindow(window);
    SDL_Quit();
}


#ifdef MAIN_DRIVER  
static void ParseArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (strcmp(arg, "--fluid-manifest") == 0 && i + 1 < argc) {
            s_fluidManifestOverride = argv[++i];
        } else if (strcmp(arg, "--fluid-frame") == 0 && i + 1 < argc) {
            g_fluidFrameIndex = atoi(argv[++i]);
        }
    }
}

int main(int argc, char* argv[]) {
    ParseArgs(argc, argv);
    (void)argc;
    (void)argv;
    // Load animation settings from config file
    LoadAllSettings();
    t_increment = 1.0 / animSettings.framesForTravel;
    printf("Loaded animation config in main.\n");

    // Menu → run loop, allowing return to menu after each run
    while (!quitRequested) {
        if (!RunMenu()) {
            printf("Menu closed. Exiting program.\n");
            return 0;
        }
        if (animSettings.previewMode) {
            SaveAllSettings();
            RunPreviewMode();
            animSettings.previewMode = false; // do not persist
            SaveAllSettings();
            continue; // back to menu for next choice
        }

        // Print selected settings
        printf("Selected Mode: %s\n",
               animSettings.interactiveMode ? "Interactive" :
               animSettings.deepRenderMode ? "Deep Render" :
               animSettings.bounceMode ? "Bounce Animation" : "Standard Animation");
        printf("Auto MP4 after render: %s\n", animSettings.autoMP4 ? "Enabled" : "Disabled");
        printf("Saving frames in directory: %s\n", animSettings.frameDir);

        // Initialize animation
        if (AnimationInit() != 0) {
            printf("Error: Animation initialization failed. Exiting program.\n");
            return -1;
        }

        t_increment = 1.0 / animSettings.framesForTravel;

        printf("Starting animation loop...\n");
        RunMainLoop();

        if (animSettings.autoMP4 && animSettings.deepRenderMode) {
            printf("Generating MP4 automatically...\n");
            MakeVideo("output.mp4");
        }

        AnimationCleanup();
        SaveAllSettings();

        // If run ended without a quit request, drop back to the menu
        if (quitRequested) {
            break;
        }
    }

    return 0;
}
#endif

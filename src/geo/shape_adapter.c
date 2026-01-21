#include "geo/shape_adapter.h"
#include "config/config_manager.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int clamp_points_copy(const ShapeAssetPath* path, double (*dst)[2]) {
    if (!path || !dst) return 0;
    int count = (int)path->point_count;
    if (count > MAX_POINTS) {
        double step = (double)(path->point_count - 1) / (double)(MAX_POINTS - 1);
        int idx = 0;
        for (int i = 0; i < MAX_POINTS; ++i) {
            size_t src = (size_t)lrint((double)i * step);
            if (src >= path->point_count) src = path->point_count - 1;
            dst[idx][0] = path->points[src].x;
            dst[idx][1] = path->points[src].y;
            ++idx;
        }
        count = MAX_POINTS;
    } else {
        for (int i = 0; i < count; ++i) {
            dst[i][0] = path->points[i].x;
            dst[i][1] = path->points[i].y;
        }
    }
    return count;
}

static void smooth_closed_polygon(double (*pts)[2], int *count) {
    if (!pts || !count) return;
    int n = *count;
    if (n < 3) return;
    int iterations = 0;
    double temp[MAX_POINTS][2];
    while (iterations < 2) {
        int newCount = n * 2;
        if (newCount > MAX_POINTS) break;
        for (int i = 0; i < n; ++i) {
            int j = (i + 1) % n;
            double x0 = pts[i][0], y0 = pts[i][1];
            double x1 = pts[j][0], y1 = pts[j][1];
            temp[i * 2][0]     = 0.75 * x0 + 0.25 * x1;
            temp[i * 2][1]     = 0.75 * y0 + 0.25 * y1;
            temp[i * 2 + 1][0] = 0.25 * x0 + 0.75 * x1;
            temp[i * 2 + 1][1] = 0.25 * y0 + 0.75 * y1;
        }
        for (int k = 0; k < newCount; ++k) {
            pts[k][0] = temp[k][0];
            pts[k][1] = temp[k][1];
        }
        n = newCount;
        iterations++;
    }
    *count = n;
}

static void apply_opts(SceneObject* obj, const ShapeToSceneOptions* opts) {
    if (!obj || !opts) return;
    obj->scale = opts->scale;
    obj->x += opts->offset_x;
    obj->y += opts->offset_y;
    MarkObjectDirty(obj);
}

bool shape_asset_to_scene_objects(const ShapeAsset* asset,
                                  SceneObject* out_objects,
                                  int max_objects,
                                  int* out_count,
                                  const ShapeToSceneOptions* opts) {
    if (!asset || !out_objects || max_objects <= 0) return false;

    int emitted = 0;
    for (size_t pi = 0; pi < asset->path_count && emitted < max_objects; ++pi) {
        const ShapeAssetPath* path = &asset->paths[pi];
        if (!path->closed || path->point_count < 3) continue;  // skip open or degenerate

        double points[MAX_POINTS][2] = {0};
        int count = clamp_points_copy(path, points);
        if (count < 3) continue;

        SceneObject* obj = &out_objects[emitted];
        memset(obj, 0, sizeof(SceneObject));
        InitObject(obj, OBJECT_POLYGON, 0.0, 0.0, 0.0, 0.0, points, count);
        smooth_closed_polygon(obj->baseShapePoints, &obj->numPoints);
        apply_opts(obj, opts);
        emitted++;
    }

    if (out_count) *out_count = emitted;
    return emitted > 0;
}

bool shape_asset_append_to_scene(const ShapeAsset* asset,
                                 const ShapeToSceneOptions* opts) {
    if (!asset) return false;
    int max_slots = MAX_OBJECTS - sceneSettings.objectCount;
    if (max_slots <= 0) {
        printf("shape_adapter: scene object limit reached\n");
        return false;
    }

    SceneObject temp[MAX_OBJECTS];
    int emitted = 0;
    if (!shape_asset_to_scene_objects(asset, temp, max_slots, &emitted, opts)) {
        return false;
    }

    for (int i = 0; i < emitted; ++i) {
        sceneSettings.sceneObjects[sceneSettings.objectCount + i] = temp[i];
        sceneSettings.sceneObjects[sceneSettings.objectCount + i].dirty = true;
    }
    sceneSettings.objectCount += emitted;
    return emitted > 0;
}

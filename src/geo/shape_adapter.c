#include "geo/shape_adapter.h"
#include "config/config_manager.h"
#include <stdio.h>
#include <string.h>

static int clamp_points_copy(const ShapeAssetPath* path, double (*dst)[2]) {
    if (!path || !dst) return 0;
    int count = (int)path->point_count;
    if (count > MAX_POINTS) {
        int stride = (int)((path->point_count + MAX_POINTS - 1) / MAX_POINTS);
        if (stride < 1) stride = 1;
        int idx = 0;
        for (size_t i = 0; i < path->point_count && idx < MAX_POINTS; i += (size_t)stride, ++idx) {
            dst[idx][0] = path->points[i].x;
            dst[idx][1] = path->points[i].y;
        }
        count = idx;
    } else {
        for (int i = 0; i < count; ++i) {
            dst[i][0] = path->points[i].x;
            dst[i][1] = path->points[i].y;
        }
    }
    return count;
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

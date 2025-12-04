#ifndef GEO_SHAPE_ADAPTER_H
#define GEO_SHAPE_ADAPTER_H

#include <stdbool.h>
#include "geo/shape_asset.h"
#include "scene/object_manager.h"

typedef struct {
    double scale;
    double offset_x;
    double offset_y;
} ShapeToSceneOptions;

// Convert a ShapeAsset into SceneObjects (polygons only). Open paths are skipped.
// Clamps points to MAX_POINTS; returns false if no objects emitted.
bool shape_asset_to_scene_objects(const ShapeAsset* asset,
                                  SceneObject* out_objects,
                                  int max_objects,
                                  int* out_count,
                                  const ShapeToSceneOptions* opts);

// Convenience: append converted objects directly into sceneSettings.sceneObjects.
bool shape_asset_append_to_scene(const ShapeAsset* asset,
                                 const ShapeToSceneOptions* opts);

#endif // GEO_SHAPE_ADAPTER_H

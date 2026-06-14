#ifndef PREVIEW_RETAINED_SCENE_MESH_H
#define PREVIEW_RETAINED_SCENE_MESH_H

#include <stdbool.h>
#include <stddef.h>

#include "app/preview_camera_projector.h"
#include "app/preview_retained_scene_renderer.h"
#include "import/runtime_scene_bridge.h"

bool PreviewRetainedSceneMeshShouldBuildSilhouetteForTriangleCount(size_t triangle_count);

bool PreviewRetainedSceneMeshResolveExtents(bool* seeded,
                                            double* min_x,
                                            double* min_y,
                                            double* min_z,
                                            double* max_x,
                                            double* max_y,
                                            double* max_z);

void PreviewRetainedSceneMeshAppendEdges(PreviewRetainedSceneLineSegment* segments,
                                         int max_segments,
                                         int* io_count);

int PreviewRetainedSceneBuildSilhouetteSegments(
    const RuntimeSceneBridge3DDigestState* digest,
    const PreviewCameraProjector* projector,
    PreviewRetainedSceneLineSegment* out_segments,
    int max_segments);

#endif

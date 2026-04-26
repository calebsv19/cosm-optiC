#ifndef RENDER_RUNTIME_SCENE_3D_BUILDER_H
#define RENDER_RUNTIME_SCENE_3D_BUILDER_H

#include <stdbool.h>

#include "import/runtime_scene_bridge.h"
#include "render/runtime_scene_3d.h"

bool RuntimeScene3DBuilder_BuildFromPrimitiveSeedState(
    RuntimeScene3D* scene,
    const RuntimeSceneBridge3DPrimitiveSeedState* seed_state);
bool RuntimeScene3DBuilder_BuildFromPrimitiveSeedStateAtT(
    RuntimeScene3D* scene,
    const RuntimeSceneBridge3DPrimitiveSeedState* seed_state,
    double normalized_t);
bool RuntimeScene3DBuilder_BuildFromBridgeSeeds(RuntimeScene3D* scene);
bool RuntimeScene3DBuilder_BuildFromBridgeSeedsAtT(RuntimeScene3D* scene, double normalized_t);

#endif

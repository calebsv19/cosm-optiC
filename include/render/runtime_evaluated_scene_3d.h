#ifndef RENDER_RUNTIME_EVALUATED_SCENE_3D_H
#define RENDER_RUNTIME_EVALUATED_SCENE_3D_H

#include <stdbool.h>

#include "animation/evaluated_scene_snapshot.h"
#include "render/runtime_scene_3d.h"

bool RuntimeEvaluatedScene3DApply(
    RuntimeScene3D* copied_scene,
    const RayEvaluatedSceneSnapshot* snapshot);

#endif

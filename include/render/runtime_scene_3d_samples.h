#ifndef RENDER_RUNTIME_SCENE_3D_SAMPLES_H
#define RENDER_RUNTIME_SCENE_3D_SAMPLES_H

#include <stdbool.h>

#include "render/runtime_scene_3d.h"

bool RuntimeScene3DSampleAuthoredLight(double normalized_t, RuntimeLight3D* out_light);
bool RuntimeScene3DSampleAuthoredCamera(double normalized_t, RuntimeCamera3D* out_camera);

#endif

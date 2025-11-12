#ifndef RENDER_IRRADIANCE_CACHE_H
#define RENDER_IRRADIANCE_CACHE_H

#include "render/integrator_common.h"

bool IrradianceCacheFill(IrradianceCache* cache,
                         const SceneObject* objects,
                         int objectCount,
                         const LightSource* light,
                         const UniformGrid* grid,
                         const float* energyBuffer,
                         int width,
                         int height,
                         double maxEnergy);

#endif

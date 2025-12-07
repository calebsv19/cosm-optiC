#ifndef RENDER_DIRECT_LIGHT_INTEGRATOR_H
#define RENDER_DIRECT_LIGHT_INTEGRATOR_H

#include "render/integrator_common.h"

// Single-pass direct lighting: per-pixel LOS test to the light, no bounces.
void DirectLightIntegratorRender(IntegratorContext* ctx, const LightSource* light);

#endif

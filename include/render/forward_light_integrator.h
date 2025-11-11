#ifndef RENDER_FORWARD_LIGHT_INTEGRATOR_H
#define RENDER_FORWARD_LIGHT_INTEGRATOR_H

#include "render/integrator_common.h"

void ForwardLightIntegratorRender(IntegratorContext* ctx,
                                  const LightSource* light);

#endif

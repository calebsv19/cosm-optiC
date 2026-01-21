#ifndef RENDER_LIGHT_PDF_H
#define RENDER_LIGHT_PDF_H

#include "render/integrator_common.h"

static inline double CircleLightPdfSolidAngle(const LightSource* light,
                                              double hitPx,
                                              double hitPy,
                                              double lightHeight) {
    if (!light) return 0.0;
    double dx = light->x - hitPx;
    double dy = light->y - hitPy;
    double dist2 = dx*dx + dy*dy + lightHeight * lightHeight;
    double area = M_PI * fmax(light->radius, 1e-6) * fmax(light->radius, 1e-6);
    return dist2 / area;
}

#endif

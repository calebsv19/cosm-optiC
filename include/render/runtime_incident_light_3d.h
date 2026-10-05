#ifndef RENDER_RUNTIME_INCIDENT_LIGHT_3D_H
#define RENDER_RUNTIME_INCIDENT_LIGHT_3D_H
#include <fisics/extensions.h>

#include "render/runtime_scene_3d.h"
#include "render/runtime_native_3d_sampling.h"
/* Incident lighting only. No receiving-surface normal or BRDF is evaluated. */
typedef struct {
    Vec3 position;
    Vec3 direction;
    double distance FISICS_DIM(length) FISICS_UNIT(meter);
    Vec3 rgb_weight;
    int target_object_index;
    int target_triangle_index;
} RuntimeIncidentLight3DSample;
bool RuntimeIncidentLight3D_Sample(const RuntimeLightSource3D* source,
    Vec3 receiver_position, int sample_index,
    const RuntimeNative3DSamplingContext* sampling,
    RuntimeIncidentLight3DSample* out_sample);
/* Zero for disabled/unqualified sources; four rectangle strata, one point or
 * four legacy sphere-proxy samples. Physical radiometry is rectangle-only. */
int RuntimeIncidentLight3D_SampleCount(const RuntimeLightSource3D* source);
#endif

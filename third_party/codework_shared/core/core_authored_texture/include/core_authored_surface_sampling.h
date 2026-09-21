#ifndef CORE_AUTHORED_SURFACE_SAMPLING_H
#define CORE_AUTHORED_SURFACE_SAMPLING_H
#include <stdbool.h>
#include <stddef.h>
/* Linear/data samples only. Hosts decode resources and own preparation lifetime.
 * Power-of-two dimensions, <=1024, 1..8 channels. Repeat addressing, trilinear
 * conservative major-axis LOD. Immutable prepared data are safe for workers. */
typedef struct CoreAuthoredSurfacePyramid {
    unsigned width, height, channels, levels;
    size_t offsets[11], value_count;
    float *values;
} CoreAuthoredSurfacePyramid;
void core_authored_surface_pyramid_free(CoreAuthoredSurfacePyramid *p);
bool core_authored_surface_pyramid_build(CoreAuthoredSurfacePyramid *p, unsigned width,
                                         unsigned height, unsigned channels,
                                         const float *linear_data);
bool core_authored_surface_pyramid_sample(const CoreAuthoredSurfacePyramid *p, const double uv[2],
                                          const double dx[2], const double dy[2], double *values,
                                          double *lod);
double core_authored_surface_srgb_to_linear(double encoded);
/* Input vectors are world-space; positive sign means B=cross(N,T). */
bool core_authored_surface_normal(const double normal[3], const double tangent[3],
                                  double handedness, const double tangent_normal[3], double out[3]);
/* dP/du and dP/dv are world meters per texture repeat. Height gradients are
 * meters per repeat. This changes shading only, never geometry. */
bool core_authored_surface_bump(const double normal[3], const double dpdu[3], const double dpdv[3],
                                const double height_gradient[2], double out[3]);
#endif

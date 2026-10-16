#ifndef RUNTIME_GLASS_PATH_TERMINATION_3D_H
#define RUNTIME_GLASS_PATH_TERMINATION_3D_H
#include "render/runtime_path_depth_policy_3d.h"
#include "render/runtime_native_3d_sampling.h"
#include "math/vec3.h"
/* Explicit-glass macro paths: zero disables a lobe; a positive limit gives
 * an exact prefix followed by compensated roulette, not black truncation. */
double RuntimeGlassPathTermination3D_Survival(int prefix_depth, int next_depth,
                                              Vec3 beta, double eta_scale);
bool RuntimeGlassPathTermination3D_Apply(double survival, double sample, Vec3 *beta);
/* Full-range, domain-separated roulette stream. Do not reuse the fixed sample
 * stratum of Stratified2D for conditional survival decisions. */
double RuntimeGlassPathTermination3D_Sample(const RuntimeNative3DSamplingContext *,
                                            uint32_t path_seed, int sample_index,
                                            uint32_t boundary);
#endif

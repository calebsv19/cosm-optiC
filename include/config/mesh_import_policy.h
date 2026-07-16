#ifndef RAY_TRACING_MESH_IMPORT_POLICY_H
#define RAY_TRACING_MESH_IMPORT_POLICY_H

#include <stddef.h>

#include "config/config_manager.h"

/* Defaults for the next imported-mesh compile only; existing sidecars stay unchanged. */
int ray_tracing_mesh_import_normal_mode_clamp(int mode);
double ray_tracing_mesh_import_crease_angle_clamp(double degrees);
void ray_tracing_mesh_import_policy_normalize(AnimationConfig *config);
void ray_tracing_mesh_import_policy_format_label(const AnimationConfig *config,
                                                 char *out,
                                                 size_t out_size);
const char *ray_tracing_mesh_import_normal_mode_name(int mode);

#endif

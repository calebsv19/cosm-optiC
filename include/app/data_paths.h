#ifndef RAY_TRACING_APP_DATA_PATHS_H
#define RAY_TRACING_APP_DATA_PATHS_H

#include <stddef.h>
#include <stdbool.h>

const char *ray_tracing_default_input_root(void);
const char *ray_tracing_default_output_root(void);
const char *ray_tracing_default_shape_asset_dir(void);
const char *ray_tracing_default_import_dir(void);
const char *ray_tracing_default_frame_root(void);
const char *ray_tracing_default_frame_dir(void);
const char *ray_tracing_default_video_output_path(void);

const char *ray_tracing_env_input_root(void);
const char *ray_tracing_env_output_root(void);

bool ray_tracing_compose_path(const char *root,
                              const char *leaf,
                              char *out,
                              size_t out_size);

const char *ray_tracing_resolve_shape_asset_dir(const char *shape_asset_env,
                                                char *out,
                                                size_t out_size);
const char *ray_tracing_resolve_import_dir(char *out, size_t out_size);

size_t ray_tracing_manifest_default_roots(const char ***out_roots);

#endif

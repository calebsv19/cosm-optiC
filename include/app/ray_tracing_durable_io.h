#ifndef RAY_TRACING_DURABLE_IO_H
#define RAY_TRACING_DURABLE_IO_H

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct RayTracingDurableOutput {
    FILE *stream;
    char target_path[PATH_MAX];
    char temporary_path[PATH_MAX];
} RayTracingDurableOutput;

bool ray_tracing_durable_output_begin(RayTracingDurableOutput *output,
                                      const char *target_path);
bool ray_tracing_durable_output_commit(RayTracingDurableOutput *output);
void ray_tracing_durable_output_abort(RayTracingDurableOutput *output);

bool ray_tracing_durable_prepare_external_write(const char *target_path,
                                                char *out_temporary_path,
                                                size_t out_temporary_path_size);
bool ray_tracing_durable_commit_external_write(const char *temporary_path,
                                               const char *target_path);

bool ray_tracing_durable_validate_bmp(const char *path,
                                      int expected_width,
                                      int expected_height);

#endif

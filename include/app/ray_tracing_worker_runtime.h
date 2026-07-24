#ifndef RAY_TRACING_WORKER_RUNTIME_H
#define RAY_TRACING_WORKER_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>

bool ray_tracing_worker_runtime_write_capabilities(const char *render_cli_path,
                                                   const char *output_path,
                                                   char *diagnostics,
                                                   size_t diagnostics_size);
int ray_tracing_worker_runtime_run(const char *request_message_path,
                                   char *diagnostics,
                                   size_t diagnostics_size);

#endif

#pragma once

typedef enum {
    RAY_TRACING_PATH_OPENER_OPENED = 0,
    RAY_TRACING_PATH_OPENER_INVALID_PATH,
    RAY_TRACING_PATH_OPENER_UNAVAILABLE,
    RAY_TRACING_PATH_OPENER_FAILED
} RayTracingPathOpenerResult;

/* Opens an existing directory in the host file manager without a shell. */
RayTracingPathOpenerResult RayTracing_PathOpener_OpenDirectory(const char *path);

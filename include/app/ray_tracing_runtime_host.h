#ifndef RAY_TRACING_APP_RUNTIME_HOST_H
#define RAY_TRACING_APP_RUNTIME_HOST_H

#include <stdbool.h>

typedef struct RayTracingRuntimeHostSnapshot {
    bool sdl_ready;
    bool window_ready;
    bool shared_device_ready;
    bool renderer_ready;
    bool render_context_ready;
    bool font_runtime_attached;
    bool font_system_ready;
    bool timer_hud_session_ready;
    bool any_resource_ready;
    char last_failure_stage[64];
    char last_failure_detail[256];
} RayTracingRuntimeHostSnapshot;

int ray_tracing_runtime_host_init(int window_width, int window_height);
void ray_tracing_runtime_host_shutdown(void);
void ray_tracing_runtime_host_snapshot(RayTracingRuntimeHostSnapshot *out_snapshot);
bool ray_tracing_runtime_host_is_clean(void);
const char *ray_tracing_runtime_host_last_failure_stage(void);
const char *ray_tracing_runtime_host_last_failure_detail(void);

#endif

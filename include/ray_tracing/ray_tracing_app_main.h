#ifndef RAY_TRACING_RAY_TRACING_APP_MAIN_H
#define RAY_TRACING_RAY_TRACING_APP_MAIN_H

#include <stdbool.h>

typedef bool (*RayTracingRenderSubmitFn)(void *user_data);
typedef bool (*RayTracingFrameUpdateFn)(void *user_data);
typedef bool (*RayTracingFrameRouteFn)(void *user_data);
typedef bool (*RayTracingFrameEventsFn)(void *user_data);

typedef struct RayTracingRenderSubmitRequest {
    RayTracingRenderSubmitFn submit_fn;
    void *user_data;
} RayTracingRenderSubmitRequest;

typedef struct RayTracingRenderSubmitOutcome {
    bool accepted_by_wrapper;
    bool submitted;
} RayTracingRenderSubmitOutcome;

typedef struct RayTracingFrameUpdateRequest {
    RayTracingFrameUpdateFn update_fn;
    void *user_data;
} RayTracingFrameUpdateRequest;

typedef struct RayTracingFrameUpdateOutcome {
    bool accepted_by_wrapper;
    bool updated;
} RayTracingFrameUpdateOutcome;

typedef struct RayTracingFrameRouteRequest {
    RayTracingFrameRouteFn route_fn;
    void *user_data;
} RayTracingFrameRouteRequest;

typedef struct RayTracingFrameRouteOutcome {
    bool accepted_by_wrapper;
    bool routed;
} RayTracingFrameRouteOutcome;

typedef struct RayTracingFrameEventsRequest {
    RayTracingFrameEventsFn events_fn;
    void *user_data;
} RayTracingFrameEventsRequest;

typedef struct RayTracingFrameEventsOutcome {
    bool accepted_by_wrapper;
    bool handled;
} RayTracingFrameEventsOutcome;

bool ray_tracing_app_bootstrap(void);
bool ray_tracing_app_config_load(void);
bool ray_tracing_app_state_seed(void);
bool ray_tracing_app_subsystems_init(void);
bool ray_tracing_runtime_start(void);
void ray_tracing_app_set_legacy_entry(int (*legacy_entry)(int argc, char **argv));
int ray_tracing_app_run_loop(void);
void ray_tracing_app_shutdown(void);
bool ray_tracing_app_frame_update(const RayTracingFrameUpdateRequest *request,
                                  RayTracingFrameUpdateOutcome *outcome);
bool ray_tracing_app_frame_route(const RayTracingFrameRouteRequest *request,
                                 RayTracingFrameRouteOutcome *outcome);
bool ray_tracing_app_frame_events(const RayTracingFrameEventsRequest *request,
                                  RayTracingFrameEventsOutcome *outcome);
bool ray_tracing_app_render_submit(const RayTracingRenderSubmitRequest *request,
                                   RayTracingRenderSubmitOutcome *outcome);

int ray_tracing_app_main(int argc, char **argv);

#endif

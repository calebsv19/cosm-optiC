#ifndef RAY_TRACING_CORE_SIM_RUNTIME_FRAME_H
#define RAY_TRACING_CORE_SIM_RUNTIME_FRAME_H

#include <stdbool.h>

#include "core_sim.h"
#include "ray_tracing/ray_tracing_app_main.h"

typedef enum RayTracingCoreSimRuntimeFramePass {
    RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_EVENTS = 1u,
    RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_UPDATE = 2u,
    RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_ROUTE = 3u,
    RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_SUBMIT = 4u,
    RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_LOOP_CONDITIONS = 5u
} RayTracingCoreSimRuntimeFramePass;

typedef double (*RayTracingCoreSimNowSecondsFn)(void *user_data);
typedef bool (*RayTracingCoreSimFrameFn)(void *user_data);

typedef struct RayTracingCoreSimLoopConditionsRequest {
    RayTracingCoreSimFrameFn check_fn;
    void *user_data;
} RayTracingCoreSimLoopConditionsRequest;

typedef struct RayTracingCoreSimRuntimeFrameStageMarks {
    double frame_begin;
    double after_events;
    double after_update;
    double after_route;
    double after_render_derive;
    double before_present;
    double after_render;
} RayTracingCoreSimRuntimeFrameStageMarks;

typedef struct RayTracingCoreSimRuntimeFrameRequest {
    double frame_dt_seconds;
    RayTracingCoreSimNowSecondsFn now_seconds_fn;
    void *now_user_data;
    RayTracingFrameEventsRequest events_request;
    RayTracingFrameUpdateRequest update_request;
    RayTracingFrameRouteRequest route_request;
    RayTracingRenderSubmitRequest submit_request;
    RayTracingCoreSimLoopConditionsRequest loop_conditions_request;
} RayTracingCoreSimRuntimeFrameRequest;

typedef struct RayTracingCoreSimRuntimeFrameResult {
    CoreSimFrameOutcome sim_outcome;
    RayTracingFrameEventsOutcome events_outcome;
    RayTracingFrameUpdateOutcome update_outcome;
    RayTracingFrameRouteOutcome route_outcome;
    RayTracingRenderSubmitOutcome submit_outcome;
    bool loop_conditions_checked;
    RayTracingCoreSimRuntimeFrameStageMarks stage_marks;
} RayTracingCoreSimRuntimeFrameResult;

bool ray_tracing_core_sim_runtime_frame_loop_init(CoreSimLoopState *loop_state);

void ray_tracing_core_sim_runtime_frame_set_paused(CoreSimLoopState *loop_state,
                                                   bool paused);

bool ray_tracing_core_sim_runtime_frame_step(
    CoreSimLoopState *loop_state,
    const RayTracingCoreSimRuntimeFrameRequest *request,
    RayTracingCoreSimRuntimeFrameResult *result);

#endif

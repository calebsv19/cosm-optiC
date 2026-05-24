#include "ray_tracing/ray_tracing_app_main.h"

#include "app/animation.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef enum RayTracingAppStage {
    RAY_TRACING_APP_STAGE_INIT = 0,
    RAY_TRACING_APP_STAGE_BOOTSTRAPPED,
    RAY_TRACING_APP_STAGE_CONFIG_LOADED,
    RAY_TRACING_APP_STAGE_STATE_SEEDED,
    RAY_TRACING_APP_STAGE_SUBSYSTEMS_READY,
    RAY_TRACING_APP_STAGE_RUNTIME_STARTED,
    RAY_TRACING_APP_STAGE_LOOP_COMPLETED,
    RAY_TRACING_APP_STAGE_SHUTDOWN_COMPLETED
} RayTracingAppStage;

typedef struct RayTracingLaunchArgs {
    int argc;
    char **argv;
} RayTracingLaunchArgs;

typedef struct RayTracingDispatchSummary {
    uint32_t dispatch_count;
    bool dispatch_succeeded;
    int last_dispatch_exit_code;
} RayTracingDispatchSummary;

typedef struct RayTracingRenderHandoffSummary {
    uint32_t events_attempt_count;
    uint32_t events_success_count;
    uint32_t events_rejected_count;
    uint32_t update_attempt_count;
    uint32_t update_success_count;
    uint32_t update_rejected_count;
    uint32_t route_attempt_count;
    uint32_t route_success_count;
    uint32_t route_rejected_count;
    uint32_t submit_attempt_count;
    uint32_t submit_success_count;
    uint32_t submit_rejected_count;
} RayTracingRenderHandoffSummary;

typedef struct RayTracingLifecycleOwnership {
    bool bootstrap_owned;
    bool config_owned;
    bool state_seed_owned;
    bool subsystems_owned;
    bool runtime_owned;
    bool dispatch_owned;
    bool shutdown_owned;
} RayTracingLifecycleOwnership;

typedef struct RayTracingAppContext {
    RayTracingAppStage stage;
    bool bootstrapped;
    bool config_loaded;
    bool state_seeded;
    bool subsystems_initialized;
    bool runtime_started;
    bool run_loop_completed;
    bool shutdown_completed;
    int exit_code;
    RayTracingLaunchArgs launch_args;
    RayTracingDispatchSummary dispatch_summary;
    RayTracingRenderHandoffSummary render_handoff_summary;
    RayTracingLifecycleOwnership ownership;
    int wrapper_error;
} RayTracingAppContext;

typedef enum RayTracingWrapperError {
    RAY_TRACING_WRAPPER_ERROR_NONE = 0,
    RAY_TRACING_WRAPPER_ERROR_BOOTSTRAP_FAILED = 1,
    RAY_TRACING_WRAPPER_ERROR_CONFIG_LOAD_FAILED = 2,
    RAY_TRACING_WRAPPER_ERROR_STATE_SEED_FAILED = 3,
    RAY_TRACING_WRAPPER_ERROR_SUBSYSTEMS_INIT_FAILED = 4,
    RAY_TRACING_WRAPPER_ERROR_RUNTIME_START_FAILED = 5,
    RAY_TRACING_WRAPPER_ERROR_RUN_LOOP_FAILED = 6
} RayTracingWrapperError;

static void ray_tracing_log_wrapper_error(const char *fn_name,
                                          RayTracingWrapperError wrapper_error,
                                          RayTracingAppStage stage,
                                          int exit_code,
                                          const char *detail) {
    fprintf(stderr,
            "ray_tracing: wrapper error fn=%s code=%d stage=%d exit_code=%d detail=%s\n",
            fn_name ? fn_name : "unknown",
            (int)wrapper_error,
            (int)stage,
            exit_code,
            detail ? detail : "n/a");
}

static RayTracingAppContext g_ray_tracing_app_ctx = {
    .stage = RAY_TRACING_APP_STAGE_INIT,
    .exit_code = 1,
};

static bool ray_tracing_app_transition_stage(RayTracingAppContext *ctx,
                                             RayTracingAppStage expected,
                                             RayTracingAppStage next,
                                             const char *stage_name,
                                             const char *fn_name) {
    if (!ctx) {
        return false;
    }
    if (ctx->stage != expected) {
        fprintf(stderr,
                "ray_tracing: lifecycle stage order violation fn=%s stage=%s (expected=%d actual=%d next=%d)\n",
                fn_name ? fn_name : "unknown",
                stage_name ? stage_name : "unknown",
                (int)expected,
                (int)ctx->stage,
                (int)next);
        return false;
    }
    ctx->stage = next;
    return true;
}

static bool ray_tracing_app_bootstrap_ctx(RayTracingAppContext *ctx) {
    RayTracingLaunchArgs launch_args = {0};

    if (!ctx) {
        return false;
    }
    launch_args = ctx->launch_args;
    memset(ctx, 0, sizeof(*ctx));
    ctx->launch_args = launch_args;
    ctx->stage = RAY_TRACING_APP_STAGE_INIT;
    ctx->exit_code = 1;
    ctx->wrapper_error = RAY_TRACING_WRAPPER_ERROR_NONE;
    ctx->dispatch_summary.last_dispatch_exit_code = 1;

    AnimationParseArgs(ctx->launch_args.argc, ctx->launch_args.argv);

    if (!ray_tracing_app_transition_stage(ctx,
                                          RAY_TRACING_APP_STAGE_INIT,
                                          RAY_TRACING_APP_STAGE_BOOTSTRAPPED,
                                          "ray_tracing_app_bootstrap",
                                          __func__)) {
        return false;
    }
    ctx->bootstrapped = true;
    ctx->ownership.bootstrap_owned = true;
    return true;
}

static bool ray_tracing_app_config_load_ctx(RayTracingAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    AnimationLoadRuntimeDefaults();
    if (!ray_tracing_app_transition_stage(ctx,
                                          RAY_TRACING_APP_STAGE_BOOTSTRAPPED,
                                          RAY_TRACING_APP_STAGE_CONFIG_LOADED,
                                          "ray_tracing_app_config_load",
                                          __func__)) {
        return false;
    }
    ctx->config_loaded = true;
    ctx->ownership.config_owned = true;
    return true;
}

static bool ray_tracing_app_state_seed_ctx(RayTracingAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!ray_tracing_app_transition_stage(ctx,
                                          RAY_TRACING_APP_STAGE_CONFIG_LOADED,
                                          RAY_TRACING_APP_STAGE_STATE_SEEDED,
                                          "ray_tracing_app_state_seed",
                                          __func__)) {
        return false;
    }
    ctx->state_seeded = true;
    ctx->ownership.state_seed_owned = true;
    return true;
}

static bool ray_tracing_app_subsystems_init_ctx(RayTracingAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!ray_tracing_app_transition_stage(ctx,
                                          RAY_TRACING_APP_STAGE_STATE_SEEDED,
                                          RAY_TRACING_APP_STAGE_SUBSYSTEMS_READY,
                                          "ray_tracing_app_subsystems_init",
                                          __func__)) {
        return false;
    }
    ctx->subsystems_initialized = true;
    ctx->ownership.subsystems_owned = true;
    return true;
}

static bool ray_tracing_runtime_start_ctx(RayTracingAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!ray_tracing_app_transition_stage(ctx,
                                          RAY_TRACING_APP_STAGE_SUBSYSTEMS_READY,
                                          RAY_TRACING_APP_STAGE_RUNTIME_STARTED,
                                          "ray_tracing_runtime_start",
                                          __func__)) {
        return false;
    }
    ctx->runtime_started = true;
    ctx->ownership.runtime_owned = true;
    return true;
}

static int ray_tracing_app_run_loop_ctx(RayTracingAppContext *ctx) {
    if (!ctx || ctx->stage != RAY_TRACING_APP_STAGE_RUNTIME_STARTED) {
        if (ctx) {
            ctx->wrapper_error = RAY_TRACING_WRAPPER_ERROR_RUN_LOOP_FAILED;
        }
        return 1;
    }
    ctx->ownership.dispatch_owned = true;
    ctx->dispatch_summary.dispatch_count += 1u;
    ctx->dispatch_summary.last_dispatch_exit_code = AnimationRunAppSession();
    ctx->dispatch_summary.dispatch_succeeded = true;
    ctx->exit_code = ctx->dispatch_summary.last_dispatch_exit_code;

    if (!ray_tracing_app_transition_stage(ctx,
                                          RAY_TRACING_APP_STAGE_RUNTIME_STARTED,
                                          RAY_TRACING_APP_STAGE_LOOP_COMPLETED,
                                          "ray_tracing_app_run_loop",
                                          __func__)) {
        ctx->wrapper_error = RAY_TRACING_WRAPPER_ERROR_RUN_LOOP_FAILED;
        return 1;
    }
    ctx->run_loop_completed = true;
    return ctx->exit_code;
}

static bool ray_tracing_app_render_submit_ctx(
    RayTracingAppContext *ctx,
    const RayTracingRenderSubmitRequest *request,
    RayTracingRenderSubmitOutcome *outcome) {
    bool accepted = false;
    bool submitted = false;

    if (!ctx || !request || !request->submit_fn) {
        return false;
    }
    ctx->render_handoff_summary.submit_attempt_count += 1u;
    accepted = (ctx->stage == RAY_TRACING_APP_STAGE_RUNTIME_STARTED ||
                ctx->stage == RAY_TRACING_APP_STAGE_LOOP_COMPLETED);
    if (!accepted) {
        ctx->render_handoff_summary.submit_rejected_count += 1u;
        if (outcome) {
            memset(outcome, 0, sizeof(*outcome));
            outcome->accepted_by_wrapper = false;
            outcome->submitted = false;
        }
        return false;
    }
    submitted = request->submit_fn(request->user_data);
    if (!submitted) {
        ctx->render_handoff_summary.submit_rejected_count += 1u;
    } else {
        ctx->render_handoff_summary.submit_success_count += 1u;
    }
    if (outcome) {
        memset(outcome, 0, sizeof(*outcome));
        outcome->accepted_by_wrapper = true;
        outcome->submitted = submitted;
    }
    return submitted;
}

static bool ray_tracing_app_frame_events_ctx(
    RayTracingAppContext *ctx,
    const RayTracingFrameEventsRequest *request,
    RayTracingFrameEventsOutcome *outcome) {
    bool accepted = false;
    bool handled = false;

    if (!ctx || !request || !request->events_fn) {
        return false;
    }
    ctx->render_handoff_summary.events_attempt_count += 1u;
    accepted = (ctx->stage == RAY_TRACING_APP_STAGE_RUNTIME_STARTED ||
                ctx->stage == RAY_TRACING_APP_STAGE_LOOP_COMPLETED);
    if (!accepted) {
        ctx->render_handoff_summary.events_rejected_count += 1u;
        if (outcome) {
            memset(outcome, 0, sizeof(*outcome));
            outcome->accepted_by_wrapper = false;
            outcome->handled = false;
        }
        return false;
    }
    handled = request->events_fn(request->user_data);
    if (!handled) {
        ctx->render_handoff_summary.events_rejected_count += 1u;
    } else {
        ctx->render_handoff_summary.events_success_count += 1u;
    }
    if (outcome) {
        memset(outcome, 0, sizeof(*outcome));
        outcome->accepted_by_wrapper = true;
        outcome->handled = handled;
    }
    return handled;
}

static bool ray_tracing_app_frame_update_ctx(
    RayTracingAppContext *ctx,
    const RayTracingFrameUpdateRequest *request,
    RayTracingFrameUpdateOutcome *outcome) {
    bool accepted = false;
    bool updated = false;

    if (!ctx || !request || !request->update_fn) {
        return false;
    }
    ctx->render_handoff_summary.update_attempt_count += 1u;
    accepted = (ctx->stage == RAY_TRACING_APP_STAGE_RUNTIME_STARTED ||
                ctx->stage == RAY_TRACING_APP_STAGE_LOOP_COMPLETED);
    if (!accepted) {
        ctx->render_handoff_summary.update_rejected_count += 1u;
        if (outcome) {
            memset(outcome, 0, sizeof(*outcome));
            outcome->accepted_by_wrapper = false;
            outcome->updated = false;
        }
        return false;
    }
    updated = request->update_fn(request->user_data);
    if (!updated) {
        ctx->render_handoff_summary.update_rejected_count += 1u;
    } else {
        ctx->render_handoff_summary.update_success_count += 1u;
    }
    if (outcome) {
        memset(outcome, 0, sizeof(*outcome));
        outcome->accepted_by_wrapper = true;
        outcome->updated = updated;
    }
    return updated;
}

static bool ray_tracing_app_frame_route_ctx(
    RayTracingAppContext *ctx,
    const RayTracingFrameRouteRequest *request,
    RayTracingFrameRouteOutcome *outcome) {
    bool accepted = false;
    bool routed = false;

    if (!ctx || !request || !request->route_fn) {
        return false;
    }
    ctx->render_handoff_summary.route_attempt_count += 1u;
    accepted = (ctx->stage == RAY_TRACING_APP_STAGE_RUNTIME_STARTED ||
                ctx->stage == RAY_TRACING_APP_STAGE_LOOP_COMPLETED);
    if (!accepted) {
        ctx->render_handoff_summary.route_rejected_count += 1u;
        if (outcome) {
            memset(outcome, 0, sizeof(*outcome));
            outcome->accepted_by_wrapper = false;
            outcome->routed = false;
        }
        return false;
    }
    routed = request->route_fn(request->user_data);
    if (!routed) {
        ctx->render_handoff_summary.route_rejected_count += 1u;
    } else {
        ctx->render_handoff_summary.route_success_count += 1u;
    }
    if (outcome) {
        memset(outcome, 0, sizeof(*outcome));
        outcome->accepted_by_wrapper = true;
        outcome->routed = routed;
    }
    return routed;
}

static void ray_tracing_app_release_lifecycle_ctx(RayTracingAppContext *ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->ownership.dispatch_owned) {
        ctx->ownership.dispatch_owned = false;
    }
    if (ctx->ownership.runtime_owned) {
        ctx->ownership.runtime_owned = false;
    }
    if (ctx->ownership.subsystems_owned) {
        ctx->ownership.subsystems_owned = false;
    }
    if (ctx->ownership.state_seed_owned) {
        ctx->ownership.state_seed_owned = false;
    }
    if (ctx->ownership.config_owned) {
        ctx->ownership.config_owned = false;
    }
    if (ctx->ownership.bootstrap_owned) {
        ctx->ownership.bootstrap_owned = false;
    }
}

static void ray_tracing_app_shutdown_ctx(RayTracingAppContext *ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->stage == RAY_TRACING_APP_STAGE_SHUTDOWN_COMPLETED) {
        return;
    }
    ray_tracing_app_release_lifecycle_ctx(ctx);
    ctx->ownership.shutdown_owned = true;
    ctx->stage = RAY_TRACING_APP_STAGE_SHUTDOWN_COMPLETED;
    ctx->shutdown_completed = true;
}

bool ray_tracing_app_bootstrap(void) {
    return ray_tracing_app_bootstrap_ctx(&g_ray_tracing_app_ctx);
}

bool ray_tracing_app_config_load(void) {
    return ray_tracing_app_config_load_ctx(&g_ray_tracing_app_ctx);
}

bool ray_tracing_app_state_seed(void) {
    return ray_tracing_app_state_seed_ctx(&g_ray_tracing_app_ctx);
}

bool ray_tracing_app_subsystems_init(void) {
    return ray_tracing_app_subsystems_init_ctx(&g_ray_tracing_app_ctx);
}

bool ray_tracing_runtime_start(void) {
    return ray_tracing_runtime_start_ctx(&g_ray_tracing_app_ctx);
}

int ray_tracing_app_run_loop(void) {
    return ray_tracing_app_run_loop_ctx(&g_ray_tracing_app_ctx);
}

void ray_tracing_app_shutdown(void) {
    ray_tracing_app_shutdown_ctx(&g_ray_tracing_app_ctx);
}

bool ray_tracing_app_frame_update(const RayTracingFrameUpdateRequest *request,
                                  RayTracingFrameUpdateOutcome *outcome) {
    return ray_tracing_app_frame_update_ctx(&g_ray_tracing_app_ctx, request, outcome);
}

bool ray_tracing_app_frame_route(const RayTracingFrameRouteRequest *request,
                                 RayTracingFrameRouteOutcome *outcome) {
    return ray_tracing_app_frame_route_ctx(&g_ray_tracing_app_ctx, request, outcome);
}

bool ray_tracing_app_frame_events(const RayTracingFrameEventsRequest *request,
                                  RayTracingFrameEventsOutcome *outcome) {
    return ray_tracing_app_frame_events_ctx(&g_ray_tracing_app_ctx, request, outcome);
}

bool ray_tracing_app_render_submit(const RayTracingRenderSubmitRequest *request,
                                   RayTracingRenderSubmitOutcome *outcome) {
    return ray_tracing_app_render_submit_ctx(&g_ray_tracing_app_ctx, request, outcome);
}

int ray_tracing_app_main(int argc, char **argv) {
    RayTracingAppContext *ctx = &g_ray_tracing_app_ctx;
    int exit_code = 1;

    ctx->launch_args.argc = argc;
    ctx->launch_args.argv = argv;

    if (!ray_tracing_app_bootstrap()) {
        ctx->wrapper_error = RAY_TRACING_WRAPPER_ERROR_BOOTSTRAP_FAILED;
        ray_tracing_log_wrapper_error(__func__,
                                      (RayTracingWrapperError)ctx->wrapper_error,
                                      ctx->stage,
                                      ctx->exit_code,
                                      "bootstrap failed");
        ray_tracing_app_shutdown();
        return 1;
    }
    if (!ray_tracing_app_config_load()) {
        ctx->wrapper_error = RAY_TRACING_WRAPPER_ERROR_CONFIG_LOAD_FAILED;
        ray_tracing_log_wrapper_error(__func__,
                                      (RayTracingWrapperError)ctx->wrapper_error,
                                      ctx->stage,
                                      ctx->exit_code,
                                      "config load failed");
        ray_tracing_app_shutdown();
        return 1;
    }
    if (!ray_tracing_app_state_seed()) {
        ctx->wrapper_error = RAY_TRACING_WRAPPER_ERROR_STATE_SEED_FAILED;
        ray_tracing_log_wrapper_error(__func__,
                                      (RayTracingWrapperError)ctx->wrapper_error,
                                      ctx->stage,
                                      ctx->exit_code,
                                      "state seed failed");
        ray_tracing_app_shutdown();
        return 1;
    }
    if (!ray_tracing_app_subsystems_init()) {
        ctx->wrapper_error = RAY_TRACING_WRAPPER_ERROR_SUBSYSTEMS_INIT_FAILED;
        ray_tracing_log_wrapper_error(__func__,
                                      (RayTracingWrapperError)ctx->wrapper_error,
                                      ctx->stage,
                                      ctx->exit_code,
                                      "subsystems init failed");
        ray_tracing_app_shutdown();
        return 1;
    }
    if (!ray_tracing_runtime_start()) {
        ctx->wrapper_error = RAY_TRACING_WRAPPER_ERROR_RUNTIME_START_FAILED;
        ray_tracing_log_wrapper_error(__func__,
                                      (RayTracingWrapperError)ctx->wrapper_error,
                                      ctx->stage,
                                      ctx->exit_code,
                                      "runtime start failed");
        ray_tracing_app_shutdown();
        return 1;
    }
    exit_code = ray_tracing_app_run_loop();
    if (ctx->wrapper_error != RAY_TRACING_WRAPPER_ERROR_NONE) {
        ray_tracing_log_wrapper_error(__func__,
                                      (RayTracingWrapperError)ctx->wrapper_error,
                                      ctx->stage,
                                      exit_code,
                                      "run loop failed");
    }
    ray_tracing_app_shutdown();
    return exit_code;
}

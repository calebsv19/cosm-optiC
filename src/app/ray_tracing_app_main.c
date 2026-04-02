#include "ray_tracing/ray_tracing_app_main.h"

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
    bool used_legacy_entry;
    bool dispatch_succeeded;
    int last_dispatch_exit_code;
} RayTracingDispatchSummary;

typedef struct RayTracingLifecycleOwnership {
    bool bootstrap_owned;
    bool config_owned;
    bool state_seed_owned;
    bool subsystems_owned;
    bool runtime_owned;
    bool dispatch_owned;
    bool shutdown_owned;
} RayTracingLifecycleOwnership;

typedef struct RayTracingDispatchRequest {
    int argc;
    char **argv;
    int (*legacy_entry)(int argc, char **argv);
} RayTracingDispatchRequest;

typedef struct RayTracingDispatchOutcome {
    bool dispatched;
    bool used_legacy_entry;
    int exit_code;
} RayTracingDispatchOutcome;

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
    int (*legacy_entry)(int argc, char **argv);
    bool (*runtime_dispatch)(const RayTracingDispatchRequest *request,
                             RayTracingDispatchOutcome *outcome);
    RayTracingLaunchArgs launch_args;
    RayTracingDispatchSummary dispatch_summary;
    RayTracingLifecycleOwnership ownership;
} RayTracingAppContext;

static int ray_tracing_default_legacy_entry(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return 1;
}

static bool ray_tracing_default_runtime_dispatch(const RayTracingDispatchRequest *request,
                                                 RayTracingDispatchOutcome *outcome) {
    if (!request || !outcome || !request->legacy_entry) {
        return false;
    }
    outcome->dispatched = true;
    outcome->used_legacy_entry = true;
    outcome->exit_code = request->legacy_entry(request->argc, request->argv);
    return true;
}

static RayTracingAppContext g_ray_tracing_app_ctx = {
    .stage = RAY_TRACING_APP_STAGE_INIT,
    .exit_code = 1,
    .legacy_entry = ray_tracing_default_legacy_entry,
    .runtime_dispatch = ray_tracing_default_runtime_dispatch
};

// Guards stage progression so wrapper lifecycle stays deterministic.
static bool ray_tracing_app_transition_stage(RayTracingAppContext *ctx,
                                             RayTracingAppStage expected,
                                             RayTracingAppStage next) {
    if (!ctx || ctx->stage != expected) {
        return false;
    }
    ctx->stage = next;
    return true;
}

static bool ray_tracing_app_bootstrap_ctx(RayTracingAppContext *ctx) {
    int (*legacy_entry)(int argc, char **argv) = ray_tracing_default_legacy_entry;
    bool (*runtime_dispatch)(const RayTracingDispatchRequest *request,
                             RayTracingDispatchOutcome *outcome) =
        ray_tracing_default_runtime_dispatch;
    RayTracingLaunchArgs launch_args = {0};

    if (!ctx) {
        return false;
    }
    if (ctx->legacy_entry) {
        legacy_entry = ctx->legacy_entry;
    }
    if (ctx->runtime_dispatch) {
        runtime_dispatch = ctx->runtime_dispatch;
    }
    launch_args = ctx->launch_args;

    memset(ctx, 0, sizeof(*ctx));
    ctx->legacy_entry = legacy_entry;
    ctx->runtime_dispatch = runtime_dispatch;
    ctx->launch_args = launch_args;
    ctx->exit_code = 1;

    if (!ray_tracing_app_transition_stage(ctx,
                                          RAY_TRACING_APP_STAGE_INIT,
                                          RAY_TRACING_APP_STAGE_BOOTSTRAPPED)) {
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
    if (!ray_tracing_app_transition_stage(ctx,
                                          RAY_TRACING_APP_STAGE_BOOTSTRAPPED,
                                          RAY_TRACING_APP_STAGE_CONFIG_LOADED)) {
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
                                          RAY_TRACING_APP_STAGE_STATE_SEEDED)) {
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
                                          RAY_TRACING_APP_STAGE_SUBSYSTEMS_READY)) {
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
                                          RAY_TRACING_APP_STAGE_RUNTIME_STARTED)) {
        return false;
    }
    ctx->runtime_started = true;
    ctx->ownership.runtime_owned = true;
    return true;
}

static bool ray_tracing_app_dispatch_prepare_ctx(RayTracingAppContext *ctx,
                                                 RayTracingDispatchRequest *request) {
    if (!ctx || !request || !ctx->legacy_entry || !ctx->runtime_dispatch) {
        return false;
    }
    if (ctx->stage != RAY_TRACING_APP_STAGE_RUNTIME_STARTED) {
        return false;
    }
    memset(request, 0, sizeof(*request));
    request->argc = ctx->launch_args.argc;
    request->argv = ctx->launch_args.argv;
    request->legacy_entry = ctx->legacy_entry;
    ctx->ownership.dispatch_owned = true;
    ctx->dispatch_summary.dispatch_count = 1u;
    return true;
}

static bool ray_tracing_app_dispatch_execute_ctx(
    RayTracingAppContext *ctx,
    const RayTracingDispatchRequest *request,
    RayTracingDispatchOutcome *outcome) {
    if (!ctx || !request || !outcome || !ctx->runtime_dispatch) {
        return false;
    }
    memset(outcome, 0, sizeof(*outcome));
    return ctx->runtime_dispatch(request, outcome) && outcome->dispatched;
}

static int ray_tracing_app_dispatch_finalize_ctx(RayTracingAppContext *ctx,
                                                 const RayTracingDispatchOutcome *outcome) {
    if (!ctx || !outcome) {
        return 1;
    }
    ctx->dispatch_summary.used_legacy_entry = outcome->used_legacy_entry;
    ctx->dispatch_summary.dispatch_succeeded = true;
    ctx->dispatch_summary.last_dispatch_exit_code = outcome->exit_code;
    ctx->exit_code = outcome->exit_code;
    if (!ray_tracing_app_transition_stage(ctx,
                                          RAY_TRACING_APP_STAGE_RUNTIME_STARTED,
                                          RAY_TRACING_APP_STAGE_LOOP_COMPLETED)) {
        return 1;
    }
    ctx->run_loop_completed = true;
    return ctx->exit_code;
}

static int ray_tracing_app_run_loop_ctx(RayTracingAppContext *ctx) {
    RayTracingDispatchRequest request = {0};
    RayTracingDispatchOutcome outcome = {0};

    if (!ray_tracing_app_dispatch_prepare_ctx(ctx, &request)) {
        return 1;
    }
    if (!ray_tracing_app_dispatch_execute_ctx(ctx, &request, &outcome)) {
        ctx->dispatch_summary.dispatch_succeeded = false;
        ctx->dispatch_summary.last_dispatch_exit_code = 1;
        return 1;
    }
    return ray_tracing_app_dispatch_finalize_ctx(ctx, &outcome);
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

void ray_tracing_app_set_legacy_entry(int (*legacy_entry)(int argc, char **argv)) {
    if (legacy_entry) {
        g_ray_tracing_app_ctx.legacy_entry = legacy_entry;
    }
}

int ray_tracing_app_run_loop(void) {
    return ray_tracing_app_run_loop_ctx(&g_ray_tracing_app_ctx);
}

void ray_tracing_app_shutdown(void) {
    ray_tracing_app_shutdown_ctx(&g_ray_tracing_app_ctx);
}

int ray_tracing_app_main(int argc, char **argv) {
    int exit_code = 1;

    g_ray_tracing_app_ctx.launch_args.argc = argc;
    g_ray_tracing_app_ctx.launch_args.argv = argv;

    if (!ray_tracing_app_bootstrap_ctx(&g_ray_tracing_app_ctx)) {
        return exit_code;
    }
    if (!ray_tracing_app_config_load_ctx(&g_ray_tracing_app_ctx)) {
        goto shutdown;
    }
    if (!ray_tracing_app_state_seed_ctx(&g_ray_tracing_app_ctx)) {
        goto shutdown;
    }
    if (!ray_tracing_app_subsystems_init_ctx(&g_ray_tracing_app_ctx)) {
        goto shutdown;
    }
    if (!ray_tracing_runtime_start_ctx(&g_ray_tracing_app_ctx)) {
        goto shutdown;
    }

    exit_code = ray_tracing_app_run_loop_ctx(&g_ray_tracing_app_ctx);
shutdown:
    ray_tracing_app_shutdown_ctx(&g_ray_tracing_app_ctx);
    return exit_code;
}

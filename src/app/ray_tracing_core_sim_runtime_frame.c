#include "app/ray_tracing_core_sim_runtime_frame.h"

#include <string.h>

enum {
    RAY_TRACING_CORE_SIM_FRAME_MAX_TICKS = 1u
};

typedef struct RayTracingCoreSimRuntimeFrameContext {
    const RayTracingCoreSimRuntimeFrameRequest *request;
    RayTracingCoreSimRuntimeFrameResult *result;
} RayTracingCoreSimRuntimeFrameContext;

static double ray_tracing_core_sim_runtime_now_seconds(
    const RayTracingCoreSimRuntimeFrameRequest *request) {
    if (request && request->now_seconds_fn) {
        return request->now_seconds_fn(request->now_user_data);
    }
    return 0.0;
}

static void ray_tracing_core_sim_runtime_fail(CoreSimPassOutcome *outcome,
                                              uint32_t pass_id,
                                              const char *message) {
    if (!outcome) {
        return;
    }
    outcome->status = CORE_SIM_STATUS_PASS_FAILED;
    outcome->pass_id = pass_id;
    outcome->message = message;
}

static bool ray_tracing_core_sim_runtime_run_events(void *user_context,
                                                    const CoreSimTickContext *tick,
                                                    CoreSimPassOutcome *outcome) {
    RayTracingCoreSimRuntimeFrameContext *ctx =
        (RayTracingCoreSimRuntimeFrameContext *)user_context;
    bool handled = false;
    (void)tick;

    core_sim_pass_outcome_init(outcome, RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_EVENTS);
    if (!ctx || !ctx->request || !ctx->result ||
        !ctx->request->events_request.events_fn) {
        ray_tracing_core_sim_runtime_fail(outcome,
                                          RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_EVENTS,
                                          "invalid ray tracing events pass");
        return false;
    }

    handled = ray_tracing_app_frame_events(&ctx->request->events_request,
                                           &ctx->result->events_outcome) &&
              ctx->result->events_outcome.handled;
    if (!handled) {
        handled = ctx->request->events_request.events_fn(
            ctx->request->events_request.user_data);
        ctx->result->events_outcome.accepted_by_wrapper = false;
        ctx->result->events_outcome.handled = handled;
    }
    if (!handled) {
        ray_tracing_core_sim_runtime_fail(outcome,
                                          RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_EVENTS,
                                          "ray tracing events pass failed");
        return false;
    }

    ctx->result->stage_marks.after_events =
        ray_tracing_core_sim_runtime_now_seconds(ctx->request);
    return true;
}

static bool ray_tracing_core_sim_runtime_run_update(void *user_context,
                                                    const CoreSimTickContext *tick,
                                                    CoreSimPassOutcome *outcome) {
    RayTracingCoreSimRuntimeFrameContext *ctx =
        (RayTracingCoreSimRuntimeFrameContext *)user_context;
    bool updated = false;
    (void)tick;

    core_sim_pass_outcome_init(outcome, RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_UPDATE);
    if (!ctx || !ctx->request || !ctx->result ||
        !ctx->request->update_request.update_fn) {
        ray_tracing_core_sim_runtime_fail(outcome,
                                          RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_UPDATE,
                                          "invalid ray tracing update pass");
        return false;
    }

    updated = ray_tracing_app_frame_update(&ctx->request->update_request,
                                           &ctx->result->update_outcome) &&
              ctx->result->update_outcome.updated;
    if (!updated) {
        updated = ctx->request->update_request.update_fn(
            ctx->request->update_request.user_data);
        ctx->result->update_outcome.accepted_by_wrapper = false;
        ctx->result->update_outcome.updated = updated;
    }
    if (!updated) {
        ray_tracing_core_sim_runtime_fail(outcome,
                                          RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_UPDATE,
                                          "ray tracing update pass failed");
        return false;
    }

    ctx->result->stage_marks.after_update =
        ray_tracing_core_sim_runtime_now_seconds(ctx->request);
    return true;
}

static bool ray_tracing_core_sim_runtime_run_route(void *user_context,
                                                   const CoreSimTickContext *tick,
                                                   CoreSimPassOutcome *outcome) {
    RayTracingCoreSimRuntimeFrameContext *ctx =
        (RayTracingCoreSimRuntimeFrameContext *)user_context;
    bool routed = false;
    (void)tick;

    core_sim_pass_outcome_init(outcome, RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_ROUTE);
    if (!ctx || !ctx->request || !ctx->result ||
        !ctx->request->route_request.route_fn) {
        ray_tracing_core_sim_runtime_fail(outcome,
                                          RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_ROUTE,
                                          "invalid ray tracing route pass");
        return false;
    }

    ctx->result->stage_marks.after_route =
        ray_tracing_core_sim_runtime_now_seconds(ctx->request);
    routed = ray_tracing_app_frame_route(&ctx->request->route_request,
                                         &ctx->result->route_outcome) &&
             ctx->result->route_outcome.routed;
    if (!routed) {
        routed = ctx->request->route_request.route_fn(
            ctx->request->route_request.user_data);
        ctx->result->route_outcome.accepted_by_wrapper = false;
        ctx->result->route_outcome.routed = routed;
    }
    if (!routed) {
        ray_tracing_core_sim_runtime_fail(outcome,
                                          RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_ROUTE,
                                          "ray tracing route pass failed");
        return false;
    }

    ctx->result->stage_marks.after_render_derive =
        ray_tracing_core_sim_runtime_now_seconds(ctx->request);
    return true;
}

static bool ray_tracing_core_sim_runtime_run_submit(void *user_context,
                                                    const CoreSimTickContext *tick,
                                                    CoreSimPassOutcome *outcome) {
    RayTracingCoreSimRuntimeFrameContext *ctx =
        (RayTracingCoreSimRuntimeFrameContext *)user_context;
    bool submitted = false;
    (void)tick;

    core_sim_pass_outcome_init(outcome, RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_SUBMIT);
    if (!ctx || !ctx->request || !ctx->result ||
        !ctx->request->submit_request.submit_fn) {
        ray_tracing_core_sim_runtime_fail(outcome,
                                          RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_SUBMIT,
                                          "invalid ray tracing submit pass");
        return false;
    }

    ctx->result->stage_marks.before_present =
        ray_tracing_core_sim_runtime_now_seconds(ctx->request);
    submitted = ray_tracing_app_render_submit(&ctx->request->submit_request,
                                              &ctx->result->submit_outcome) &&
                ctx->result->submit_outcome.submitted;
    if (!submitted) {
        submitted = ctx->request->submit_request.submit_fn(
            ctx->request->submit_request.user_data);
        ctx->result->submit_outcome.accepted_by_wrapper = false;
        ctx->result->submit_outcome.submitted = submitted;
    }
    if (!submitted) {
        ray_tracing_core_sim_runtime_fail(outcome,
                                          RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_SUBMIT,
                                          "ray tracing submit pass failed");
        return false;
    }

    ctx->result->stage_marks.after_render =
        ray_tracing_core_sim_runtime_now_seconds(ctx->request);
    return true;
}

static bool ray_tracing_core_sim_runtime_run_loop_conditions(
    void *user_context,
    const CoreSimTickContext *tick,
    CoreSimPassOutcome *outcome) {
    RayTracingCoreSimRuntimeFrameContext *ctx =
        (RayTracingCoreSimRuntimeFrameContext *)user_context;
    bool checked = false;
    (void)tick;

    core_sim_pass_outcome_init(outcome,
                               RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_LOOP_CONDITIONS);
    if (!ctx || !ctx->request || !ctx->result) {
        ray_tracing_core_sim_runtime_fail(
            outcome,
            RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_LOOP_CONDITIONS,
            "invalid ray tracing loop condition pass");
        return false;
    }
    if (!ctx->request->loop_conditions_request.check_fn) {
        return true;
    }

    checked = ctx->request->loop_conditions_request.check_fn(
        ctx->request->loop_conditions_request.user_data);
    ctx->result->loop_conditions_checked = checked;
    if (!checked) {
        ray_tracing_core_sim_runtime_fail(
            outcome,
            RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_LOOP_CONDITIONS,
            "ray tracing loop condition pass failed");
        return false;
    }
    return true;
}

bool ray_tracing_core_sim_runtime_frame_loop_init(CoreSimLoopState *loop_state) {
    CoreSimStepPolicy policy;

    if (!loop_state) {
        return false;
    }
    core_sim_step_policy_defaults(&policy);
    policy.fixed_dt_seconds = 1.0 / 60.0;
    policy.max_ticks_per_frame = RAY_TRACING_CORE_SIM_FRAME_MAX_TICKS;
    policy.drop_excess_accumulator_on_clamp = true;

    if (!core_sim_loop_init(loop_state, &policy)) {
        return false;
    }
    core_sim_loop_set_paused(loop_state, false);
    return true;
}

void ray_tracing_core_sim_runtime_frame_set_paused(CoreSimLoopState *loop_state,
                                                   bool paused) {
    core_sim_loop_set_paused(loop_state, paused);
}

bool ray_tracing_core_sim_runtime_frame_step(
    CoreSimLoopState *loop_state,
    const RayTracingCoreSimRuntimeFrameRequest *request,
    RayTracingCoreSimRuntimeFrameResult *result) {
    static const CoreSimPassDescriptor kPasses[] = {
        {RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_EVENTS,
         "events",
         ray_tracing_core_sim_runtime_run_events},
        {RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_UPDATE,
         "update",
         ray_tracing_core_sim_runtime_run_update},
        {RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_ROUTE,
         "route",
         ray_tracing_core_sim_runtime_run_route},
        {RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_SUBMIT,
         "submit",
         ray_tracing_core_sim_runtime_run_submit},
        {RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_LOOP_CONDITIONS,
         "loop_conditions",
         ray_tracing_core_sim_runtime_run_loop_conditions}
    };
    CoreSimPassOrder pass_order = {kPasses, sizeof(kPasses) / sizeof(kPasses[0])};
    CoreSimFrameRequest frame_request;
    RayTracingCoreSimRuntimeFrameContext context;

    if (!loop_state || !request || !result) {
        if (result) {
            memset(result, 0, sizeof(*result));
            result->sim_outcome = core_sim_frame_outcome_make_invalid(
                CORE_SIM_STATUS_INVALID_ARGUMENT,
                "invalid ray tracing core sim frame arguments");
        }
        return false;
    }
    if (!core_sim_step_policy_valid(&loop_state->policy)) {
        if (!ray_tracing_core_sim_runtime_frame_loop_init(loop_state)) {
            memset(result, 0, sizeof(*result));
            result->sim_outcome = core_sim_frame_outcome_make_invalid(
                CORE_SIM_STATUS_INVALID_POLICY,
                "invalid ray tracing core sim frame policy");
            return false;
        }
    }

    memset(result, 0, sizeof(*result));
    result->stage_marks.frame_begin =
        ray_tracing_core_sim_runtime_now_seconds(request);

    context.request = request;
    context.result = result;

    frame_request.frame_dt_seconds =
        request->frame_dt_seconds > 0.0
            ? request->frame_dt_seconds
            : loop_state->policy.fixed_dt_seconds;
    frame_request.user_context = &context;
    frame_request.pass_order = &pass_order;

    result->sim_outcome = core_sim_loop_advance(loop_state, &frame_request);
    return result->sim_outcome.status == CORE_SIM_STATUS_OK &&
           result->sim_outcome.ticks_executed == 1u;
}

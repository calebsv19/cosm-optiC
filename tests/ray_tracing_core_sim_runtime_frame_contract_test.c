#include "app/ray_tracing_core_sim_runtime_frame.h"

#include <stdio.h>
#include <string.h>

typedef struct RuntimeFrameContractState {
    int order[8];
    int order_count;
    double now_seconds;
} RuntimeFrameContractState;

static bool record_pass(RuntimeFrameContractState *state, int pass_id) {
    if (!state || state->order_count >= (int)(sizeof(state->order) / sizeof(state->order[0]))) {
        return false;
    }
    state->order[state->order_count++] = pass_id;
    return true;
}

static double test_now_seconds(void *user_data) {
    RuntimeFrameContractState *state = (RuntimeFrameContractState *)user_data;
    if (!state) {
        return 0.0;
    }
    state->now_seconds += 0.001;
    return state->now_seconds;
}

static bool test_events(void *user_data) {
    return record_pass((RuntimeFrameContractState *)user_data,
                       RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_EVENTS);
}

static bool test_update(void *user_data) {
    return record_pass((RuntimeFrameContractState *)user_data,
                       RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_UPDATE);
}

static bool test_route(void *user_data) {
    return record_pass((RuntimeFrameContractState *)user_data,
                       RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_ROUTE);
}

static bool test_submit(void *user_data) {
    return record_pass((RuntimeFrameContractState *)user_data,
                       RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_SUBMIT);
}

static bool test_loop_conditions(void *user_data) {
    return record_pass((RuntimeFrameContractState *)user_data,
                       RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_LOOP_CONDITIONS);
}

static bool test_ordered_runtime_frame_passes(void) {
    RuntimeFrameContractState state = {0};
    CoreSimLoopState loop = {0};
    RayTracingCoreSimRuntimeFrameRequest request = {0};
    RayTracingCoreSimRuntimeFrameResult result = {0};
    const int expected[] = {
        RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_EVENTS,
        RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_UPDATE,
        RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_ROUTE,
        RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_SUBMIT,
        RAY_TRACING_CORE_SIM_RUNTIME_FRAME_PASS_LOOP_CONDITIONS
    };
    int i = 0;

    if (!ray_tracing_app_bootstrap()) return false;
    if (!ray_tracing_app_config_load()) return false;
    if (!ray_tracing_app_state_seed()) return false;
    if (!ray_tracing_app_subsystems_init()) return false;
    if (!ray_tracing_runtime_start()) return false;

    if (!ray_tracing_core_sim_runtime_frame_loop_init(&loop)) {
        return false;
    }

    request.frame_dt_seconds = 1.0 / 60.0;
    request.now_seconds_fn = test_now_seconds;
    request.now_user_data = &state;
    request.events_request.events_fn = test_events;
    request.events_request.user_data = &state;
    request.update_request.update_fn = test_update;
    request.update_request.user_data = &state;
    request.route_request.route_fn = test_route;
    request.route_request.user_data = &state;
    request.submit_request.submit_fn = test_submit;
    request.submit_request.user_data = &state;
    request.loop_conditions_request.check_fn = test_loop_conditions;
    request.loop_conditions_request.user_data = &state;

    if (!ray_tracing_core_sim_runtime_frame_step(&loop, &request, &result)) {
        return false;
    }

    if (result.sim_outcome.status != CORE_SIM_STATUS_OK) return false;
    if (result.sim_outcome.ticks_executed != 1u) return false;
    if (result.sim_outcome.passes_executed != 5u) return false;
    if (!result.events_outcome.accepted_by_wrapper || !result.events_outcome.handled) return false;
    if (!result.update_outcome.accepted_by_wrapper || !result.update_outcome.updated) return false;
    if (!result.route_outcome.accepted_by_wrapper || !result.route_outcome.routed) return false;
    if (!result.submit_outcome.accepted_by_wrapper || !result.submit_outcome.submitted) return false;
    if (!result.loop_conditions_checked) return false;
    if (state.order_count != 5) return false;
    for (i = 0; i < state.order_count; ++i) {
        if (state.order[i] != expected[i]) return false;
    }
    if (!(result.stage_marks.frame_begin < result.stage_marks.after_events &&
          result.stage_marks.after_events < result.stage_marks.after_update &&
          result.stage_marks.after_update < result.stage_marks.after_route &&
          result.stage_marks.after_route < result.stage_marks.after_render_derive &&
          result.stage_marks.after_render_derive < result.stage_marks.before_present &&
          result.stage_marks.before_present < result.stage_marks.after_render)) {
        return false;
    }

    ray_tracing_app_shutdown();
    return true;
}

int main(void) {
    if (!test_ordered_runtime_frame_passes()) {
        fprintf(stderr, "ray_tracing_core_sim_runtime_frame_contract_test: failed\n");
        return 1;
    }
    fprintf(stdout, "ray_tracing_core_sim_runtime_frame_contract_test: success\n");
    return 0;
}

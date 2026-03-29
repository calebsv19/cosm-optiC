#include "ray_tracing/ray_tracing_app_main.h"

#include <string.h>

typedef struct RayTracingLifecycleState {
    bool bootstrapped;
    bool config_loaded;
    bool state_seeded;
    bool subsystems_initialized;
    bool runtime_started;
    bool run_loop_completed;
    bool shutdown_completed;
    int exit_code;
} RayTracingLifecycleState;

static RayTracingLifecycleState g_ray_tracing_lifecycle = {0};

static int g_ray_tracing_launch_argc = 0;
static char **g_ray_tracing_launch_argv = NULL;

static int ray_tracing_default_legacy_entry(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return 1;
}

static int (*g_ray_tracing_legacy_entry)(int argc, char **argv) =
    ray_tracing_default_legacy_entry;

bool ray_tracing_app_bootstrap(void) {
    memset(&g_ray_tracing_lifecycle, 0, sizeof(g_ray_tracing_lifecycle));
    g_ray_tracing_lifecycle.bootstrapped = true;
    return true;
}

bool ray_tracing_app_config_load(void) {
    if (!g_ray_tracing_lifecycle.bootstrapped) {
        return false;
    }
    g_ray_tracing_lifecycle.config_loaded = true;
    return true;
}

bool ray_tracing_app_state_seed(void) {
    if (!g_ray_tracing_lifecycle.config_loaded) {
        return false;
    }
    g_ray_tracing_lifecycle.state_seeded = true;
    return true;
}

bool ray_tracing_app_subsystems_init(void) {
    if (!g_ray_tracing_lifecycle.state_seeded) {
        return false;
    }
    g_ray_tracing_lifecycle.subsystems_initialized = true;
    return true;
}

bool ray_tracing_runtime_start(void) {
    if (!g_ray_tracing_lifecycle.subsystems_initialized) {
        return false;
    }
    g_ray_tracing_lifecycle.runtime_started = true;
    return true;
}

void ray_tracing_app_set_legacy_entry(int (*legacy_entry)(int argc, char **argv)) {
    if (legacy_entry) {
        g_ray_tracing_legacy_entry = legacy_entry;
    }
}

int ray_tracing_app_run_loop(void) {
    if (!g_ray_tracing_lifecycle.runtime_started) {
        return 1;
    }
    g_ray_tracing_lifecycle.exit_code =
        g_ray_tracing_legacy_entry(g_ray_tracing_launch_argc, g_ray_tracing_launch_argv);
    g_ray_tracing_lifecycle.run_loop_completed = true;
    return g_ray_tracing_lifecycle.exit_code;
}

void ray_tracing_app_shutdown(void) {
    if (!g_ray_tracing_lifecycle.bootstrapped) {
        return;
    }
    g_ray_tracing_lifecycle.shutdown_completed = true;
}

int ray_tracing_app_main(int argc, char **argv) {
    int exit_code = 1;

    g_ray_tracing_launch_argc = argc;
    g_ray_tracing_launch_argv = argv;

    if (!ray_tracing_app_bootstrap()) {
        return exit_code;
    }
    if (!ray_tracing_app_config_load()) {
        ray_tracing_app_shutdown();
        return exit_code;
    }
    if (!ray_tracing_app_state_seed()) {
        ray_tracing_app_shutdown();
        return exit_code;
    }
    if (!ray_tracing_app_subsystems_init()) {
        ray_tracing_app_shutdown();
        return exit_code;
    }
    if (!ray_tracing_runtime_start()) {
        ray_tracing_app_shutdown();
        return exit_code;
    }

    exit_code = ray_tracing_app_run_loop();
    ray_tracing_app_shutdown();
    return exit_code;
}

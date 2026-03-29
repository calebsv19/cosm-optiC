#ifndef RAY_TRACING_RAY_TRACING_APP_MAIN_H
#define RAY_TRACING_RAY_TRACING_APP_MAIN_H

#include <stdbool.h>

bool ray_tracing_app_bootstrap(void);
bool ray_tracing_app_config_load(void);
bool ray_tracing_app_state_seed(void);
bool ray_tracing_app_subsystems_init(void);
bool ray_tracing_runtime_start(void);
void ray_tracing_app_set_legacy_entry(int (*legacy_entry)(int argc, char **argv));
int ray_tracing_app_run_loop(void);
void ray_tracing_app_shutdown(void);

int ray_tracing_app_main(int argc, char **argv);

#endif

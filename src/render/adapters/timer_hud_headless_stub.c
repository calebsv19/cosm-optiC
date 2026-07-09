#include "render/timer_hud_adapter.h"

TimerHUDSession* timer_hud_session(void) {
    return NULL;
}

void timer_hud_register_backend(void) {
}

void timer_hud_apply_startup_env_overrides(void) {
}

void timer_hud_shutdown_session(void) {
}

bool timer_hud_is_active(void) {
    return false;
}

void timer_hud_record_duration_ms(const char* name, double duration_ms) {
    (void)name;
    (void)duration_ms;
}

#ifndef RENDER_TIMER_HUD_ADAPTER_H
#define RENDER_TIMER_HUD_ADAPTER_H

#include "timer_hud/time_scope.h"

TimerHUDSession* timer_hud_session(void);
void timer_hud_register_backend(void);
void timer_hud_apply_startup_env_overrides(void);
void timer_hud_shutdown_session(void);

#endif // RENDER_TIMER_HUD_ADAPTER_H

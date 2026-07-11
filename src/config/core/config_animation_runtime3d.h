#ifndef CONFIG_ANIMATION_RUNTIME3D_H
#define CONFIG_ANIMATION_RUNTIME3D_H

#include "config/config_manager.h"

int animation_config_runtime_window_dimension_clamp(int value, int fallback);
void animation_config_normalize_runtime3d_fields(AnimationConfig* cfg);

#endif

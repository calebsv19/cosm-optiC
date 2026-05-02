#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "config/config_manager.h"

bool runtime_scene_volume_defaults_resolve(const char* runtime_scene_path,
                                           int* out_volume_source_kind,
                                           char* out_volume_source_path,
                                           size_t out_volume_source_path_size);
void runtime_scene_volume_defaults_apply_transition(AnimationConfig* cfg,
                                                    const char* previous_runtime_scene_path,
                                                    const char* next_runtime_scene_path);

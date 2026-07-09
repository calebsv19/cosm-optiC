#pragma once

#include <stdbool.h>

bool test_runtime_scene_3d_geometry_trace_enabled(void);
void test_runtime_scene_3d_geometry_trace(const char* name);

int run_test_runtime_scene_3d_geometry_builder_suite(void);
int run_test_runtime_scene_3d_geometry_trace_suite(void);

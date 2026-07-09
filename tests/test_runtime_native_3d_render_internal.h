#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint8_t native3d_test_pixel_r(const uint8_t* pixels, int width, int x, int y);
uint8_t native3d_test_pixel_g(const uint8_t* pixels, int width, int x, int y);
uint8_t native3d_test_pixel_b(const uint8_t* pixels, int width, int x, int y);
bool native3d_test_pixels_match_rgb_only(const uint8_t* a, const uint8_t* b, size_t pixel_count);

int run_test_runtime_native_3d_render_live_suite(void);
int run_test_runtime_native_3d_render_prepared_suite(void);

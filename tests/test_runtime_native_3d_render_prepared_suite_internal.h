#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "render/runtime_volume_3d.h"

bool prepared_suite_make_temp_dir(char* out_dir, size_t out_dir_size);
bool prepared_suite_write_text_file(const char* path, const char* text);
bool prepared_suite_write_sample_vf3d(const char* path);
bool prepared_suite_attach_dense_volume(RuntimeVolumeAttachment3D* volume,
                                        Vec3 origin,
                                        uint32_t grid_w,
                                        uint32_t grid_h,
                                        uint32_t grid_d,
                                        double voxel_size,
                                        float density_value);

int run_test_runtime_native_3d_render_prepared_parity_volume_suite(void);
int run_test_runtime_native_3d_render_prepared_scatter_preview_suite(void);

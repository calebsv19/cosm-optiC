#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "render/runtime_volume_3d.h"
#include "test_runtime_native_3d_render_internal.h"
#include "test_runtime_native_3d_render_prepared_suite_internal.h"
#include "test_support.h"

typedef struct VolumeFrameHeaderVf3dPreparedTestV1 {
    uint32_t magic;
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    uint32_t grid_d;
    double time_seconds;
    uint64_t frame_index;
    double dt_seconds;
    float origin_x;
    float origin_y;
    float origin_z;
    float voxel_size;
    float scene_up_x;
    float scene_up_y;
    float scene_up_z;
    uint32_t solid_mask_crc32;
    uint32_t reserved[3];
} VolumeFrameHeaderVf3dPreparedTestV1;

static const uint32_t kPreparedSuiteVf3dMagic =
    ('V' << 24) | ('F' << 16) | ('3' << 8) | ('D');

bool prepared_suite_make_temp_dir(char* out_dir, size_t out_dir_size) {
    char tmp_template[] = "/tmp/rt_native_3d_volume_prepareXXXXXX";
    char* resolved = NULL;
    if (!out_dir || out_dir_size == 0u) return false;
    resolved = mkdtemp(tmp_template);
    if (!resolved) return false;
    if (snprintf(out_dir, out_dir_size, "%s", resolved) >= (int)out_dir_size) {
        rmdir(resolved);
        return false;
    }
    return true;
}

bool prepared_suite_write_text_file(const char* path, const char* text) {
    FILE* file = NULL;
    size_t text_len = 0u;
    if (!path || !text) return false;
    file = fopen(path, "wb");
    if (!file) return false;
    text_len = strlen(text);
    if (fwrite(text, 1u, text_len, file) != text_len) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

bool prepared_suite_write_sample_vf3d(const char* path) {
    VolumeFrameHeaderVf3dPreparedTestV1 header = {0};
    float density[8] = {0.15f, 0.25f, 0.35f, 0.45f, 0.55f, 0.65f, 0.75f, 0.85f};
    float velx[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    float vely[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float velz[8] = {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f};
    float pressure[8] = {2.0f, 2.1f, 2.2f, 2.3f, 2.4f, 2.5f, 2.6f, 2.7f};
    uint8_t solid[8] = {0u, 0u, 1u, 1u, 0u, 0u, 1u, 1u};
    FILE* file = NULL;

    if (!path) return false;
    header.magic = kPreparedSuiteVf3dMagic;
    header.version = 1u;
    header.grid_w = 2u;
    header.grid_h = 2u;
    header.grid_d = 2u;
    header.time_seconds = 2.0;
    header.frame_index = 5u;
    header.dt_seconds = 0.02;
    header.origin_x = -0.5f;
    header.origin_y = -0.5f;
    header.origin_z = 0.0f;
    header.voxel_size = 0.5f;
    header.scene_up_x = 0.0f;
    header.scene_up_y = 0.0f;
    header.scene_up_z = 1.0f;
    header.solid_mask_crc32 = 44u;

    file = fopen(path, "wb");
    if (!file) return false;
    if (fwrite(&header, sizeof(header), 1u, file) != 1u ||
        fwrite(density, sizeof(density), 1u, file) != 1u ||
        fwrite(velx, sizeof(velx), 1u, file) != 1u ||
        fwrite(vely, sizeof(vely), 1u, file) != 1u ||
        fwrite(velz, sizeof(velz), 1u, file) != 1u ||
        fwrite(pressure, sizeof(pressure), 1u, file) != 1u ||
        fwrite(solid, sizeof(solid), 1u, file) != 1u) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

bool prepared_suite_attach_dense_volume(RuntimeVolumeAttachment3D* volume,
                                        Vec3 origin,
                                        uint32_t grid_w,
                                        uint32_t grid_h,
                                        uint32_t grid_d,
                                        double voxel_size,
                                        float density_value) {
    bool ok = false;

    if (!volume) return false;
    RuntimeVolumeAttachment3D_Reset(volume);
    volume->enabled = true;
    volume->affectsLighting = true;
    ok = RuntimeVolumeGrid3D_Configure(&volume->grid,
                                       1u,
                                       grid_w,
                                       grid_h,
                                       grid_d,
                                       0.0,
                                       0u,
                                       0.02,
                                       origin,
                                       voxel_size,
                                       vec3(0.0, 0.0, 1.0),
                                       0u);
    if (!ok) return false;
    ok = RuntimeVolumeAttachment3D_AllocateOwnedChannels(
        volume,
        RUNTIME_VOLUME_3D_CHANNEL_DENSITY | RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK);
    if (!ok) return false;
    for (uint64_t i = 0; i < volume->grid.cellCount; ++i) {
        volume->channels.density[i] = density_value;
        volume->channels.solidMask[i] = 0u;
    }
    return true;
}

int run_test_runtime_native_3d_render_prepared_suite(void) {
    int before = test_support_failures();

    run_test_runtime_native_3d_render_prepared_parity_volume_suite();
    run_test_runtime_native_3d_render_prepared_scatter_preview_suite();
    return test_support_failures() - before;
}

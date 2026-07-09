#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "core_pack.h"
#include "import/fluid_volume_import_3d.h"
#include "test_fluid_volume_pack_import_3d.h"
#include "test_support.h"

typedef struct Vf3dHeaderCanonical {
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
} Vf3dHeaderCanonical;

typedef struct TestVf3dPackWriteOptions {
    bool include_density;
    bool include_velocity_x;
    bool include_velocity_y;
    bool include_velocity_z;
    bool include_pressure;
    bool include_solid;
} TestVf3dPackWriteOptions;

static bool make_temp_pack_path(char* out_path, size_t out_path_size) {
    char tmp_template[] = "/tmp/rt_vf3d_pack_testXXXXXX";
    int fd = -1;
    if (!out_path || out_path_size == 0u) return false;
    fd = mkstemp(tmp_template);
    if (fd < 0) return false;
    close(fd);
    if (snprintf(out_path, out_path_size, "%s.pack", tmp_template) >= (int)out_path_size) {
        unlink(tmp_template);
        return false;
    }
    if (rename(tmp_template, out_path) != 0) {
        unlink(tmp_template);
        return false;
    }
    return true;
}

static bool write_test_vf3d_pack(const char* path,
                                 const TestVf3dPackWriteOptions* options) {
    Vf3dHeaderCanonical header = {0};
    float density[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float velx[8] = {1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f};
    float vely[8] = {-1.0f, -1.1f, -1.2f, -1.3f, -1.4f, -1.5f, -1.6f, -1.7f};
    float velz[8] = {2.0f, 2.1f, 2.2f, 2.3f, 2.4f, 2.5f, 2.6f, 2.7f};
    float pressure[8] = {3.0f, 3.1f, 3.2f, 3.3f, 3.4f, 3.5f, 3.6f, 3.7f};
    uint8_t solid[8] = {0u, 1u, 0u, 1u, 1u, 0u, 1u, 0u};
    CorePackWriter writer = {0};
    CoreResult wr = core_result_ok();
    CoreResult close_result = core_result_ok();

    if (!path || !options) return false;
    header.version = 1u;
    header.grid_w = 2u;
    header.grid_h = 2u;
    header.grid_d = 2u;
    header.time_seconds = 2.0;
    header.frame_index = 12u;
    header.dt_seconds = 0.02;
    header.origin_x = 4.0f;
    header.origin_y = -3.0f;
    header.origin_z = 1.0f;
    header.voxel_size = 0.5f;
    header.scene_up_x = 0.0f;
    header.scene_up_y = 0.0f;
    header.scene_up_z = 1.0f;
    header.solid_mask_crc32 = 55u;

    wr = core_pack_writer_open(path, &writer);
    if (wr.code != CORE_OK) return false;
    wr = core_pack_writer_add_chunk(&writer, "VF3H", &header, (uint64_t)sizeof(header));
    if (wr.code == CORE_OK && options->include_density) {
        wr = core_pack_writer_add_chunk(&writer, "DENS", density, (uint64_t)sizeof(density));
    }
    if (wr.code == CORE_OK && options->include_velocity_x) {
        wr = core_pack_writer_add_chunk(&writer, "VELX", velx, (uint64_t)sizeof(velx));
    }
    if (wr.code == CORE_OK && options->include_velocity_y) {
        wr = core_pack_writer_add_chunk(&writer, "VELY", vely, (uint64_t)sizeof(vely));
    }
    if (wr.code == CORE_OK && options->include_velocity_z) {
        wr = core_pack_writer_add_chunk(&writer, "VELZ", velz, (uint64_t)sizeof(velz));
    }
    if (wr.code == CORE_OK && options->include_pressure) {
        wr = core_pack_writer_add_chunk(&writer, "PRES", pressure, (uint64_t)sizeof(pressure));
    }
    if (wr.code == CORE_OK && options->include_solid) {
        wr = core_pack_writer_add_chunk(&writer, "SOLI", solid, (uint64_t)sizeof(solid));
    }

    close_result = core_pack_writer_close(&writer);
    return wr.code == CORE_OK && close_result.code == CORE_OK;
}

static int test_fluid_volume_pack_import_3d_loads_full_pack(void) {
    char path[PATH_MAX] = {0};
    RuntimeVolumeAttachment3D attachment;
    char diagnostics[128] = {0};
    const TestVf3dPackWriteOptions options = {
        .include_density = true,
        .include_velocity_x = true,
        .include_velocity_y = true,
        .include_velocity_z = true,
        .include_pressure = true,
        .include_solid = true
    };
    bool ok = false;

    RuntimeVolumeAttachment3D_Init(&attachment);
    assert_true("fluid_volume_pack_import_3d_full_temp_path",
                make_temp_pack_path(path, sizeof(path)));
    if (!path[0]) {
        RuntimeVolumeAttachment3D_Free(&attachment);
        return 0;
    }
    assert_true("fluid_volume_pack_import_3d_full_write",
                write_test_vf3d_pack(path, &options));

    ok = fluid_volume_import_3d_load_pack(path, &attachment, diagnostics, sizeof(diagnostics));
    assert_true("fluid_volume_pack_import_3d_full_load_ok", ok);
    assert_true("fluid_volume_pack_import_3d_full_diag_ok",
                strcmp(diagnostics, "ok") == 0);
    assert_true("fluid_volume_pack_import_3d_full_source_pack",
                attachment.sourceKind == RUNTIME_VOLUME_3D_SOURCE_PACK);
    assert_true("fluid_volume_pack_import_3d_full_dims", attachment.grid.gridD == 2u);
    assert_true("fluid_volume_pack_import_3d_full_has_velocity",
                RuntimeVolumeAttachment3D_HasChannel(&attachment, RUNTIME_VOLUME_3D_CHANNEL_VELOCITY));
    assert_true("fluid_volume_pack_import_3d_full_has_pressure",
                RuntimeVolumeAttachment3D_HasChannel(&attachment, RUNTIME_VOLUME_3D_CHANNEL_PRESSURE));
    assert_true("fluid_volume_pack_import_3d_full_has_solid",
                RuntimeVolumeAttachment3D_HasChannel(&attachment, RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK));
    assert_true("fluid_volume_pack_import_3d_full_density_value",
                attachment.channels.density && attachment.channels.density[6] == 0.7f);
    assert_true("fluid_volume_pack_import_3d_full_vely_value",
                attachment.channels.velocityY && attachment.channels.velocityY[1] == -1.1f);
    assert_true("fluid_volume_pack_import_3d_full_pressure_value",
                attachment.channels.pressure && attachment.channels.pressure[4] == 3.4f);
    assert_true("fluid_volume_pack_import_3d_full_solid_value",
                attachment.channels.solidMask && attachment.channels.solidMask[3] == 1u);

    RuntimeVolumeAttachment3D_Free(&attachment);
    unlink(path);
    return 0;
}

static int test_fluid_volume_pack_import_3d_allows_density_only_pack(void) {
    char path[PATH_MAX] = {0};
    RuntimeVolumeAttachment3D attachment;
    char diagnostics[128] = {0};
    const TestVf3dPackWriteOptions options = {
        .include_density = true,
        .include_velocity_x = false,
        .include_velocity_y = false,
        .include_velocity_z = false,
        .include_pressure = false,
        .include_solid = false
    };
    bool ok = false;

    RuntimeVolumeAttachment3D_Init(&attachment);
    assert_true("fluid_volume_pack_import_3d_density_only_temp_path",
                make_temp_pack_path(path, sizeof(path)));
    if (!path[0]) {
        RuntimeVolumeAttachment3D_Free(&attachment);
        return 0;
    }
    assert_true("fluid_volume_pack_import_3d_density_only_write",
                write_test_vf3d_pack(path, &options));

    ok = fluid_volume_import_3d_load_pack(path, &attachment, diagnostics, sizeof(diagnostics));
    assert_true("fluid_volume_pack_import_3d_density_only_ok", ok);
    assert_true("fluid_volume_pack_import_3d_density_only_no_velocity",
                !RuntimeVolumeAttachment3D_HasChannel(&attachment, RUNTIME_VOLUME_3D_CHANNEL_VELOCITY));
    assert_true("fluid_volume_pack_import_3d_density_only_no_pressure",
                !RuntimeVolumeAttachment3D_HasChannel(&attachment, RUNTIME_VOLUME_3D_CHANNEL_PRESSURE));
    assert_true("fluid_volume_pack_import_3d_density_only_no_solid",
                !RuntimeVolumeAttachment3D_HasChannel(&attachment, RUNTIME_VOLUME_3D_CHANNEL_SOLID_MASK));
    assert_true("fluid_volume_pack_import_3d_density_only_density_present",
                attachment.channels.density != NULL);

    RuntimeVolumeAttachment3D_Free(&attachment);
    unlink(path);
    return 0;
}

static int test_fluid_volume_pack_import_3d_rejects_partial_velocity_triple(void) {
    char path[PATH_MAX] = {0};
    RuntimeVolumeAttachment3D attachment;
    char diagnostics[128] = {0};
    const TestVf3dPackWriteOptions options = {
        .include_density = true,
        .include_velocity_x = true,
        .include_velocity_y = true,
        .include_velocity_z = false,
        .include_pressure = false,
        .include_solid = false
    };
    bool ok = false;

    RuntimeVolumeAttachment3D_Init(&attachment);
    assert_true("fluid_volume_pack_import_3d_partial_velocity_temp_path",
                make_temp_pack_path(path, sizeof(path)));
    if (!path[0]) {
        RuntimeVolumeAttachment3D_Free(&attachment);
        return 0;
    }
    assert_true("fluid_volume_pack_import_3d_partial_velocity_write",
                write_test_vf3d_pack(path, &options));

    ok = fluid_volume_import_3d_load_pack(path, &attachment, diagnostics, sizeof(diagnostics));
    assert_true("fluid_volume_pack_import_3d_partial_velocity_rejected", !ok);
    assert_true("fluid_volume_pack_import_3d_partial_velocity_diag",
                strcmp(diagnostics,
                       "vf3d pack velocity chunks must be all present or all absent") == 0);
    assert_true("fluid_volume_pack_import_3d_partial_velocity_reset",
                !RuntimeVolumeGrid3D_IsConfigured(&attachment.grid));

    RuntimeVolumeAttachment3D_Free(&attachment);
    unlink(path);
    return 0;
}

int run_test_fluid_volume_pack_import_3d_tests(void) {
    int before = test_support_failures();

    test_fluid_volume_pack_import_3d_loads_full_pack();
    test_fluid_volume_pack_import_3d_allows_density_only_pack();
    test_fluid_volume_pack_import_3d_rejects_partial_velocity_triple();
    return test_support_failures() - before;
}

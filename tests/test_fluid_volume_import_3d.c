#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core_pack.h"
#include "import/fluid_volume_import_3d.h"
#include "import/water_surface_import.h"
#include "render/runtime_volume_3d_debug.h"
#include "test_fluid_volume_import_3d.h"
#include "test_support.h"

typedef struct VolumeFrameHeaderVf3dV1 {
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
} VolumeFrameHeaderVf3dV1;

static const uint32_t VOLUME_VF3D_MAGIC = ('V' << 24) | ('F' << 16) | ('3' << 8) | ('D');

static bool write_sample_vf3d_custom(const char* path,
                                     uint32_t version,
                                     bool truncate_payload,
                                     bool zero_scene_up,
                                     float density_bias,
                                     uint64_t frame_index) {
    VolumeFrameHeaderVf3dV1 header = {0};
    float density[8] = {
        0.1f + density_bias,
        0.2f + density_bias,
        0.3f + density_bias,
        0.4f + density_bias,
        0.5f + density_bias,
        0.6f + density_bias,
        0.7f + density_bias,
        0.8f + density_bias
    };
    float velx[8] = {1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f};
    float vely[8] = {-1.0f, -1.1f, -1.2f, -1.3f, -1.4f, -1.5f, -1.6f, -1.7f};
    float velz[8] = {2.0f, 2.1f, 2.2f, 2.3f, 2.4f, 2.5f, 2.6f, 2.7f};
    float pressure[8] = {3.0f, 3.1f, 3.2f, 3.3f, 3.4f, 3.5f, 3.6f, 3.7f};
    uint8_t solid[8] = {0u, 1u, 0u, 1u, 1u, 0u, 1u, 0u};
    FILE* file = NULL;

    if (!path) return false;
    header.magic = VOLUME_VF3D_MAGIC;
    header.version = version;
    header.grid_w = 2u;
    header.grid_h = 2u;
    header.grid_d = 2u;
    header.time_seconds = 1.5;
    header.frame_index = frame_index;
    header.dt_seconds = 0.016;
    header.origin_x = -1.0f;
    header.origin_y = 2.0f;
    header.origin_z = 3.0f;
    header.voxel_size = 0.25f;
    header.scene_up_x = zero_scene_up ? 0.0f : 0.0f;
    header.scene_up_y = zero_scene_up ? 0.0f : 0.0f;
    header.scene_up_z = zero_scene_up ? 0.0f : 1.0f;
    header.solid_mask_crc32 = 123u;

    file = fopen(path, "wb");
    if (!file) return false;
    if (fwrite(&header, sizeof(header), 1u, file) != 1u ||
        fwrite(density, sizeof(density), 1u, file) != 1u ||
        fwrite(velx, sizeof(velx), 1u, file) != 1u ||
        fwrite(vely, sizeof(vely), 1u, file) != 1u ||
        fwrite(velz, sizeof(velz), 1u, file) != 1u ||
        fwrite(pressure, sizeof(pressure), 1u, file) != 1u) {
        fclose(file);
        return false;
    }
    if (truncate_payload) {
        if (fwrite(solid, sizeof(uint8_t), 3u, file) != 3u) {
            fclose(file);
            return false;
        }
    } else if (fwrite(solid, sizeof(solid), 1u, file) != 1u) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

static bool write_sample_vf3d(const char* path,
                              uint32_t version,
                              bool truncate_payload,
                              bool zero_scene_up) {
    return write_sample_vf3d_custom(path,
                                    version,
                                    truncate_payload,
                                    zero_scene_up,
                                    0.0f,
                                    9u);
}

static bool make_temp_vf3d_path(char* out_path, size_t out_path_size) {
    char tmp_template[] = "/tmp/rt_vf3d_testXXXXXX";
    int fd = -1;
    if (!out_path || out_path_size == 0u) return false;
    fd = mkstemp(tmp_template);
    if (fd < 0) return false;
    close(fd);
    if (snprintf(out_path, out_path_size, "%s.vf3d", tmp_template) >= (int)out_path_size) {
        unlink(tmp_template);
        return false;
    }
    if (rename(tmp_template, out_path) != 0) {
        unlink(tmp_template);
        return false;
    }
    return true;
}

static bool make_temp_volume_dir(char* out_dir, size_t out_dir_size) {
    char tmp_template[] = "/tmp/rt_vf3d_source_testXXXXXX";
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

static bool write_local_text_file(const char* path, const char* text) {
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

static bool write_sample_vf3d_pack(const char* path) {
    CorePackWriter writer = {0};
    CoreResult result = core_result_ok();
    CoreResult close_result = core_result_ok();
    struct {
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
    } header = {0};
    float density[8] = {0.11f, 0.22f, 0.33f, 0.44f, 0.55f, 0.66f, 0.77f, 0.88f};

    if (!path) return false;
    header.version = 1u;
    header.grid_w = 2u;
    header.grid_h = 2u;
    header.grid_d = 2u;
    header.time_seconds = 4.0;
    header.frame_index = 17u;
    header.dt_seconds = 0.01;
    header.origin_x = 1.0f;
    header.origin_y = 2.0f;
    header.origin_z = 3.0f;
    header.voxel_size = 0.5f;
    header.scene_up_x = 0.0f;
    header.scene_up_y = 0.0f;
    header.scene_up_z = 1.0f;
    header.solid_mask_crc32 = 9u;

    result = core_pack_writer_open(path, &writer);
    if (result.code != CORE_OK) return false;
    result = core_pack_writer_add_chunk(&writer, "VF3H", &header, (uint64_t)sizeof(header));
    if (result.code == CORE_OK) {
        result = core_pack_writer_add_chunk(&writer, "DENS", density, (uint64_t)sizeof(density));
    }
    close_result = core_pack_writer_close(&writer);
    return result.code == CORE_OK && close_result.code == CORE_OK;
}

static int test_fluid_volume_import_3d_classifies_paths(void) {
    RuntimeVolume3DSourceKind kind = RUNTIME_VOLUME_3D_SOURCE_NONE;

    assert_true("fluid_volume_import_3d_classify_raw",
                fluid_volume_import_3d_classify_path("/tmp/sample.vf3d", &kind));
    assert_true("fluid_volume_import_3d_classify_raw_kind",
                kind == RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D);
    assert_true("fluid_volume_import_3d_classify_unsupported",
                !fluid_volume_import_3d_classify_path("/tmp/sample.bin", &kind));
    return 0;
}

static int test_fluid_volume_import_3d_loads_valid_raw_file(void) {
    char path[PATH_MAX];
    RuntimeVolumeAttachment3D attachment;
    char diagnostics[128] = {0};
    bool ok = false;

    RuntimeVolumeAttachment3D_Init(&attachment);
    assert_true("fluid_volume_import_3d_valid_temp_path",
                make_temp_vf3d_path(path, sizeof(path)));
    if (!path[0]) {
        RuntimeVolumeAttachment3D_Free(&attachment);
        return 0;
    }
    assert_true("fluid_volume_import_3d_valid_write",
                write_sample_vf3d(path, 1u, false, false));

    ok = fluid_volume_import_3d_load_raw(path, &attachment, diagnostics, sizeof(diagnostics));
    assert_true("fluid_volume_import_3d_valid_load_ok", ok);
    assert_true("fluid_volume_import_3d_valid_diag_ok",
                strcmp(diagnostics, "ok") == 0);
    assert_true("fluid_volume_import_3d_valid_source_raw",
                attachment.sourceKind == RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D);
    assert_true("fluid_volume_import_3d_valid_enabled", attachment.enabled);
    assert_true("fluid_volume_import_3d_valid_has_data", attachment.hasData);
    assert_true("fluid_volume_import_3d_valid_grid",
                RuntimeVolumeGrid3D_IsConfigured(&attachment.grid));
    assert_true("fluid_volume_import_3d_valid_dims_w", attachment.grid.gridW == 2u);
    assert_true("fluid_volume_import_3d_valid_dims_d", attachment.grid.gridD == 2u);
    assert_true("fluid_volume_import_3d_valid_cell_count", attachment.grid.cellCount == 8u);
    assert_close("fluid_volume_import_3d_valid_origin_y",
                 attachment.grid.origin.y, 2.0, 1e-9);
    assert_close("fluid_volume_import_3d_valid_bounds_max_z",
                 attachment.grid.boundsMax.z, 3.5, 1e-9);
    assert_true("fluid_volume_import_3d_valid_density_value",
                attachment.channels.density && attachment.channels.density[7] == 0.8f);
    assert_true("fluid_volume_import_3d_valid_velz_value",
                attachment.channels.velocityZ && attachment.channels.velocityZ[3] == 2.3f);
    assert_true("fluid_volume_import_3d_valid_pressure_value",
                attachment.channels.pressure && attachment.channels.pressure[0] == 3.0f);
    assert_true("fluid_volume_import_3d_valid_solid_value",
                attachment.channels.solidMask && attachment.channels.solidMask[1] == 1u);

    RuntimeVolumeAttachment3D_Free(&attachment);
    unlink(path);
    return 0;
}

static int test_fluid_volume_import_3d_rejects_unsupported_version(void) {
    char path[PATH_MAX];
    RuntimeVolumeAttachment3D attachment;
    char diagnostics[128] = {0};
    bool ok = false;

    RuntimeVolumeAttachment3D_Init(&attachment);
    assert_true("fluid_volume_import_3d_bad_version_temp_path",
                make_temp_vf3d_path(path, sizeof(path)));
    if (!path[0]) {
        RuntimeVolumeAttachment3D_Free(&attachment);
        return 0;
    }
    assert_true("fluid_volume_import_3d_bad_version_write",
                write_sample_vf3d(path, 99u, false, false));

    ok = fluid_volume_import_3d_load_raw(path, &attachment, diagnostics, sizeof(diagnostics));
    assert_true("fluid_volume_import_3d_bad_version_rejected", !ok);
    assert_true("fluid_volume_import_3d_bad_version_diag",
                strcmp(diagnostics, "unsupported raw vf3d version") == 0);
    assert_true("fluid_volume_import_3d_bad_version_no_data", !attachment.hasData);

    RuntimeVolumeAttachment3D_Free(&attachment);
    unlink(path);
    return 0;
}

static int test_fluid_volume_import_3d_rejects_truncated_payload(void) {
    char path[PATH_MAX];
    RuntimeVolumeAttachment3D attachment;
    char diagnostics[128] = {0};
    bool ok = false;

    RuntimeVolumeAttachment3D_Init(&attachment);
    assert_true("fluid_volume_import_3d_truncated_temp_path",
                make_temp_vf3d_path(path, sizeof(path)));
    if (!path[0]) {
        RuntimeVolumeAttachment3D_Free(&attachment);
        return 0;
    }
    assert_true("fluid_volume_import_3d_truncated_write",
                write_sample_vf3d(path, 1u, true, false));

    ok = fluid_volume_import_3d_load_raw(path, &attachment, diagnostics, sizeof(diagnostics));
    assert_true("fluid_volume_import_3d_truncated_rejected", !ok);
    assert_true("fluid_volume_import_3d_truncated_diag",
                strcmp(diagnostics, "failed to read raw vf3d payload") == 0);
    assert_true("fluid_volume_import_3d_truncated_layout_reset",
                !RuntimeVolumeGrid3D_IsConfigured(&attachment.grid));
    assert_true("fluid_volume_import_3d_truncated_no_density",
                attachment.channels.density == NULL);

    RuntimeVolumeAttachment3D_Free(&attachment);
    unlink(path);
    return 0;
}

static int test_fluid_volume_import_3d_rejects_zero_scene_up(void) {
    char path[PATH_MAX] = {0};
    RuntimeVolumeAttachment3D attachment;
    char diagnostics[128] = {0};
    bool ok = false;

    RuntimeVolumeAttachment3D_Init(&attachment);
    assert_true("fluid_volume_import_3d_zero_scene_up_temp_path",
                make_temp_vf3d_path(path, sizeof(path)));
    if (!path[0]) {
        RuntimeVolumeAttachment3D_Free(&attachment);
        return 0;
    }
    assert_true("fluid_volume_import_3d_zero_scene_up_write",
                write_sample_vf3d(path, 1u, false, true));

    ok = fluid_volume_import_3d_load_raw(path, &attachment, diagnostics, sizeof(diagnostics));
    assert_true("fluid_volume_import_3d_zero_scene_up_rejected", !ok);
    assert_true("fluid_volume_import_3d_zero_scene_up_diag",
                strcmp(diagnostics, "raw vf3d scene_up must be non-zero") == 0);
    assert_true("fluid_volume_import_3d_zero_scene_up_no_grid",
                !RuntimeVolumeGrid3D_IsConfigured(&attachment.grid));

    RuntimeVolumeAttachment3D_Free(&attachment);
    unlink(path);
    return 0;
}

static int test_fluid_volume_import_3d_loads_manifest_backed_raw_source(void) {
    char dir[PATH_MAX] = {0};
    char vf3d_path[PATH_MAX] = {0};
    char manifest_path[PATH_MAX] = {0};
    RuntimeVolumeAttachment3D attachment;
    RuntimeVolumeDebugSummary3D summary;
    char diagnostics[128] = {0};
    bool ok = false;
    const char* manifest_json =
        "{\n"
        "  \"manifest_version\": 2,\n"
        "  \"frame_contract\": \"vf3d\",\n"
        "  \"space_mode\": \"3d\",\n"
        "  \"frames\": [\n"
        "    {\n"
        "      \"frame_index\": 9,\n"
        "      \"time_seconds\": 1.5,\n"
        "      \"dt_seconds\": 0.016,\n"
        "      \"path\": \"frame_000009.vf3d\",\n"
        "      \"frame_contract\": \"vf3d\"\n"
        "    }\n"
        "  ]\n"
        "}\n";

    RuntimeVolumeAttachment3D_Init(&attachment);
    assert_true("fluid_volume_import_3d_manifest_raw_temp_dir",
                make_temp_volume_dir(dir, sizeof(dir)));
    assert_true("fluid_volume_import_3d_manifest_raw_vf3d_path",
                snprintf(vf3d_path, sizeof(vf3d_path), "%s/frame_000009.vf3d", dir) <
                    (int)sizeof(vf3d_path));
    assert_true("fluid_volume_import_3d_manifest_raw_manifest_path",
                snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", dir) <
                    (int)sizeof(manifest_path));
    assert_true("fluid_volume_import_3d_manifest_raw_write_vf3d",
                write_sample_vf3d(vf3d_path, 1u, false, false));
    assert_true("fluid_volume_import_3d_manifest_raw_write_manifest",
                write_local_text_file(manifest_path, manifest_json));

    ok = fluid_volume_import_3d_load_source(manifest_path,
                                            RUNTIME_VOLUME_3D_SOURCE_MANIFEST,
                                            &attachment,
                                            diagnostics,
                                            sizeof(diagnostics));
    assert_true("fluid_volume_import_3d_manifest_raw_ok", ok);
    assert_true("fluid_volume_import_3d_manifest_raw_source_kind",
                attachment.sourceKind == RUNTIME_VOLUME_3D_SOURCE_RAW_VF3D);
    assert_true("fluid_volume_import_3d_manifest_raw_has_density",
                RuntimeVolumeAttachment3D_HasChannel(&attachment, RUNTIME_VOLUME_3D_CHANNEL_DENSITY));
    assert_true("fluid_volume_import_3d_manifest_raw_density_value",
                attachment.channels.density && attachment.channels.density[4] == 0.5f);
    ok = RuntimeVolumeDebugSummary3D_Build(&attachment, &summary);
    assert_true("fluid_volume_import_3d_manifest_raw_summary_ok", ok);
    assert_true("fluid_volume_import_3d_manifest_raw_summary_layout_valid",
                summary.layoutValid);
    assert_true("fluid_volume_import_3d_manifest_raw_summary_density_range",
                summary.hasDensityRange);
    assert_true("fluid_volume_import_3d_manifest_raw_summary_nonzero_count",
                summary.densityNonZeroCellCount == 8u);
    assert_close("fluid_volume_import_3d_manifest_raw_summary_density_max",
                 summary.densityMax, 0.8, 1e-6);

    RuntimeVolumeAttachment3D_Free(&attachment);
    unlink(manifest_path);
    unlink(vf3d_path);
    rmdir(dir);
    return 0;
}

static int test_fluid_volume_import_3d_manifest_prefers_first_frame_by_default(void) {
    char dir[PATH_MAX] = {0};
    char first_vf3d_path[PATH_MAX] = {0};
    char second_vf3d_path[PATH_MAX] = {0};
    char manifest_path[PATH_MAX] = {0};
    RuntimeVolumeAttachment3D attachment;
    char diagnostics[128] = {0};
    bool ok = false;
    const char* manifest_json =
        "{\n"
        "  \"manifest_version\": 2,\n"
        "  \"frame_contract\": \"vf3d\",\n"
        "  \"space_mode\": \"3d\",\n"
        "  \"frames\": [\n"
        "    {\n"
        "      \"frame_index\": 1,\n"
        "      \"path\": \"frame_000001.vf3d\",\n"
        "      \"frame_contract\": \"vf3d\"\n"
        "    },\n"
        "    {\n"
        "      \"frame_index\": 2,\n"
        "      \"path\": \"frame_000002.vf3d\",\n"
        "      \"frame_contract\": \"vf3d\"\n"
        "    }\n"
        "  ]\n"
        "}\n";

    RuntimeVolumeAttachment3D_Init(&attachment);
    assert_true("fluid_volume_import_3d_manifest_first_frame_temp_dir",
                make_temp_volume_dir(dir, sizeof(dir)));
    assert_true("fluid_volume_import_3d_manifest_first_frame_path_a",
                snprintf(first_vf3d_path, sizeof(first_vf3d_path), "%s/frame_000001.vf3d", dir) <
                    (int)sizeof(first_vf3d_path));
    assert_true("fluid_volume_import_3d_manifest_first_frame_path_b",
                snprintf(second_vf3d_path, sizeof(second_vf3d_path), "%s/frame_000002.vf3d", dir) <
                    (int)sizeof(second_vf3d_path));
    assert_true("fluid_volume_import_3d_manifest_first_frame_manifest_path",
                snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", dir) <
                    (int)sizeof(manifest_path));
    assert_true("fluid_volume_import_3d_manifest_first_frame_write_a",
                write_sample_vf3d_custom(first_vf3d_path, 1u, false, false, 0.0f, 1u));
    assert_true("fluid_volume_import_3d_manifest_first_frame_write_b",
                write_sample_vf3d_custom(second_vf3d_path, 1u, false, false, 1.0f, 2u));
    assert_true("fluid_volume_import_3d_manifest_first_frame_write_manifest",
                write_local_text_file(manifest_path, manifest_json));

    ok = fluid_volume_import_3d_load_source(manifest_path,
                                            RUNTIME_VOLUME_3D_SOURCE_MANIFEST,
                                            &attachment,
                                            diagnostics,
                                            sizeof(diagnostics));
    assert_true("fluid_volume_import_3d_manifest_first_frame_ok", ok);
    assert_true("fluid_volume_import_3d_manifest_first_frame_density_from_first",
                attachment.channels.density && attachment.channels.density[4] == 0.5f);
    assert_true("fluid_volume_import_3d_manifest_first_frame_not_second",
                attachment.channels.density && attachment.channels.density[4] != 1.5f);

    RuntimeVolumeAttachment3D_Free(&attachment);
    unlink(manifest_path);
    unlink(first_vf3d_path);
    unlink(second_vf3d_path);
    rmdir(dir);
    return 0;
}

static int test_fluid_volume_import_3d_manifest_selects_requested_frame_index(void) {
    char dir[PATH_MAX] = {0};
    char first_vf3d_path[PATH_MAX] = {0};
    char second_vf3d_path[PATH_MAX] = {0};
    char manifest_path[PATH_MAX] = {0};
    RuntimeVolumeAttachment3D attachment;
    char diagnostics[128] = {0};
    bool ok = false;
    const char* manifest_json =
        "{\n"
        "  \"manifest_version\": 2,\n"
        "  \"frame_contract\": \"vf3d\",\n"
        "  \"space_mode\": \"3d\",\n"
        "  \"frames\": [\n"
        "    {\n"
        "      \"frame_index\": 1,\n"
        "      \"path\": \"frame_000001.vf3d\",\n"
        "      \"frame_contract\": \"vf3d\"\n"
        "    },\n"
        "    {\n"
        "      \"frame_index\": 2,\n"
        "      \"path\": \"frame_000002.vf3d\",\n"
        "      \"frame_contract\": \"vf3d\"\n"
        "    }\n"
        "  ]\n"
        "}\n";

    RuntimeVolumeAttachment3D_Init(&attachment);
    assert_true("fluid_volume_import_3d_manifest_index_frame_temp_dir",
                make_temp_volume_dir(dir, sizeof(dir)));
    assert_true("fluid_volume_import_3d_manifest_index_frame_path_a",
                snprintf(first_vf3d_path, sizeof(first_vf3d_path), "%s/frame_000001.vf3d", dir) <
                    (int)sizeof(first_vf3d_path));
    assert_true("fluid_volume_import_3d_manifest_index_frame_path_b",
                snprintf(second_vf3d_path, sizeof(second_vf3d_path), "%s/frame_000002.vf3d", dir) <
                    (int)sizeof(second_vf3d_path));
    assert_true("fluid_volume_import_3d_manifest_index_frame_manifest_path",
                snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", dir) <
                    (int)sizeof(manifest_path));
    assert_true("fluid_volume_import_3d_manifest_index_frame_write_a",
                write_sample_vf3d_custom(first_vf3d_path, 1u, false, false, 0.0f, 1u));
    assert_true("fluid_volume_import_3d_manifest_index_frame_write_b",
                write_sample_vf3d_custom(second_vf3d_path, 1u, false, false, 1.0f, 2u));
    assert_true("fluid_volume_import_3d_manifest_index_frame_write_manifest",
                write_local_text_file(manifest_path, manifest_json));

    ok = fluid_volume_import_3d_load_source_at_frame(manifest_path,
                                                     RUNTIME_VOLUME_3D_SOURCE_MANIFEST,
                                                     2,
                                                     &attachment,
                                                     diagnostics,
                                                     sizeof(diagnostics));
    assert_true("fluid_volume_import_3d_manifest_index_frame_ok", ok);
    assert_true("fluid_volume_import_3d_manifest_index_frame_density_from_second",
                attachment.channels.density && attachment.channels.density[4] == 1.5f);
    assert_true("fluid_volume_import_3d_manifest_index_frame_not_first",
                attachment.channels.density && attachment.channels.density[4] != 0.5f);

    RuntimeVolumeAttachment3D_Free(&attachment);
    unlink(manifest_path);
    unlink(first_vf3d_path);
    unlink(second_vf3d_path);
    rmdir(dir);
    return 0;
}

static int test_fluid_volume_import_3d_scene_bundle_manifest_selects_requested_frame_index(void) {
    char dir[PATH_MAX] = {0};
    char first_vf3d_path[PATH_MAX] = {0};
    char second_vf3d_path[PATH_MAX] = {0};
    char manifest_path[PATH_MAX] = {0};
    char bundle_path[PATH_MAX] = {0};
    char selected_path[PATH_MAX] = {0};
    RuntimeVolumeAttachment3D attachment;
    char diagnostics[128] = {0};
    bool ok = false;
    const char* manifest_json =
        "{\n"
        "  \"manifest_version\": 2,\n"
        "  \"frame_contract\": \"vf3d\",\n"
        "  \"space_mode\": \"3d\",\n"
        "  \"frames\": [\n"
        "    {\n"
        "      \"frame_index\": 1,\n"
        "      \"path\": \"frame_000001.vf3d\",\n"
        "      \"frame_contract\": \"vf3d\"\n"
        "    },\n"
        "    {\n"
        "      \"frame_index\": 2,\n"
        "      \"path\": \"frame_000002.vf3d\",\n"
        "      \"frame_contract\": \"vf3d\"\n"
        "    }\n"
        "  ]\n"
        "}\n";
    const char* bundle_json =
        "{\n"
        "  \"bundle_type\": \"physics_scene_bundle_v1\",\n"
        "  \"bundle_version\": 1,\n"
        "  \"profile\": \"physics\",\n"
        "  \"fluid_source\": {\n"
        "    \"kind\": \"manifest\",\n"
        "    \"path\": \"manifest.json\"\n"
        "  }\n"
        "}\n";

    RuntimeVolumeAttachment3D_Init(&attachment);
    assert_true("fluid_volume_import_3d_bundle_manifest_index_temp_dir",
                make_temp_volume_dir(dir, sizeof(dir)));
    assert_true("fluid_volume_import_3d_bundle_manifest_index_frame_path_a",
                snprintf(first_vf3d_path, sizeof(first_vf3d_path), "%s/frame_000001.vf3d", dir) <
                    (int)sizeof(first_vf3d_path));
    assert_true("fluid_volume_import_3d_bundle_manifest_index_frame_path_b",
                snprintf(second_vf3d_path, sizeof(second_vf3d_path), "%s/frame_000002.vf3d", dir) <
                    (int)sizeof(second_vf3d_path));
    assert_true("fluid_volume_import_3d_bundle_manifest_index_manifest_path",
                snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", dir) <
                    (int)sizeof(manifest_path));
    assert_true("fluid_volume_import_3d_bundle_manifest_index_bundle_path",
                snprintf(bundle_path, sizeof(bundle_path), "%s/scene_bundle.json", dir) <
                    (int)sizeof(bundle_path));
    assert_true("fluid_volume_import_3d_bundle_manifest_index_write_a",
                write_sample_vf3d_custom(first_vf3d_path, 1u, false, false, 0.0f, 1u));
    assert_true("fluid_volume_import_3d_bundle_manifest_index_write_b",
                write_sample_vf3d_custom(second_vf3d_path, 1u, false, false, 1.0f, 2u));
    assert_true("fluid_volume_import_3d_bundle_manifest_index_write_manifest",
                write_local_text_file(manifest_path, manifest_json));
    assert_true("fluid_volume_import_3d_bundle_manifest_index_write_bundle",
                write_local_text_file(bundle_path, bundle_json));

    ok = fluid_volume_import_3d_resolve_source_frame_path(bundle_path,
                                                          RUNTIME_VOLUME_3D_SOURCE_MANIFEST,
                                                          2,
                                                          selected_path,
                                                          sizeof(selected_path),
                                                          diagnostics,
                                                          sizeof(diagnostics));
    assert_true("fluid_volume_import_3d_bundle_manifest_index_resolve_ok", ok);
    assert_true("fluid_volume_import_3d_bundle_manifest_index_resolve_second",
                strstr(selected_path, "frame_000002.vf3d") != NULL);

    ok = fluid_volume_import_3d_load_source_at_frame(bundle_path,
                                                     RUNTIME_VOLUME_3D_SOURCE_MANIFEST,
                                                     2,
                                                     &attachment,
                                                     diagnostics,
                                                     sizeof(diagnostics));
    assert_true("fluid_volume_import_3d_bundle_manifest_index_load_ok", ok);
    assert_true("fluid_volume_import_3d_bundle_manifest_index_density_from_second",
                attachment.channels.density && attachment.channels.density[4] == 1.5f);
    assert_true("fluid_volume_import_3d_bundle_manifest_index_frame_header",
                attachment.grid.frameIndex == 2u);

    RuntimeVolumeAttachment3D_Free(&attachment);
    unlink(bundle_path);
    unlink(manifest_path);
    unlink(first_vf3d_path);
    unlink(second_vf3d_path);
    rmdir(dir);
    return 0;
}

static int test_fluid_volume_import_3d_loads_scene_bundle_pack_source(void) {
    char dir[PATH_MAX] = {0};
    char pack_path[PATH_MAX] = {0};
    char bundle_path[PATH_MAX] = {0};
    RuntimeVolumeAttachment3D attachment;
    RuntimeVolumeDebugSummary3D summary;
    char diagnostics[128] = {0};
    bool ok = false;
    const char* bundle_json =
        "{\n"
        "  \"bundle_type\": \"physics_scene_bundle_v1\",\n"
        "  \"bundle_version\": 1,\n"
        "  \"profile\": \"physics\",\n"
        "  \"fluid_source\": {\n"
        "    \"kind\": \"pack\",\n"
        "    \"path\": \"frame_000017.pack\"\n"
        "  }\n"
        "}\n";

    RuntimeVolumeAttachment3D_Init(&attachment);
    assert_true("fluid_volume_import_3d_bundle_pack_temp_dir",
                make_temp_volume_dir(dir, sizeof(dir)));
    assert_true("fluid_volume_import_3d_bundle_pack_pack_path",
                snprintf(pack_path, sizeof(pack_path), "%s/frame_000017.pack", dir) <
                    (int)sizeof(pack_path));
    assert_true("fluid_volume_import_3d_bundle_pack_bundle_path",
                snprintf(bundle_path, sizeof(bundle_path), "%s/scene_bundle.json", dir) <
                    (int)sizeof(bundle_path));
    assert_true("fluid_volume_import_3d_bundle_pack_write_pack",
                write_sample_vf3d_pack(pack_path));
    assert_true("fluid_volume_import_3d_bundle_pack_write_bundle",
                write_local_text_file(bundle_path, bundle_json));

    ok = fluid_volume_import_3d_load_source(bundle_path,
                                            RUNTIME_VOLUME_3D_SOURCE_MANIFEST,
                                            &attachment,
                                            diagnostics,
                                            sizeof(diagnostics));
    assert_true("fluid_volume_import_3d_bundle_pack_ok", ok);
    assert_true("fluid_volume_import_3d_bundle_pack_source_kind",
                attachment.sourceKind == RUNTIME_VOLUME_3D_SOURCE_PACK);
    assert_true("fluid_volume_import_3d_bundle_pack_density_value",
                attachment.channels.density && attachment.channels.density[6] == 0.77f);
    ok = RuntimeVolumeDebugSummary3D_Build(&attachment, &summary);
    assert_true("fluid_volume_import_3d_bundle_pack_summary_ok", ok);
    assert_true("fluid_volume_import_3d_bundle_pack_summary_has_density",
                summary.hasDensity);
    assert_true("fluid_volume_import_3d_bundle_pack_summary_density_range",
                summary.hasDensityRange);
    assert_true("fluid_volume_import_3d_bundle_pack_summary_nonzero_count",
                summary.densityNonZeroCellCount == 8u);
    assert_close("fluid_volume_import_3d_bundle_pack_summary_density_min",
                 summary.densityMin, 0.11, 1e-9);
    assert_close("fluid_volume_import_3d_bundle_pack_summary_density_max",
                 summary.densityMax, 0.88, 1e-6);

    RuntimeVolumeAttachment3D_Free(&attachment);
    unlink(bundle_path);
    unlink(pack_path);
    rmdir(dir);
    return 0;
}

static int test_fluid_volume_import_3d_rejects_non_vf3d_manifest(void) {
    char dir[PATH_MAX] = {0};
    char manifest_path[PATH_MAX] = {0};
    RuntimeVolumeAttachment3D attachment;
    char diagnostics[128] = {0};
    bool ok = false;
    const char* manifest_json =
        "{\n"
        "  \"manifest_version\": 2,\n"
        "  \"frame_contract\": \"vf2d\",\n"
        "  \"space_mode\": \"2d\",\n"
        "  \"frames\": []\n"
        "}\n";

    RuntimeVolumeAttachment3D_Init(&attachment);
    assert_true("fluid_volume_import_3d_bad_manifest_temp_dir",
                make_temp_volume_dir(dir, sizeof(dir)));
    assert_true("fluid_volume_import_3d_bad_manifest_path",
                snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", dir) <
                    (int)sizeof(manifest_path));
    assert_true("fluid_volume_import_3d_bad_manifest_write",
                write_local_text_file(manifest_path, manifest_json));

    ok = fluid_volume_import_3d_load_source(manifest_path,
                                            RUNTIME_VOLUME_3D_SOURCE_MANIFEST,
                                            &attachment,
                                            diagnostics,
                                            sizeof(diagnostics));
    assert_true("fluid_volume_import_3d_bad_manifest_rejected", !ok);
    assert_true("fluid_volume_import_3d_bad_manifest_diag",
                strcmp(diagnostics, "vf3d manifest frame_contract mismatch") == 0);

    RuntimeVolumeAttachment3D_Free(&attachment);
    unlink(manifest_path);
    rmdir(dir);
    return 0;
}

static int test_water_surface_import_loads_scene_bundle_heightfield(void) {
    char dir[PATH_MAX] = {0};
    char bundle_path[PATH_MAX] = {0};
    char water_manifest_path[PATH_MAX] = {0};
    char frame0_path[PATH_MAX] = {0};
    char frame1_path[PATH_MAX] = {0};
    RuntimeWaterSurfaceFrame frame;
    char diagnostics[256] = {0};
    bool found = false;
    bool ok = false;
    const char* bundle_json =
        "{\n"
        "  \"bundle_type\": \"physics_scene_bundle_v1\",\n"
        "  \"fluid_source\": { \"kind\": \"manifest\", \"path\": \"manifest.json\", \"contract\": \"vf3d\" },\n"
        "  \"water_source\": {\n"
        "    \"kind\": \"water_manifest\",\n"
        "    \"path\": \"water_manifest_v1.json\",\n"
        "    \"contract\": \"water_manifest_v1\",\n"
        "    \"surface_representation\": \"heightfield\"\n"
        "  }\n"
        "}\n";
    const char* manifest_json =
        "{\n"
        "  \"schema\": \"physics_sim_water_manifest_v1\",\n"
        "  \"version\": 1,\n"
        "  \"frame_contract\": \"water_surface_heightfield_v1\",\n"
        "  \"surface_representation\": \"heightfield\",\n"
        "  \"surface_axis\": \"y\",\n"
        "  \"volume_grid_w\": 3,\n"
        "  \"volume_grid_h\": 3,\n"
        "  \"volume_grid_d\": 3,\n"
        "  \"density_threshold\": 0.5,\n"
        "  \"material\": {\n"
        "    \"ior\": 1.333,\n"
        "    \"absorption_distance_m\": 4.0,\n"
        "    \"absorption_rgb\": [0.1, 0.035, 0.015]\n"
        "  },\n"
        "  \"frames\": [\n"
        "    { \"path\": \"water_surface_000000.json\", \"frame_contract\": \"water_surface_heightfield_v1\" },\n"
        "    { \"path\": \"water_surface_000001.json\", \"frame_contract\": \"water_surface_heightfield_v1\" }\n"
        "  ]\n"
        "}\n";
    const char* frame0_json =
        "{\n"
        "  \"schema\": \"physics_sim_water_surface_heightfield_v1\",\n"
        "  \"version\": 1,\n"
        "  \"frame_contract\": \"water_surface_heightfield_v1\",\n"
        "  \"frame_index\": 0,\n"
        "  \"time_seconds\": 0.0,\n"
        "  \"dt_seconds\": 0.016,\n"
        "  \"layout\": \"row_major_z_x\",\n"
        "  \"surface_axis\": \"y\",\n"
        "  \"grid_w\": 3,\n"
        "  \"grid_d\": 3,\n"
        "  \"sample_count\": 9,\n"
        "  \"origin_x\": -1.0,\n"
        "  \"origin_y\": 0.0,\n"
        "  \"origin_z\": -1.0,\n"
        "  \"sample_origin_x\": -1.0,\n"
        "  \"sample_origin_z\": -1.0,\n"
        "  \"sample_spacing_x\": 0.5,\n"
        "  \"sample_spacing_z\": 0.5,\n"
        "  \"summary\": {\n"
        "    \"wet_columns\": 4,\n"
        "    \"dry_columns\": 5,\n"
        "    \"solid_columns\": 0,\n"
        "    \"water_cells\": 12,\n"
        "    \"surface_min_y\": 0.0,\n"
        "    \"surface_max_y\": 0.5,\n"
        "    \"surface_avg_y\": 0.22,\n"
        "    \"max_slope\": 0.4,\n"
        "    \"finite_normals\": true\n"
        "  },\n"
        "  \"heights_y\": [0.0,0.0,0.0, 0.0,0.4,0.5, 0.0,0.5,0.5],\n"
        "  \"normals_xyz\": [0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0]\n"
        "}\n";
    const char* frame1_json =
        "{\n"
        "  \"schema\": \"physics_sim_water_surface_heightfield_v1\",\n"
        "  \"version\": 1,\n"
        "  \"frame_contract\": \"water_surface_heightfield_v1\",\n"
        "  \"frame_index\": 1,\n"
        "  \"time_seconds\": 0.016,\n"
        "  \"dt_seconds\": 0.016,\n"
        "  \"layout\": \"row_major_z_x\",\n"
        "  \"surface_axis\": \"y\",\n"
        "  \"grid_w\": 3,\n"
        "  \"grid_d\": 3,\n"
        "  \"sample_count\": 9,\n"
        "  \"origin_x\": -1.0,\n"
        "  \"origin_y\": 0.0,\n"
        "  \"origin_z\": -1.0,\n"
        "  \"sample_origin_x\": -1.0,\n"
        "  \"sample_origin_z\": -1.0,\n"
        "  \"sample_spacing_x\": 0.5,\n"
        "  \"sample_spacing_z\": 0.5,\n"
        "  \"summary\": {\n"
        "    \"wet_columns\": 5,\n"
        "    \"dry_columns\": 4,\n"
        "    \"solid_columns\": 0,\n"
        "    \"water_cells\": 15,\n"
        "    \"surface_min_y\": 0.0,\n"
        "    \"surface_max_y\": 0.6,\n"
        "    \"surface_avg_y\": 0.27,\n"
        "    \"max_slope\": 0.5,\n"
        "    \"finite_normals\": true\n"
        "  },\n"
        "  \"heights_y\": [0.0,0.0,0.0, 0.0,0.45,0.55, 0.0,0.55,0.6],\n"
        "  \"normals_xyz\": [0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0]\n"
        "}\n";

    RuntimeWaterSurfaceFrame_Init(&frame);
    assert_true("water_surface_import_temp_dir", make_temp_volume_dir(dir, sizeof(dir)));
    assert_true("water_surface_import_bundle_path",
                snprintf(bundle_path, sizeof(bundle_path), "%s/scene_bundle.json", dir) <
                    (int)sizeof(bundle_path));
    assert_true("water_surface_import_manifest_path",
                snprintf(water_manifest_path,
                         sizeof(water_manifest_path),
                         "%s/water_manifest_v1.json",
                         dir) < (int)sizeof(water_manifest_path));
    assert_true("water_surface_import_frame0_path",
                snprintf(frame0_path, sizeof(frame0_path), "%s/water_surface_000000.json", dir) <
                    (int)sizeof(frame0_path));
    assert_true("water_surface_import_frame1_path",
                snprintf(frame1_path, sizeof(frame1_path), "%s/water_surface_000001.json", dir) <
                    (int)sizeof(frame1_path));
    assert_true("water_surface_import_write_bundle", write_local_text_file(bundle_path, bundle_json));
    assert_true("water_surface_import_write_manifest",
                write_local_text_file(water_manifest_path, manifest_json));
    assert_true("water_surface_import_write_frame0", write_local_text_file(frame0_path, frame0_json));
    assert_true("water_surface_import_write_frame1", write_local_text_file(frame1_path, frame1_json));

    ok = RuntimeWaterSurfaceImport_LoadSourceAtFrame(bundle_path,
                                                     7,
                                                     &frame,
                                                     &found,
                                                     diagnostics,
                                                     sizeof(diagnostics));
    assert_true("water_surface_import_bundle_ok", ok);
    assert_true("water_surface_import_bundle_found", found);
    assert_true("water_surface_import_bundle_valid", frame.valid);
    assert_true("water_surface_import_bundle_frame_clamped", frame.frame_index == 1u);
    assert_true("water_surface_import_bundle_grid_w", frame.grid_w == 3u);
    assert_true("water_surface_import_bundle_grid_d", frame.grid_d == 3u);
    assert_true("water_surface_import_bundle_samples", frame.sample_count == 9u);
    assert_true("water_surface_import_bundle_heights", frame.heights_y != NULL);
    assert_true("water_surface_import_bundle_normals", frame.normals_xyz != NULL);
    if (!ok || !found || !frame.valid || !frame.heights_y || !frame.normals_xyz) {
        RuntimeWaterSurfaceFrame_Free(&frame);
        unlink(bundle_path);
        unlink(water_manifest_path);
        unlink(frame0_path);
        unlink(frame1_path);
        rmdir(dir);
        return 0;
    }
    assert_close("water_surface_import_bundle_height_sample", frame.heights_y[8], 0.6, 1e-6);
    assert_true("water_surface_import_bundle_manifest_path",
                strstr(frame.source_manifest_path, "water_manifest_v1.json") != NULL);
    assert_true("water_surface_import_bundle_frame_path",
                strstr(frame.frame_path, "water_surface_000001.json") != NULL);
    assert_close("water_surface_import_bundle_material_ior", frame.material.ior, 1.333, 1e-6);
    assert_close("water_surface_import_bundle_material_absorption_distance",
                 frame.material.absorption_distance_m,
                 4.0,
                 1e-9);
    assert_true("water_surface_import_bundle_wet_columns", frame.wet_columns == 5u);
    assert_true("water_surface_import_bundle_finite_normals", frame.finite_normals);

    RuntimeWaterSurfaceFrame_Free(&frame);
    unlink(bundle_path);
    unlink(water_manifest_path);
    unlink(frame0_path);
    unlink(frame1_path);
    rmdir(dir);
    return 0;
}

static int test_water_surface_import_ignores_bundle_without_water_source(void) {
    char dir[PATH_MAX] = {0};
    char bundle_path[PATH_MAX] = {0};
    RuntimeWaterSurfaceFrame frame;
    char diagnostics[256] = {0};
    bool found = true;
    bool ok = false;
    const char* bundle_json =
        "{\n"
        "  \"bundle_type\": \"physics_scene_bundle_v1\",\n"
        "  \"fluid_source\": { \"kind\": \"manifest\", \"path\": \"manifest.json\", \"contract\": \"vf3d\" }\n"
        "}\n";

    RuntimeWaterSurfaceFrame_Init(&frame);
    assert_true("water_surface_import_missing_temp_dir", make_temp_volume_dir(dir, sizeof(dir)));
    assert_true("water_surface_import_missing_bundle_path",
                snprintf(bundle_path, sizeof(bundle_path), "%s/scene_bundle.json", dir) <
                    (int)sizeof(bundle_path));
    assert_true("water_surface_import_missing_write_bundle",
                write_local_text_file(bundle_path, bundle_json));

    ok = RuntimeWaterSurfaceImport_LoadSourceAtFrame(bundle_path,
                                                     0,
                                                     &frame,
                                                     &found,
                                                     diagnostics,
                                                     sizeof(diagnostics));
    assert_true("water_surface_import_missing_ok", ok);
    assert_true("water_surface_import_missing_not_found", !found);
    assert_true("water_surface_import_missing_not_valid", !frame.valid);

    RuntimeWaterSurfaceFrame_Free(&frame);
    unlink(bundle_path);
    rmdir(dir);
    return 0;
}

int run_test_fluid_volume_import_3d_tests(void) {
    int before = test_support_failures();

    test_fluid_volume_import_3d_classifies_paths();
    test_fluid_volume_import_3d_loads_valid_raw_file();
    test_fluid_volume_import_3d_rejects_unsupported_version();
    test_fluid_volume_import_3d_rejects_truncated_payload();
    test_fluid_volume_import_3d_rejects_zero_scene_up();
    test_fluid_volume_import_3d_loads_manifest_backed_raw_source();
    test_fluid_volume_import_3d_manifest_prefers_first_frame_by_default();
    test_fluid_volume_import_3d_manifest_selects_requested_frame_index();
    test_fluid_volume_import_3d_scene_bundle_manifest_selects_requested_frame_index();
    test_fluid_volume_import_3d_loads_scene_bundle_pack_source();
    test_fluid_volume_import_3d_rejects_non_vf3d_manifest();
    test_water_surface_import_loads_scene_bundle_heightfield();
    test_water_surface_import_ignores_bundle_without_water_source();
    return test_support_failures() - before;
}

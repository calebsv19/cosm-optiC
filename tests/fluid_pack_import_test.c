#include "fluid_pack_import_test.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "core_pack.h"
#include "import/fluid_import.h"
#include "import/fluid_pack_import.h"

typedef struct Vf2dHeaderCanonical {
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double time_seconds;
    uint64_t frame_index;
    double dt_seconds;
    float origin_x;
    float origin_y;
    float cell_size;
    uint32_t obstacle_mask_crc32;
} Vf2dHeaderCanonical;

static int fail(const char *name) {
    printf("FAIL %-32s\n", name);
    return 1;
}

static int write_sample_pack(const char *path) {
    Vf2dHeaderCanonical header;
    memset(&header, 0, sizeof(header));
    header.version = 2u;
    header.grid_w = 2u;
    header.grid_h = 2u;
    header.time_seconds = 1.25;
    header.frame_index = 42u;
    header.dt_seconds = 0.02;
    header.origin_x = 3.0f;
    header.origin_y = 4.0f;
    header.cell_size = 0.5f;
    header.obstacle_mask_crc32 = 1234u;

    float density[4] = { 0.1f, 0.2f, 0.3f, 0.4f };
    float velx[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    float vely[4] = { -1.0f, -2.0f, -3.0f, -4.0f };

    CorePackWriter writer;
    CoreResult wr = core_pack_writer_open(path, &writer);
    if (wr.code != CORE_OK) return 0;
    wr = core_pack_writer_add_chunk(&writer, "VFHD", &header, sizeof(header));
    if (wr.code == CORE_OK) wr = core_pack_writer_add_chunk(&writer, "DENS", density, sizeof(density));
    if (wr.code == CORE_OK) wr = core_pack_writer_add_chunk(&writer, "VELX", velx, sizeof(velx));
    if (wr.code == CORE_OK) wr = core_pack_writer_add_chunk(&writer, "VELY", vely, sizeof(vely));
    CoreResult close_r = core_pack_writer_close(&writer);
    return wr.code == CORE_OK && close_r.code == CORE_OK;
}

static int write_scene_bundle(const char *path, const char *relative_source) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = fprintf(f,
                     "{\n"
                     "  \"bundle_type\": \"physics_scene_bundle_v1\",\n"
                     "  \"bundle_version\": 1,\n"
                     "  \"profile\": \"physics\",\n"
                     "  \"fluid_source\": {\n"
                     "    \"kind\": \"pack\",\n"
                     "    \"path\": \"%s\"\n"
                     "  }\n"
                     "}\n",
                     relative_source) > 0;
    fclose(f);
    return ok;
}

static int write_manifest_with_space_contract(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = fprintf(f,
                     "{\n"
                     "  \"manifest_version\": 1,\n"
                     "  \"grid_w\": 10,\n"
                     "  \"grid_h\": 8,\n"
                     "  \"cell_size\": 1.0,\n"
                     "  \"origin_x\": 0.0,\n"
                     "  \"origin_y\": 0.0,\n"
                     "  \"space_contract\": {\n"
                     "    \"version\": 1,\n"
                     "    \"grid_w\": 20,\n"
                     "    \"grid_h\": 16,\n"
                     "    \"origin_x\": 2.5,\n"
                     "    \"origin_y\": 3.5,\n"
                     "    \"cell_size\": 0.5,\n"
                     "    \"author_window_w\": 1400,\n"
                     "    \"author_window_h\": 900,\n"
                     "    \"import_fit\": 0.2\n"
                     "  },\n"
                     "  \"frames\": [\n"
                     "    {\n"
                     "      \"frame_index\": 0,\n"
                     "      \"time_seconds\": 0.0,\n"
                     "      \"dt_seconds\": 0.016,\n"
                     "      \"path\": \"frame_000000.vf2d\"\n"
                     "    }\n"
                     "  ]\n"
                     "}\n") > 0;
    fclose(f);
    return ok;
}

int run_fluid_pack_import_tests(void) {
    int failures = 0;

    char tmp_template[] = "/tmp/rt_pack_testXXXXXX";
    int fd = mkstemp(tmp_template);
    if (fd < 0) {
        return fail("fluid_pack_mkstemp");
    }
    close(fd);

    char pack_path[PATH_MAX];
    if (snprintf(pack_path, sizeof(pack_path), "%s.pack", tmp_template) >= (int)sizeof(pack_path)) {
        unlink(tmp_template);
        return fail("fluid_pack_path_overflow");
    }
    if (rename(tmp_template, pack_path) != 0) {
        unlink(tmp_template);
        return fail("fluid_pack_rename");
    }

    if (!write_sample_pack(pack_path)) {
        unlink(pack_path);
        return fail("fluid_pack_write");
    }

    FluidFrame frame = {0};
    if (!fluid_pack_frame_load(pack_path, &frame)) {
        unlink(pack_path);
        return fail("fluid_pack_frame_load");
    }

    if (frame.w != 2 || frame.h != 2) failures += fail("fluid_pack_dims");
    if (!frame.density || !frame.velX || !frame.velY) failures += fail("fluid_pack_arrays");
    if (frame.meta.frame_index != 42u) failures += fail("fluid_pack_meta_frame");
    if (frame.meta.cell_size != 0.5f) failures += fail("fluid_pack_meta_cell");
    if (frame.density && frame.density[3] != 0.4f) failures += fail("fluid_pack_density_value");
    if (frame.velX && frame.velX[2] != 3.0f) failures += fail("fluid_pack_velx_value");
    if (frame.velY && frame.velY[1] != -2.0f) failures += fail("fluid_pack_vely_value");

    fluid_frame_free(&frame);

    char legacy_path[PATH_MAX];
    if (!fluid_pack_derive_legacy_vf2d_path(pack_path, legacy_path, sizeof(legacy_path))) {
        failures += fail("fluid_pack_legacy_path");
    } else if (!strstr(legacy_path, ".vf2d")) {
        failures += fail("fluid_pack_legacy_ext");
    }

    // Route test: fluid_frame_load should use pack path handling for .pack extension.
    FluidFrame routed = {0};
    if (!fluid_frame_load(pack_path, &routed)) {
        failures += fail("fluid_pack_routed_load");
    } else {
        if (routed.w != 2 || routed.h != 2) failures += fail("fluid_pack_routed_dims");
        fluid_frame_free(&routed);
    }

    // Scene bundle routing test.
    char bundle_path[PATH_MAX];
    if (snprintf(bundle_path, sizeof(bundle_path), "%s.scene_bundle.json", pack_path) < (int)sizeof(bundle_path)) {
        if (!write_scene_bundle(bundle_path, "test.pack")) {
            failures += fail("scene_bundle_write");
        } else {
            // Create a sibling copy named test.pack so relative bundle resolution can find it.
            char sibling_pack_path[PATH_MAX];
            if (snprintf(sibling_pack_path, sizeof(sibling_pack_path), "%s", bundle_path) < (int)sizeof(sibling_pack_path)) {
                char *slash = strrchr(sibling_pack_path, '/');
                if (slash) {
                    *(slash + 1) = '\0';
                    strncat(sibling_pack_path, "test.pack", sizeof(sibling_pack_path) - strlen(sibling_pack_path) - 1);
                    FILE *src = fopen(pack_path, "rb");
                    FILE *dst = fopen(sibling_pack_path, "wb");
                    if (src && dst) {
                        char buf[4096];
                        size_t n = 0;
                        while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
                            fwrite(buf, 1, n, dst);
                        }
                    }
                    if (src) fclose(src);
                    if (dst) fclose(dst);

                    FluidManifest bundle_manifest = {0};
                    if (!fluid_manifest_load(bundle_path, &bundle_manifest)) {
                        failures += fail("scene_bundle_manifest_load");
                    } else {
                        if (bundle_manifest.count != 1) failures += fail("scene_bundle_count");
                        fluid_manifest_free(&bundle_manifest);
                    }
                    unlink(sibling_pack_path);
                }
            }
            unlink(bundle_path);
        }
    }

    // Manifest space_contract parse test.
    char manifest_path[PATH_MAX];
    if (snprintf(manifest_path, sizeof(manifest_path), "%s.manifest.json", pack_path) < (int)sizeof(manifest_path)) {
        if (!write_manifest_with_space_contract(manifest_path)) {
            failures += fail("manifest_write_space_contract");
        } else {
            char frame_path[PATH_MAX];
            if (snprintf(frame_path, sizeof(frame_path), "%s", manifest_path) < (int)sizeof(frame_path)) {
                char *slash = strrchr(frame_path, '/');
                if (slash) {
                    *(slash + 1) = '\0';
                    strncat(frame_path, "frame_000000.vf2d", sizeof(frame_path) - strlen(frame_path) - 1);
                    FILE *placeholder = fopen(frame_path, "wb");
                    if (placeholder) fclose(placeholder);
                }
            }

            FluidManifest parsed = {0};
            if (!fluid_manifest_load(manifest_path, &parsed)) {
                failures += fail("manifest_space_contract_load");
            } else {
                if (parsed.space_contract_version != 1u) failures += fail("manifest_space_contract_version");
                if (parsed.grid_w != 20u || parsed.grid_h != 16u) failures += fail("manifest_space_contract_grid");
                if (parsed.origin_x != 2.5f || parsed.origin_y != 3.5f) failures += fail("manifest_space_contract_origin");
                if (parsed.cell_size != 0.5f) failures += fail("manifest_space_contract_cell");
                if (parsed.space_author_window_w != 1400 || parsed.space_author_window_h != 900) {
                    failures += fail("manifest_space_contract_window");
                }
                if (parsed.space_desired_fit != 0.2f) failures += fail("manifest_space_contract_fit");
                fluid_manifest_free(&parsed);
            }

            unlink(frame_path);
            unlink(manifest_path);
        }
    }

    unlink(pack_path);
    return failures;
}

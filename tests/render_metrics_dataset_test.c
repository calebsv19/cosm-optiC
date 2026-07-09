#include "render_metrics_dataset_test.h"

#include "export/render_metrics_dataset.h"

#include "core_io.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fail(const char *name) {
    printf("FAIL %s\n", name ? name : "render_metrics_dataset_test");
    return 1;
}

int run_render_metrics_dataset_tests(void) {
    int failures = 0;

    char tmp_template[] = "/tmp/rt_metrics_datasetXXXXXX";
    int fd = mkstemp(tmp_template);
    if (fd < 0) {
        return fail("render_metrics_dataset_mkstemp");
    }
    close(fd);

    char json_path[1024];
    if (snprintf(json_path, sizeof(json_path), "%s.json", tmp_template) >= (int)sizeof(json_path)) {
        unlink(tmp_template);
        return fail("render_metrics_dataset_path_overflow");
    }
    if (rename(tmp_template, json_path) != 0) {
        unlink(tmp_template);
        return fail("render_metrics_dataset_rename");
    }

    RayTracingRenderMetricsSnapshot snapshot = {0};
    snapshot.frames_rendered = 42;
    snapshot.loops_completed = 1;
    snapshot.runtime_seconds = 2.5;
    snapshot.scene_object_count = 7;
    snapshot.scene_rays = 2048;
    snapshot.target_fps = 60;
    snapshot.frame_duration_seconds = 1.0 / 60.0;
    snapshot.integrator_mode = 1;
    snapshot.integrator_mode_3d = 0;
    snapshot.route_family = 2;
    snapshot.integrator_uses_3d_catalog = true;
    snapshot.bounce_limit = 8;
    snapshot.path_samples_per_pixel = 4;
    snapshot.path_max_depth = 6;
    snapshot.use_tiled_renderer = true;
    snapshot.tile_size = 16;
    snapshot.light_intensity = 3.0;
    snapshot.cache_variance_cutoff = 0.35;
    snapshot.cache_halo_radius = 3.5;
    snapshot.environment_brightness = 0.1;
    snapshot.interactive_mode = false;
    snapshot.deep_render_mode = true;
    snapshot.bounce_mode = false;
    snprintf(snapshot.integrator_status_label,
             sizeof(snapshot.integrator_status_label),
             "%s",
             "integrator: 3D Direct Light");

    if (!ray_tracing_render_metrics_dataset_export_json(&snapshot, json_path)) {
        failures += fail("render_metrics_dataset_export");
    } else {
        CoreBuffer buffer = {0};
        if (core_io_read_all(json_path, &buffer).code != CORE_OK || !buffer.data || buffer.size == 0) {
            failures += fail("render_metrics_dataset_read_back");
        } else {
            cJSON *root = cJSON_Parse((const char *)buffer.data);
            if (!root) {
                failures += fail("render_metrics_dataset_parse_json");
                core_io_buffer_free(&buffer);
                unlink(json_path);
                return failures;
            }
            cJSON *profile = cJSON_GetObjectItem(root, "profile");
            if (!cJSON_IsString(profile) ||
                strcmp(profile->valuestring, "ray_tracing_render_metrics_v2") != 0) {
                failures += fail("render_metrics_dataset_profile");
            }
            cJSON *schema_family = cJSON_GetObjectItem(root, "schema_family");
            if (!cJSON_IsString(schema_family) ||
                strcmp(schema_family->valuestring, "ray_tracing_render_metrics") != 0) {
                failures += fail("render_metrics_dataset_schema_family");
            }
            cJSON *schema_variant = cJSON_GetObjectItem(root, "schema_variant");
            if (!cJSON_IsString(schema_variant) ||
                strcmp(schema_variant->valuestring, "runtime") != 0) {
                failures += fail("render_metrics_dataset_schema_variant");
            }
            cJSON *items = cJSON_GetObjectItem(root, "items");
            if (!cJSON_IsArray(items)) {
                failures += fail("render_metrics_dataset_items");
            }
            cJSON *metadata = cJSON_GetObjectItem(root, "metadata");
            cJSON *status_label = metadata ? cJSON_GetObjectItem(metadata, "integrator_status_label") : NULL;
            if (!cJSON_IsString(status_label) ||
                strcmp(status_label->valuestring, "integrator: 3D Direct Light") != 0) {
                failures += fail("render_metrics_dataset_integrator_status_label");
            }
            if (!strstr((const char *)buffer.data, "render_metrics_table_v2")) {
                failures += fail("render_metrics_dataset_table_name");
            }
            if (!strstr((const char *)buffer.data, "\"frames_rendered\"") ||
                !strstr((const char *)buffer.data, "42")) {
                failures += fail("render_metrics_dataset_frames_value");
            }
            if (!strstr((const char *)buffer.data, "\"integrator_mode_3d\"") ||
                !strstr((const char *)buffer.data, "\"route_family\"") ||
                !strstr((const char *)buffer.data, "\"integrator_uses_3d_catalog\"")) {
                failures += fail("render_metrics_dataset_3d_truth_columns");
            }
            cJSON_Delete(root);
        }
        core_io_buffer_free(&buffer);
    }

    unlink(json_path);
    return failures;
}

#include "export/render_metrics_dataset.h"

#include "core_data.h"
#include "core_io.h"
#include "cJSON.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char *k_default_dataset_path = "data/runtime/render_metrics.dataset.json";
static const char *k_dataset_profile = "ray_tracing_render_metrics_v2";
static const char *k_metrics_table_name = "render_metrics_table_v2";

static bool append_dataset_items_json(cJSON *items, const CoreDataset *dataset) {
    if (!items || !dataset) return false;

    for (size_t i = 0; i < dataset->item_count; ++i) {
        const CoreDataItem *item = &dataset->items[i];

        if (item->kind == CORE_DATA_SCALAR_F64) {
            cJSON *entry = cJSON_CreateObject();
            if (!entry) return false;
            cJSON_AddStringToObject(entry, "name", item->name ? item->name : "unnamed");
            cJSON_AddStringToObject(entry, "kind", "scalar_f64");
            cJSON_AddNumberToObject(entry, "value", item->as.scalar_f64);
            cJSON_AddItemToArray(items, entry);
            continue;
        }

        if (item->kind != CORE_DATA_TABLE_TYPED || item->as.table_typed.row_count == 0) {
            continue;
        }

        cJSON *entry = cJSON_CreateObject();
        cJSON *row = cJSON_CreateObject();
        if (!entry || !row) {
            cJSON_Delete(entry);
            cJSON_Delete(row);
            return false;
        }

        cJSON_AddStringToObject(entry, "name", item->name ? item->name : "table_typed");
        cJSON_AddStringToObject(entry, "kind", "table_typed");
        cJSON_AddNumberToObject(entry, "rows", item->as.table_typed.row_count);
        cJSON_AddNumberToObject(entry, "columns", item->as.table_typed.column_count);

        for (uint32_t c = 0; c < item->as.table_typed.column_count; ++c) {
            const CoreTableColumnTyped *col = &item->as.table_typed.columns[c];
            const char *name = col->name ? col->name : "col";
            switch (col->type) {
                case CORE_TABLE_COL_F32:
                    cJSON_AddNumberToObject(row, name, (double)col->as.f32_values[0]);
                    break;
                case CORE_TABLE_COL_F64:
                    cJSON_AddNumberToObject(row, name, col->as.f64_values[0]);
                    break;
                case CORE_TABLE_COL_I64:
                    cJSON_AddNumberToObject(row, name, (double)col->as.i64_values[0]);
                    break;
                case CORE_TABLE_COL_U32:
                    cJSON_AddNumberToObject(row, name, (double)col->as.u32_values[0]);
                    break;
                case CORE_TABLE_COL_BOOL:
                    cJSON_AddBoolToObject(row, name, col->as.bool_values[0] ? 1 : 0);
                    break;
                default:
                    break;
            }
        }

        cJSON_AddItemToObject(entry, "row0", row);
        cJSON_AddItemToArray(items, entry);
    }

    return true;
}

bool ray_tracing_render_metrics_dataset_export_json(const RayTracingRenderMetricsSnapshot *snapshot,
                                                    const char *dataset_json_path) {
    if (!snapshot) return false;

    const char *json_path = (dataset_json_path && dataset_json_path[0]) ? dataset_json_path : k_default_dataset_path;
    CoreDataset dataset;
    core_dataset_init(&dataset);

    CoreResult r = core_dataset_add_metadata_string(&dataset, "profile", k_dataset_profile);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "schema_family", "ray_tracing_render_metrics");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "schema_variant", "runtime");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "dataset_schema", "ray_tracing.render_metrics");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_i64(&dataset, "dataset_contract_version", 2);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_i64(&dataset, "schema_version", 2);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "metrics_table", k_metrics_table_name);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset,
                                         "integrator_status_label",
                                         snapshot->integrator_status_label[0]
                                             ? snapshot->integrator_status_label
                                             : "integrator: Forward Light");
    if (r.code != CORE_OK) goto fail;

    const int64_t rays_per_frame = (snapshot->scene_rays > 0) ? (int64_t)snapshot->scene_rays : 0;
    const int64_t samples_per_ray = (snapshot->path_samples_per_pixel > 0) ? (int64_t)snapshot->path_samples_per_pixel : 1;
    const int64_t sample_estimate = (int64_t)snapshot->frames_rendered * rays_per_frame * samples_per_ray;

    r = core_dataset_add_scalar_f64(&dataset, "runtime_seconds", snapshot->runtime_seconds);
    if (r.code != CORE_OK) goto fail;

    {
        const char *cols[] = {
            "frames_rendered", "loops_completed", "runtime_seconds", "target_fps", "frame_duration_seconds",
            "scene_object_count", "scene_rays", "integrator_mode", "integrator_mode_3d",
            "route_family", "integrator_uses_3d_catalog", "sample_estimate", "bounce_limit",
            "path_samples_per_pixel", "path_max_depth", "use_tiled_renderer", "tile_size",
            "light_intensity", "cache_variance_cutoff", "cache_halo_radius", "environment_brightness",
            "interactive_mode", "deep_render_mode", "bounce_mode"
        };
        CoreTableColumnType types[] = {
            CORE_TABLE_COL_I64, CORE_TABLE_COL_I64, CORE_TABLE_COL_F64, CORE_TABLE_COL_I64, CORE_TABLE_COL_F64,
            CORE_TABLE_COL_I64, CORE_TABLE_COL_I64, CORE_TABLE_COL_I64, CORE_TABLE_COL_I64,
            CORE_TABLE_COL_I64, CORE_TABLE_COL_BOOL, CORE_TABLE_COL_I64, CORE_TABLE_COL_I64,
            CORE_TABLE_COL_I64, CORE_TABLE_COL_I64, CORE_TABLE_COL_BOOL, CORE_TABLE_COL_I64,
            CORE_TABLE_COL_F64, CORE_TABLE_COL_F64, CORE_TABLE_COL_F64, CORE_TABLE_COL_F64,
            CORE_TABLE_COL_BOOL, CORE_TABLE_COL_BOOL, CORE_TABLE_COL_BOOL
        };
        int64_t frames_rendered_col[] = {(int64_t)snapshot->frames_rendered};
        int64_t loops_completed_col[] = {(int64_t)snapshot->loops_completed};
        double runtime_seconds_col[] = {snapshot->runtime_seconds};
        int64_t target_fps_col[] = {(int64_t)snapshot->target_fps};
        double frame_duration_seconds_col[] = {snapshot->frame_duration_seconds};
        int64_t scene_object_count_col[] = {(int64_t)snapshot->scene_object_count};
        int64_t scene_rays_col[] = {(int64_t)snapshot->scene_rays};
        int64_t integrator_mode_col[] = {(int64_t)snapshot->integrator_mode};
        int64_t integrator_mode_3d_col[] = {(int64_t)snapshot->integrator_mode_3d};
        int64_t route_family_col[] = {(int64_t)snapshot->route_family};
        bool integrator_uses_3d_catalog_col[] = {snapshot->integrator_uses_3d_catalog};
        int64_t sample_estimate_col[] = {sample_estimate};
        int64_t bounce_limit_col[] = {(int64_t)snapshot->bounce_limit};
        int64_t path_samples_per_pixel_col[] = {(int64_t)snapshot->path_samples_per_pixel};
        int64_t path_max_depth_col[] = {(int64_t)snapshot->path_max_depth};
        bool use_tiled_renderer_col[] = {snapshot->use_tiled_renderer};
        int64_t tile_size_col[] = {(int64_t)snapshot->tile_size};
        double light_intensity_col[] = {snapshot->light_intensity};
        double cache_variance_cutoff_col[] = {snapshot->cache_variance_cutoff};
        double cache_halo_radius_col[] = {snapshot->cache_halo_radius};
        double environment_brightness_col[] = {snapshot->environment_brightness};
        bool interactive_mode_col[] = {snapshot->interactive_mode};
        bool deep_render_mode_col[] = {snapshot->deep_render_mode};
        bool bounce_mode_col[] = {snapshot->bounce_mode};
        const void *column_data[] = {
            frames_rendered_col, loops_completed_col, runtime_seconds_col, target_fps_col, frame_duration_seconds_col,
            scene_object_count_col, scene_rays_col, integrator_mode_col, integrator_mode_3d_col,
            route_family_col, integrator_uses_3d_catalog_col, sample_estimate_col, bounce_limit_col,
            path_samples_per_pixel_col, path_max_depth_col, use_tiled_renderer_col, tile_size_col,
            light_intensity_col, cache_variance_cutoff_col, cache_halo_radius_col, environment_brightness_col,
            interactive_mode_col, deep_render_mode_col, bounce_mode_col
        };

        r = core_dataset_add_table_typed(&dataset,
                                         k_metrics_table_name,
                                         cols,
                                         types,
                                         (uint32_t)(sizeof(cols) / sizeof(cols[0])),
                                         1u,
                                         column_data);
        if (r.code != CORE_OK) goto fail;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *metadata = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    if (!root || !metadata || !items) {
        cJSON_Delete(root);
        cJSON_Delete(metadata);
        cJSON_Delete(items);
        goto fail;
    }

    cJSON_AddStringToObject(root, "profile", k_dataset_profile);
    cJSON_AddStringToObject(root, "schema_family", "ray_tracing_render_metrics");
    cJSON_AddStringToObject(root, "schema_variant", "runtime");
    cJSON_AddStringToObject(root, "dataset_schema", "ray_tracing.render_metrics");
    cJSON_AddNumberToObject(root, "schema_version", 2);

    for (size_t i = 0; i < dataset.metadata_count; ++i) {
        const CoreMetadataItem *m = &dataset.metadata[i];
        if (!m->key) continue;
        switch (m->type) {
            case CORE_META_STRING:
                cJSON_AddStringToObject(metadata, m->key, m->as.string_value ? m->as.string_value : "");
                break;
            case CORE_META_F64:
                cJSON_AddNumberToObject(metadata, m->key, m->as.f64_value);
                break;
            case CORE_META_I64:
                cJSON_AddNumberToObject(metadata, m->key, (double)m->as.i64_value);
                break;
            case CORE_META_BOOL:
                cJSON_AddBoolToObject(metadata, m->key, m->as.bool_value ? 1 : 0);
                break;
            default:
                break;
        }
    }
    cJSON_AddItemToObject(root, "metadata", metadata);

    if (!append_dataset_items_json(items, &dataset)) {
        cJSON_Delete(root);
        goto fail;
    }
    cJSON_AddItemToObject(root, "items", items);

    char *json_text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!json_text) goto fail;

    r = core_io_write_all(json_path, json_text, strlen(json_text));
    free(json_text);
    core_dataset_free(&dataset);
    return r.code == CORE_OK;

fail:
    core_dataset_free(&dataset);
    return false;
}

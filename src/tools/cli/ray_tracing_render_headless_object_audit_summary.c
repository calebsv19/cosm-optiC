#include "tools/ray_tracing_render_headless_internal.h"

#include "app/ray_tracing_request_utils.h"

#include <stdio.h>

void ray_tracing_headless_write_object_audit(
    FILE *file,
    const RayTracingHeadlessPreflight *preflight) {
    fprintf(file, "  \"object_audit\": [\n");
    {
        int emitted = 0;
        for (int i = 0; i < RAY_TRACING_HEADLESS_OBJECT_AUDIT_MAX; ++i) {
            const RayTracingHeadlessObjectAuditEntry *entry = &preflight->object_audit[i];
            if (!entry->used) continue;
            if (emitted > 0) {
                fprintf(file, ",\n");
            }
            fprintf(file, "    {\n");
            fprintf(file, "      \"scene_object_index\": %d,\n", entry->scene_object_index);
            fprintf(file, "      \"object_id\": ");
            RayTracingJsonWriteString(file, entry->object_id);
            fprintf(file, ",\n");
            fprintf(file, "      \"object_type\": ");
            RayTracingJsonWriteString(file, entry->object_type);
            fprintf(file, ",\n");
            fprintf(file, "      \"material_id\": %d,\n", entry->material_id);
            fprintf(file, "      \"alpha\": %.6f,\n", entry->alpha);
            fprintf(file, "      \"reflectivity\": %.6f,\n", entry->reflectivity);
            fprintf(file, "      \"roughness\": %.6f,\n", entry->roughness);
            fprintf(file, "      \"emissive_strength\": %.6f,\n", entry->emissive_strength);
            fprintf(file, "      \"texture_id\": %d,\n", entry->texture_id);
            fprintf(file, "      \"texture_strength\": %.6f,\n", entry->texture_strength);
            fprintf(file, "      \"texture_scale\": %.6f,\n", entry->texture_scale);
            fprintf(file, "      \"texture_offset_u\": %.6f,\n", entry->texture_offset_u);
            fprintf(file, "      \"texture_offset_v\": %.6f,\n", entry->texture_offset_v);
            fprintf(file, "      \"texture_seed\": %d,\n", entry->texture_seed);
            fprintf(file, "      \"texture_pattern_mode\": %d,\n", entry->texture_pattern_mode);
            fprintf(file, "      \"texture_coverage\": %.6f,\n", entry->texture_coverage);
            fprintf(file, "      \"texture_grain\": %.6f,\n", entry->texture_grain);
            fprintf(file, "      \"texture_edge_softness\": %.6f,\n", entry->texture_edge_softness);
            fprintf(file, "      \"texture_contrast\": %.6f,\n", entry->texture_contrast);
            fprintf(file, "      \"texture_flow\": %.6f,\n", entry->texture_flow);
            fprintf(file, "      \"texture_color_depth\": %.6f,\n", entry->texture_color_depth);
            fprintf(file, "      \"texture_surface_damage\": %.6f,\n", entry->texture_surface_damage);
            fprintf(file, "      \"packed_color\": %d,\n", entry->packed_color);
            fprintf(file, "      \"primitive_count\": %d,\n", entry->primitive_count);
            fprintf(file, "      \"triangle_count\": %d,\n", entry->triangle_count);
            fprintf(file, "      \"primary_hit_pixels\": %d,\n", entry->primary_hit_pixels);
            fprintf(file, "      \"center\": { \"x\": %.6f, \"y\": %.6f, \"z\": %.6f },\n",
                    entry->center_x,
                    entry->center_y,
                    entry->center_z);
            fprintf(file, "      \"center_projectable\": %s,\n",
                    entry->center_projectable ? "true" : "false");
            fprintf(file, "      \"center_inside_viewport\": %s,\n",
                    entry->center_inside_viewport ? "true" : "false");
            fprintf(file,
                    "      \"center_screen\": { \"x\": %.6f, \"y\": %.6f, \"camera_depth\": %.6f }\n",
                    entry->center_screen_x,
                    entry->center_screen_y,
                    entry->center_camera_depth);
            fprintf(file, "    }");
            emitted += 1;
        }
        if (emitted > 0) {
            fprintf(file, "\n");
        }
    }
    fprintf(file, "  ],\n");
}

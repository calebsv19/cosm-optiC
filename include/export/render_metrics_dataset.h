#ifndef RENDER_METRICS_DATASET_H
#define RENDER_METRICS_DATASET_H

#include <stdbool.h>

typedef struct RayTracingRenderMetricsSnapshot {
    int frames_rendered;
    int loops_completed;
    double runtime_seconds;
    int scene_object_count;
    int scene_rays;
    int target_fps;
    double frame_duration_seconds;
    int integrator_mode;
    int bounce_limit;
    int path_samples_per_pixel;
    int path_max_depth;
    bool use_tiled_renderer;
    int tile_size;
    double light_intensity;
    double cache_variance_cutoff;
    double cache_halo_radius;
    double environment_brightness;
    bool interactive_mode;
    bool deep_render_mode;
    bool bounce_mode;
} RayTracingRenderMetricsSnapshot;

bool ray_tracing_render_metrics_dataset_export_json(const RayTracingRenderMetricsSnapshot *snapshot,
                                                    const char *dataset_json_path);

#endif

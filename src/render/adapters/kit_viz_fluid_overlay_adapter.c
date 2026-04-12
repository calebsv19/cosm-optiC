#include "render/kit_viz_fluid_overlay_adapter.h"

#include "kit_viz.h"

bool kit_viz_fluid_overlay_build_density_rgba(const FluidFrame *frame,
                                              const KitVizFluidOverlayConfig *config,
                                              uint8_t *out_rgba,
                                              size_t out_rgba_size) {
    if (!frame || !frame->density || frame->w <= 0 || frame->h <= 0 || !out_rgba) return false;
    if (!config) return false;

    uint32_t width = (uint32_t)frame->w;
    uint32_t height = (uint32_t)frame->h;
    float min_v = config->min_density;
    float max_v = config->max_density;

    if (config->auto_range) {
        KitVizFieldStats stats;
        CoreResult stats_r = kit_viz_compute_field_stats(frame->density, width, height, &stats);
        if (stats_r.code != CORE_OK) return false;
        min_v = stats.min_value;
        max_v = stats.max_value;
    }

    CoreResult rgba_r = kit_viz_build_heatmap_rgba(frame->density,
                                                   width,
                                                   height,
                                                   min_v,
                                                   max_v,
                                                   (KitVizColormap)config->colormap,
                                                   out_rgba,
                                                   out_rgba_size);
    if (rgba_r.code != CORE_OK) return false;

    size_t pixel_count = (size_t)width * (size_t)height;
    for (size_t i = 0; i < pixel_count; ++i) {
        out_rgba[i * 4u + 3u] = config->alpha;
    }
    return true;
}

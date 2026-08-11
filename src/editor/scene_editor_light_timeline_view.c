#include "scene_editor_light_timeline_view.h"

#include <math.h>
#include <string.h>

static double clamp_double(double value, double minimum, double maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

void scene_editor_light_timeline_panel_geometry(
    const SDL_Rect* panel,
    SceneEditorLightTimelinePanelGeometry* out_geometry) {
    bool compact;
    int graph_top;
    int speed_height;
    int point_height;
    int footer_height;
    int graph_bottom;
    int mode_width;
    int mode_gap;
    if (!out_geometry) return;
    memset(out_geometry, 0, sizeof(*out_geometry));
    if (!panel || panel->w <= 0 || panel->h <= 0) return;

    compact = panel->h < 260;
    graph_top = panel->y + (compact ? 72 : 78);
    speed_height = compact ? 48 : 64;
    point_height = compact ? 16 : 18;
    footer_height = compact ? 12 : 14;
    graph_bottom = panel->y + panel->h - footer_height - 4 -
                   speed_height - 10 - point_height - 12;
    out_geometry->metrics_line = (SDL_Rect){
        panel->x + 12,
        panel->y + 27,
        panel->w > 24 ? panel->w - 24 : 0,
        13
    };
    out_geometry->timing_graph = (SDL_Rect){
        panel->x + 54,
        graph_top,
        panel->w > 70 ? panel->w - 70 : 0,
        graph_bottom - graph_top
    };
    out_geometry->path_point_strip = (SDL_Rect){
        out_geometry->timing_graph.x,
        out_geometry->timing_graph.y + out_geometry->timing_graph.h + 12,
        out_geometry->timing_graph.w,
        point_height
    };
    out_geometry->speed_strip = (SDL_Rect){
        out_geometry->timing_graph.x,
        out_geometry->path_point_strip.y +
            out_geometry->path_point_strip.h + 10,
        out_geometry->timing_graph.w,
        speed_height
    };
    out_geometry->footer_hint = (SDL_Rect){
        out_geometry->timing_graph.x,
        panel->y + panel->h - footer_height,
        out_geometry->timing_graph.w,
        footer_height
    };
    out_geometry->add_key_button = (SDL_Rect){
        panel->x + panel->w - 64,
        panel->y + 7,
        52,
        20
    };
    out_geometry->play_button = (SDL_Rect){
        panel->x + panel->w - 122,
        panel->y + 7,
        52,
        20
    };
    out_geometry->motion_lane_button = (SDL_Rect){
        panel->x + 116, panel->y + 7, 66, 20
    };
    out_geometry->intensity_lane_button = (SDL_Rect){
        panel->x + 186, panel->y + 7, 72, 20
    };
    mode_gap = 4;
    mode_width = panel->w >= 460 ? 94 : 66;
    out_geometry->constant_speed_button = (SDL_Rect){
        panel->x + 12,
        panel->y + 45,
        mode_width,
        20
    };
    out_geometry->equal_segments_button = (SDL_Rect){
        out_geometry->constant_speed_button.x + mode_width + mode_gap,
        panel->y + 45,
        mode_width,
        20
    };
    out_geometry->custom_mode_indicator = (SDL_Rect){
        out_geometry->equal_segments_button.x + mode_width + mode_gap,
        panel->y + 45,
        panel->w >= 460 ? 70 : 50,
        20
    };
    out_geometry->step_button = (SDL_Rect){
        out_geometry->custom_mode_indicator.x +
            out_geometry->custom_mode_indicator.w + mode_gap,
        panel->y + 45,
        panel->w >= 460 ? 44 : 36,
        20
    };
    out_geometry->linear_button = (SDL_Rect){
        out_geometry->step_button.x + out_geometry->step_button.w + mode_gap,
        panel->y + 45,
        panel->w >= 460 ? 50 : 40,
        20
    };
    out_geometry->bezier_button = (SDL_Rect){
        out_geometry->linear_button.x +
            out_geometry->linear_button.w + mode_gap,
        panel->y + 45,
        panel->w >= 460 ? 54 : 44,
        20
    };
}

void scene_editor_light_timeline_view_reset(
    SceneEditorLightTimelineView* view) {
    if (!view) return;
    view->start_normalized = 0.0;
    view->span_normalized = 1.0;
}

void scene_editor_light_timeline_view_zoom(
    SceneEditorLightTimelineView* view,
    double anchor_normalized,
    double factor,
    double minimum_span) {
    double anchor_local;
    double new_span;
    if (!view || !isfinite(anchor_normalized) || !isfinite(factor) ||
        factor <= 0.0) return;
    minimum_span = clamp_double(minimum_span, 0.001, 1.0);
    anchor_normalized = clamp_double(anchor_normalized, 0.0, 1.0);
    if (view->span_normalized <= 0.0 || view->span_normalized > 1.0) {
        scene_editor_light_timeline_view_reset(view);
    }
    anchor_local =
        (anchor_normalized - view->start_normalized) / view->span_normalized;
    anchor_local = clamp_double(anchor_local, 0.0, 1.0);
    new_span = clamp_double(view->span_normalized * factor,
                            minimum_span, 1.0);
    view->start_normalized =
        anchor_normalized - anchor_local * new_span;
    view->span_normalized = new_span;
    view->start_normalized = clamp_double(
        view->start_normalized, 0.0, 1.0 - view->span_normalized);
}

void scene_editor_light_timeline_view_pan(
    SceneEditorLightTimelineView* view,
    double delta_normalized) {
    if (!view || !isfinite(delta_normalized)) return;
    if (view->span_normalized <= 0.0 || view->span_normalized > 1.0) {
        scene_editor_light_timeline_view_reset(view);
    }
    view->start_normalized = clamp_double(
        view->start_normalized + delta_normalized,
        0.0, 1.0 - view->span_normalized);
}

double scene_editor_light_timeline_view_normalized_at_x(
    const SceneEditorLightTimelineView* view,
    const SDL_Rect* graph,
    int x) {
    double local;
    if (!view || !graph || graph->w <= 0) return 0.0;
    local = clamp_double((double)(x - graph->x) / (double)graph->w,
                         0.0, 1.0);
    return clamp_double(view->start_normalized +
                            local * view->span_normalized,
                        0.0, 1.0);
}

int scene_editor_light_timeline_view_x_at_normalized(
    const SceneEditorLightTimelineView* view,
    const SDL_Rect* graph,
    double normalized) {
    double local;
    if (!view || !graph || graph->w <= 0 ||
        view->span_normalized <= 0.0) return graph ? graph->x : 0;
    local = (normalized - view->start_normalized) /
            view->span_normalized;
    return graph->x + (int)llround(local * graph->w);
}

double scene_editor_light_timeline_progress_at_y(
    const SDL_Rect* graph,
    int y) {
    if (!graph || graph->h <= 0) return 0.0;
    return clamp_double(
        1.0 - (double)(y - graph->y) / (double)graph->h,
        0.0, 1.0);
}

double scene_editor_light_timeline_value_at_y(
    const SDL_Rect* graph,
    int y,
    double minimum,
    double maximum) {
    double normalized;
    if (!graph || graph->h <= 0 || !isfinite(minimum) ||
        !isfinite(maximum) || maximum <= minimum) {
        return minimum;
    }
    normalized = clamp_double(
        1.0 - (double)(y - graph->y) / (double)graph->h,
        0.0, 1.0);
    return minimum + normalized * (maximum - minimum);
}

int scene_editor_light_timeline_y_at_value(
    const SDL_Rect* graph,
    double value,
    double minimum,
    double maximum) {
    double normalized;
    if (!graph || graph->h <= 0 || !isfinite(value) ||
        !isfinite(minimum) || !isfinite(maximum) || maximum <= minimum) {
        return graph ? graph->y + graph->h : 0;
    }
    normalized = clamp_double(
        (value - minimum) / (maximum - minimum), 0.0, 1.0);
    return graph->y + graph->h -
        (int)llround(normalized * (double)graph->h);
}

void scene_editor_light_timeline_key_point(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const SDL_Rect* graph,
    const TimelineKeyframe* key,
    int* out_x,
    int* out_y) {
    scene_editor_light_timeline_scalar_key_point(
        view, document, graph, key, 0.0, 1.0, out_x, out_y);
}

void scene_editor_light_timeline_scalar_key_point(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const SDL_Rect* graph,
    const TimelineKeyframe* key,
    double minimum,
    double maximum,
    int* out_x,
    int* out_y) {
    double normalized;
    if (!view || !document || !graph || !key ||
        document->timeline.range.frame_count < 2u) return;
    normalized =
        (double)(key->frame - document->timeline.range.start_frame) /
        (double)(document->timeline.range.frame_count - 1u);
    if (out_x) {
        *out_x = scene_editor_light_timeline_view_x_at_normalized(
            view, graph, normalized);
    }
    if (out_y) {
        *out_y = scene_editor_light_timeline_y_at_value(
            graph, key->value.as.scalar, minimum, maximum);
    }
}

int scene_editor_light_timeline_pick_key(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    const SDL_Rect* graph,
    int x,
    int y) {
    return scene_editor_light_timeline_pick_scalar_key(
        view, document, track, graph, 0.0, 1.0, x, y);
}

int scene_editor_light_timeline_pick_scalar_key(
    const SceneEditorLightTimelineView* view,
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    const SDL_Rect* graph,
    double minimum,
    double maximum,
    int x,
    int y) {
    int best = -1;
    double best_distance = 10.0 * 10.0;
    if (!view || !document || !track || !graph) return -1;
    for (size_t i = 0u; i < track->key_count; ++i) {
        int key_x = 0;
        int key_y = 0;
        double dx;
        double dy;
        double distance;
        scene_editor_light_timeline_scalar_key_point(
            view, document, graph, &track->keys[i],
            minimum, maximum, &key_x, &key_y);
        dx = (double)key_x - x;
        dy = (double)key_y - y;
        distance = dx * dx + dy * dy;
        if (distance <= best_distance) {
            best_distance = distance;
            best = (int)i;
        }
    }
    return best;
}

double scene_editor_light_timeline_nice_ceiling(double value) {
    double magnitude;
    double normalized;
    double step;
    if (!isfinite(value) || value <= 0.0) return 1.0;
    magnitude = pow(10.0, floor(log10(value)));
    normalized = value / magnitude;
    step = normalized <= 1.0 ? 1.0 :
           normalized <= 2.0 ? 2.0 :
           normalized <= 5.0 ? 5.0 : 10.0;
    return step * magnitude;
}

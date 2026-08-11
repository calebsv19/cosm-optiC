#include "scene_editor_light_timeline_intensity_panel.h"

#include "render/font_runtime.h"
#include "render/text_draw.h"
#include "scene_editor_light_timeline_curve_edit.h"
#include "ui/shared_theme_font_adapter.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static SDL_Color offset_color(SDL_Color color, int offset, Uint8 alpha) {
    int r = (int)color.r + offset;
    int g = (int)color.g + offset;
    int b = (int)color.b + offset;
    color.r = (Uint8)(r < 0 ? 0 : r > 255 ? 255 : r);
    color.g = (Uint8)(g < 0 ? 0 : g > 255 ? 255 : g);
    color.b = (Uint8)(b < 0 ? 0 : b > 255 ? 255 : b);
    color.a = alpha;
    return color;
}

static RayTracingThemePalette palette(void) {
    RayTracingThemePalette result = {0};
    if (!ray_tracing_shared_theme_resolve_palette(&result)) {
        result.panel_fill = (SDL_Color){36, 39, 49, 255};
        result.panel_border = (SDL_Color){82, 89, 106, 255};
        result.button_fill = (SDL_Color){45, 49, 60, 255};
        result.button_active_fill = (SDL_Color){70, 112, 154, 255};
        result.text_primary = (SDL_Color){226, 232, 242, 255};
        result.text_muted = (SDL_Color){151, 162, 181, 255};
        result.accent_primary = (SDL_Color){105, 196, 239, 255};
    }
    return result;
}

static void text(SDL_Renderer* renderer, TTF_Font* font,
                 const char* value, int x, int y, SDL_Color color) {
    if (renderer && font && value && value[0]) {
        ray_tracing_text_draw_utf8_at(
            renderer, font, value, x, y, color);
    }
}

static void control(SDL_Renderer* renderer, TTF_Font* font,
                    const SDL_Rect* rect, const char* label,
                    bool active, const RayTracingThemePalette* colors) {
    SDL_Color fill = active
        ? ray_tracing_theme_resolve_button_active_fill(*colors)
        : offset_color(colors->button_fill, -8, 255);
    SDL_Color border = active
        ? colors->accent_primary : colors->panel_border;
    SDL_Color foreground =
        ray_tracing_theme_choose_button_text(fill, *colors);
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(
        renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, rect);
    text(renderer, font, label, rect->x + 7, rect->y + 4, foreground);
}

static bool evaluate_scalar(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    double frame_position,
    double* out_value) {
    int64_t frame = (int64_t)floor(frame_position);
    uint32_t subframe = (uint32_t)llround(
        (frame_position - (double)frame) * 1000000.0);
    TimelineEvaluationContext context;
    TimelineEvaluationResult result;
    if (!document || !track || !out_value) return false;
    if (subframe >= 1000000u) {
        frame += 1;
        subframe = 0u;
    }
    if (TimelineEvaluationContextBuild(
            document->timeline.rate, document->timeline.range,
            (TimelineSample){frame, subframe, 1000000u}, &context) !=
            TIMELINE_STATUS_OK ||
        TimelineTrackEvaluate(track, &context, &result) !=
            TIMELINE_STATUS_OK) {
        return false;
    }
    *out_value = result.value.as.scalar;
    return isfinite(*out_value);
}

void scene_editor_light_timeline_intensity_panel_render(
    SDL_Renderer* renderer,
    const SDL_Rect* panel,
    const RuntimeSceneLightTimelineDocument* document,
    const SceneEditorLightTimelinePanelState* state) {
    SceneEditorLightTimelinePanelGeometry geometry;
    RayTracingThemePalette colors;
    TimelineTrack fallback_track;
    const TimelineTrack* track = NULL;
    bool authored = false;
    double minimum = 0.0;
    double maximum = 1.0;
    TTF_Font* title_font;
    TTF_Font* font;
    TTF_Font* small_font;
    char label[160];
    size_t index = SIZE_MAX;
    if (!renderer || !panel || !document || !state || !state->view ||
        document->timeline.range.frame_count < 2u) {
        return;
    }
    if (RuntimeSceneLightTimelineFindTrack(
            document, "light/intensity", &index) == TIMELINE_STATUS_OK) {
        track = &document->timeline.tracks[index];
        authored = true;
    } else {
        const TimelineTrack* progress =
            &document->timeline.tracks[document->progress_track_index];
        memset(&fallback_track, 0, sizeof(fallback_track));
        if (TimelineTrackInit(
                &fallback_track, "base_light_intensity", progress->target_id,
                "light/intensity", TIMELINE_VALUE_SCALAR) !=
                TIMELINE_STATUS_OK ||
            TimelineTrackSetUnit(
                &fallback_track, TIMELINE_UNIT_RELATIVE_INTENSITY) !=
                TIMELINE_STATUS_OK ||
            TimelineTrackAddKey(
                &fallback_track, document->timeline.range.start_frame,
                TimelineValueScalar(state->base_intensity),
                TIMELINE_INTERPOLATION_LINEAR) != TIMELINE_STATUS_OK ||
            TimelineTrackAddKey(
                &fallback_track,
                document->timeline.range.start_frame +
                    (int64_t)document->timeline.range.frame_count - 1,
                TimelineValueScalar(state->base_intensity),
                TIMELINE_INTERPOLATION_STEP) != TIMELINE_STATUS_OK) {
            return;
        }
        track = &fallback_track;
    }
    scene_editor_light_timeline_lane_value_range(
        SCENE_EDITOR_LIGHT_TIMELINE_LANE_INTENSITY, track,
        state->base_intensity, &minimum, &maximum);
    scene_editor_light_timeline_panel_geometry(panel, &geometry);
    colors = palette();
    title_font = ray_tracing_font_runtime_get_ui_regular(
        renderer, panel->h >= 260 ? 13 : 12, 10);
    font = ray_tracing_font_runtime_get_ui_regular(renderer, 10, 8);
    small_font = ray_tracing_font_runtime_get_ui_regular(renderer, 9, 7);

    SDL_SetRenderDrawColor(
        renderer, colors.panel_fill.r, colors.panel_fill.g,
        colors.panel_fill.b, 255);
    SDL_RenderFillRect(renderer, panel);
    SDL_SetRenderDrawColor(
        renderer, colors.panel_border.r, colors.panel_border.g,
        colors.panel_border.b, colors.panel_border.a);
    SDL_RenderDrawRect(renderer, panel);
    text(renderer, title_font, "LIGHT TIMELINE",
         panel->x + 12, panel->y + 5, colors.text_primary);
    control(renderer, small_font, &geometry.motion_lane_button,
            "MOTION", false, &colors);
    control(renderer, small_font, &geometry.intensity_lane_button,
            "INTENSITY", true, &colors);
    snprintf(label, sizeof(label),
             "F %lld | Intensity %.3f | %s",
             (long long)state->current_frame,
             state->evaluated_scene && state->evaluated_scene->valid
                 ? state->evaluated_scene->light.intensity
                 : state->base_intensity,
             authored ? "AUTHORED" : "BASE LIGHT (not authored)");
    text(renderer, font, label, geometry.metrics_line.x,
         geometry.metrics_line.y, colors.text_muted);

    control(renderer, small_font, &geometry.constant_speed_button,
            "MOTION ONLY", false, &colors);
    control(renderer, small_font, &geometry.equal_segments_button,
            "MOTION ONLY", false, &colors);
    {
        TimelineInterpolation interpolation = TIMELINE_INTERPOLATION_STEP;
        bool selectable = authored && state->selected_key_index >= 0 &&
            state->selected_key_index + 1 < (int)track->key_count;
        if (selectable) {
            interpolation = track->keys[state->selected_key_index].
                interpolation_to_next;
        }
        control(renderer, small_font, &geometry.step_button, "STEP",
                selectable &&
                    interpolation == TIMELINE_INTERPOLATION_STEP, &colors);
        control(renderer, small_font, &geometry.linear_button, "LINEAR",
                selectable &&
                    interpolation == TIMELINE_INTERPOLATION_LINEAR, &colors);
        control(renderer, small_font, &geometry.bezier_button, "BEZIER",
                selectable &&
                    interpolation == TIMELINE_INTERPOLATION_CUBIC_BEZIER,
                &colors);
    }
    control(renderer, small_font, &geometry.add_key_button,
            "+ KEY", false, &colors);
    control(renderer, small_font, &geometry.play_button,
            state->playing ? "PAUSE" : "PLAY",
            state->playing, &colors);

    SDL_Color graph_fill = offset_color(colors.panel_fill, -18, 255);
    SDL_SetRenderDrawColor(
        renderer, graph_fill.r, graph_fill.g, graph_fill.b, graph_fill.a);
    SDL_RenderFillRect(renderer, &geometry.timing_graph);
    SDL_SetRenderDrawColor(
        renderer, colors.panel_border.r, colors.panel_border.g,
        colors.panel_border.b, colors.panel_border.a);
    SDL_RenderDrawRect(renderer, &geometry.timing_graph);
    text(renderer, small_font, "INTENSITY", geometry.timing_graph.x,
         geometry.timing_graph.y - 12, colors.accent_primary);
    for (int division = 0; division <= 4; ++division) {
        double value = minimum +
            (maximum - minimum) * (double)division / 4.0;
        int y = scene_editor_light_timeline_y_at_value(
            &geometry.timing_graph, value, minimum, maximum);
        SDL_SetRenderDrawColor(
            renderer, colors.panel_border.r, colors.panel_border.g,
            colors.panel_border.b, 110);
        SDL_RenderDrawLine(
            renderer, geometry.timing_graph.x, y,
            geometry.timing_graph.x + geometry.timing_graph.w, y);
        snprintf(label, sizeof(label), "%.2g", value);
        text(renderer, small_font, label,
             geometry.timing_graph.x - 36, y - 5, colors.text_muted);
    }
    {
        int prior_x = geometry.timing_graph.x;
        int prior_y = scene_editor_light_timeline_y_at_value(
            &geometry.timing_graph, track->keys[0].value.as.scalar,
            minimum, maximum);
        for (int x = geometry.timing_graph.x + 1;
             x <= geometry.timing_graph.x + geometry.timing_graph.w; ++x) {
            double normalized =
                state->view->start_normalized +
                (double)(x - geometry.timing_graph.x) /
                    (double)geometry.timing_graph.w *
                    state->view->span_normalized;
            double frame_position =
                (double)document->timeline.range.start_frame +
                normalized *
                    (double)(document->timeline.range.frame_count - 1u);
            double value = 0.0;
            if (evaluate_scalar(document, track, frame_position, &value)) {
                int y = scene_editor_light_timeline_y_at_value(
                    &geometry.timing_graph, value, minimum, maximum);
                SDL_SetRenderDrawColor(
                    renderer, colors.accent_primary.r,
                    colors.accent_primary.g,
                    colors.accent_primary.b, 255);
                SDL_RenderDrawLine(renderer, prior_x, prior_y, x, y);
                prior_x = x;
                prior_y = y;
            }
        }
    }
    if (authored && state->selected_key_index >= 0 &&
        (size_t)state->selected_key_index < track->key_count) {
        const size_t key_index = (size_t)state->selected_key_index;
        int key_x = 0;
        int key_y = 0;
        const SceneEditorLightTimelineHandle handles[] = {
            SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_INCOMING,
            SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_OUTGOING
        };
        scene_editor_light_timeline_scalar_key_point(
            state->view, document, &geometry.timing_graph,
            &track->keys[key_index], minimum, maximum, &key_x, &key_y);
        for (size_t i = 0u; i < 2u; ++i) {
            int hx = 0;
            int hy = 0;
            if (!scene_editor_light_timeline_scalar_handle_point(
                    state->view, document, track, key_index, handles[i],
                    &geometry.timing_graph, minimum, maximum, &hx, &hy)) {
                continue;
            }
            SDL_SetRenderDrawColor(renderer, 255, 218, 92, 200);
            SDL_RenderDrawLine(renderer, key_x, key_y, hx, hy);
            SDL_Rect marker = {hx - 4, hy - 4, 9, 9};
            SDL_RenderFillRect(renderer, &marker);
        }
    }
    if (authored) {
        for (size_t i = 0u; i < track->key_count; ++i) {
            int x = 0;
            int y = 0;
            bool selected = (int)i == state->selected_key_index;
            scene_editor_light_timeline_scalar_key_point(
                state->view, document, &geometry.timing_graph,
                &track->keys[i], minimum, maximum, &x, &y);
            SDL_Rect marker = {
                x - (selected ? 5 : 4), y - (selected ? 5 : 4),
                selected ? 11 : 9, selected ? 11 : 9
            };
            SDL_SetRenderDrawColor(
                renderer, selected ? 255 : 226,
                selected ? 218 : 181, selected ? 92 : 88, 255);
            SDL_RenderFillRect(renderer, &marker);
        }
    }
    {
        double normalized = (double)(state->current_frame -
            document->timeline.range.start_frame) /
            (double)(document->timeline.range.frame_count - 1u);
        int x = scene_editor_light_timeline_view_x_at_normalized(
            state->view, &geometry.timing_graph, normalized);
        SDL_SetRenderDrawColor(renderer, 255, 91, 91, 225);
        SDL_RenderDrawLine(
            renderer, x, geometry.timing_graph.y,
            x, geometry.timing_graph.y + geometry.timing_graph.h);
    }
    text(renderer, small_font,
         authored
             ? "Double-click to add | drag keys/Bezier handles | Cmd-Z undo"
             : "Base intensity is unchanged until + KEY or double-click creates this track",
         geometry.footer_hint.x, geometry.footer_hint.y,
         colors.text_muted);
}

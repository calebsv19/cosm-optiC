#include "scene_editor_light_timeline_panel.h"

#include "animation/timeline_document.h"
#include "render/font_runtime.h"
#include "render/text_draw.h"
#include "scene_editor_light_timeline_curve_edit.h"
#include "scene_editor_light_timeline_edit.h"
#include "ui/shared_theme_font_adapter.h"

#include <math.h>
#include <stdio.h>

static Uint8 panel_channel_offset(Uint8 value, int offset) {
    int result = (int)value + offset;
    if (result < 0) return 0;
    if (result > 255) return 255;
    return (Uint8)result;
}

static SDL_Color panel_color_offset(
    SDL_Color color,
    int offset,
    Uint8 alpha) {
    color.r = panel_channel_offset(color.r, offset);
    color.g = panel_channel_offset(color.g, offset);
    color.b = panel_channel_offset(color.b, offset);
    color.a = alpha;
    return color;
}

static RayTracingThemePalette panel_palette(void) {
    RayTracingThemePalette palette = {0};
    if (!ray_tracing_shared_theme_resolve_palette(&palette)) {
        palette.background_fill = (SDL_Color){24, 26, 33, 255};
        palette.panel_fill = (SDL_Color){36, 39, 49, 255};
        palette.panel_border = (SDL_Color){82, 89, 106, 255};
        palette.button_fill = (SDL_Color){45, 49, 60, 255};
        palette.button_active_fill = (SDL_Color){70, 112, 154, 255};
        palette.button_text = (SDL_Color){232, 237, 246, 255};
        palette.text_primary = (SDL_Color){226, 232, 242, 255};
        palette.text_muted = (SDL_Color){151, 162, 181, 255};
        palette.accent_primary = (SDL_Color){105, 196, 239, 255};
    }
    return palette;
}

static void panel_text(SDL_Renderer* renderer, TTF_Font* font,
                       const char* text, int x, int y, SDL_Color color) {
    if (renderer && font && text && text[0]) {
        ray_tracing_text_draw_utf8_at(renderer, font, text, x, y, color);
    }
}

static int panel_text_width(TTF_Font* font, const char* text) {
    int width = 0;
    int height = 0;
    if (!font || !text || !text[0] ||
        TTF_SizeUTF8(font, text, &width, &height) != 0) {
        return 0;
    }
    return width;
}

static bool panel_point_in_rect(int x, int y, const SDL_Rect* rect) {
    return rect && x >= rect->x && x < rect->x + rect->w &&
        y >= rect->y && y < rect->y + rect->h;
}

static void panel_mode_control(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const SDL_Rect* rect,
    const char* text,
    bool active,
    int text_offset_x,
    const RayTracingThemePalette* palette) {
    SDL_Color fill;
    SDL_Color border;
    SDL_Color text_color;
    if (!renderer || !rect) return;
    fill = active
        ? ray_tracing_theme_resolve_button_active_fill(*palette)
        : panel_color_offset(palette->panel_fill, -8, 245);
    border = active ? palette->accent_primary : palette->panel_border;
    text_color = active
        ? ray_tracing_theme_choose_button_text(fill, *palette)
        : palette->text_muted;
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(
        renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, rect);
    panel_text(renderer, font, text,
               rect->x + text_offset_x, rect->y + 4, text_color);
}

static void panel_mode_status(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const SDL_Rect* rect,
    bool active,
    const RayTracingThemePalette* palette) {
    SDL_Color dot_color;
    SDL_Rect dot;
    if (!renderer || !rect || !palette) return;
    dot_color = active ? palette->accent_primary : palette->text_muted;
    dot = (SDL_Rect){rect->x + 4, rect->y + 8, 5, 5};
    SDL_SetRenderDrawColor(
        renderer, dot_color.r, dot_color.g, dot_color.b,
        active ? 255 : 125);
    SDL_RenderFillRect(renderer, &dot);
    panel_text(renderer, font, "CUSTOM", rect->x + 14, rect->y + 4,
               active ? palette->text_primary : palette->text_muted);
}

static double panel_fps(const RuntimeSceneLightTimelineDocument* document) {
    if (!document ||
        document->timeline.rate.frames_per_second_denominator == 0u) {
        return 1.0;
    }
    return (double)document->timeline.rate.frames_per_second_numerator /
           (double)document->timeline.rate.frames_per_second_denominator;
}

static int panel_segment_for_frame(const TimelineTrack* track,
                                   int64_t frame) {
    if (!track || track->key_count < 2u) return -1;
    for (size_t i = 0u; i + 1u < track->key_count; ++i) {
        if (frame >= track->keys[i].frame &&
            frame <= track->keys[i + 1u].frame) {
            return (int)i;
        }
    }
    return frame < track->keys[0].frame ? 0 : (int)track->key_count - 2;
}

static double panel_segment_speed(const TimelineTrack* track,
                                  size_t segment,
                                  double path_length,
                                  double fps) {
    double progress_delta;
    double frame_delta;
    if (!track || segment + 1u >= track->key_count ||
        !isfinite(path_length) || path_length < 0.0 ||
        !isfinite(fps) || fps <= 0.0) {
        return 0.0;
    }
    progress_delta =
        track->keys[segment + 1u].value.as.scalar -
        track->keys[segment].value.as.scalar;
    frame_delta = (double)(track->keys[segment + 1u].frame -
                           track->keys[segment].frame);
    if (frame_delta <= 0.0) return 0.0;
    return fabs(progress_delta) * path_length * fps / frame_delta;
}

static bool panel_evaluate_progress(
    const RuntimeSceneLightTimelineDocument* document,
    const TimelineTrack* track,
    double frame_position,
    double* out_progress) {
    int64_t frame;
    uint32_t subframe;
    TimelineEvaluationContext context;
    TimelineEvaluationResult result;
    if (!document || !track || !out_progress || !isfinite(frame_position)) {
        return false;
    }
    frame = (int64_t)floor(frame_position);
    subframe =
        (uint32_t)llround((frame_position - (double)frame) * 1000000.0);
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
    *out_progress = result.value.as.scalar;
    return isfinite(*out_progress);
}

void scene_editor_light_timeline_panel_render(
    SDL_Renderer* renderer,
    const SDL_Rect* panel,
    const RuntimeSceneLightTimelineDocument* document,
    const SceneEditorLightTimelinePanelState* state) {
    const TimelineTrack* track;
    SceneEditorLightTimelinePanelGeometry geometry;
    SDL_Rect graph;
    SDL_Rect path_point_strip;
    SDL_Rect speed_strip;
    TTF_Font* title_font;
    TTF_Font* font;
    TTF_Font* small_font;
    RayTracingThemePalette palette;
    SDL_Color graph_fill;
    SDL_Color grid_color;
    SDL_Color timing_color;
    SDL_Color speed_color;
    double fps;
    double path_length;
    double visible_start;
    double visible_span;
    double max_segment_speed = 0.0;
    double speed_axis_max;
    int selected_segment;
    int grid_x;
    int speed_header_width;
    char label[192];

    if (!renderer || !panel || !document || !document->valid || !state ||
        !state->view || !state->evaluated_scene ||
        !state->evaluated_scene->valid ||
        document->progress_track_index >= document->timeline.track_count ||
        document->timeline.range.frame_count < 2u) {
        return;
    }
    track = &document->timeline.tracks[document->progress_track_index];
    palette = panel_palette();
    graph_fill = panel_color_offset(palette.panel_fill, -18, 255);
    grid_color = panel_color_offset(palette.panel_border, -25, 255);
    timing_color = palette.accent_primary;
    speed_color = panel_color_offset(palette.accent_primary, 18, 255);
    scene_editor_light_timeline_panel_geometry(panel, &geometry);
    graph = geometry.timing_graph;
    path_point_strip = geometry.path_point_strip;
    speed_strip = geometry.speed_strip;
    fps = panel_fps(document);
    path_length = state->evaluated_scene->light.path_length_world;
    visible_start = state->view->start_normalized;
    visible_span = state->view->span_normalized;
    if (!isfinite(visible_start) || !isfinite(visible_span) ||
        visible_span <= 0.0 || visible_span > 1.0) {
        visible_start = 0.0;
        visible_span = 1.0;
    }
    selected_segment =
        state->selected_key_index >= 0
            ? (state->selected_key_index + 1 < (int)track->key_count
                   ? state->selected_key_index
                   : state->selected_key_index - 1)
            : panel_segment_for_frame(track, state->current_frame);
    grid_x = graph.w >= 640 ? 4 : 2;

    for (size_t i = 0u; i + 1u < track->key_count; ++i) {
        double speed = panel_segment_speed(track, i, path_length, fps);
        if (speed > max_segment_speed) max_segment_speed = speed;
    }
    if (max_segment_speed <= 0.0) max_segment_speed = 1.0;
    speed_axis_max =
        scene_editor_light_timeline_nice_ceiling(max_segment_speed);

    SDL_SetRenderDrawColor(
        renderer, palette.panel_fill.r, palette.panel_fill.g,
        palette.panel_fill.b, 255);
    SDL_RenderFillRect(renderer, panel);
    SDL_SetRenderDrawColor(
        renderer, palette.panel_border.r, palette.panel_border.g,
        palette.panel_border.b, palette.panel_border.a);
    SDL_RenderDrawRect(renderer, panel);

    title_font = ray_tracing_font_runtime_get_ui_regular(
        renderer, panel->h >= 260 ? 13 : 12, 10);
    font = ray_tracing_font_runtime_get_ui_regular(renderer, 10, 8);
    small_font = ray_tracing_font_runtime_get_ui_regular(renderer, 9, 7);

    panel_text(renderer, title_font, "LIGHT MOTION",
               panel->x + 12, panel->y + 5, palette.text_primary);
    if (panel->w >= 620) {
        snprintf(label, sizeof(label),
                 "F %lld/%lld | %.2fs | %.1f%% path | %.2f world/s | %llu frames @ %.2f fps",
                 (long long)state->current_frame,
                 (long long)(document->timeline.range.start_frame +
                     (int64_t)document->timeline.range.frame_count - 1),
                 (double)(state->current_frame -
                          document->timeline.range.start_frame) / fps,
                 state->evaluated_scene->light.progress * 100.0,
                 state->evaluated_scene->light.speed_valid
                     ? state->evaluated_scene->light.world_speed_per_second
                     : 0.0,
                 (unsigned long long)document->timeline.range.frame_count,
                 fps);
    } else {
        snprintf(label, sizeof(label),
                 "F %lld/%lld | %.2fs | %.0f%% | %.2f world/s",
                 (long long)state->current_frame,
                 (long long)(document->timeline.range.start_frame +
                     (int64_t)document->timeline.range.frame_count - 1),
                 (double)(state->current_frame -
                          document->timeline.range.start_frame) / fps,
                 state->evaluated_scene->light.progress * 100.0,
                 state->evaluated_scene->light.speed_valid
                     ? state->evaluated_scene->light.world_speed_per_second
                     : 0.0);
    }
    panel_text(renderer, font, label, geometry.metrics_line.x,
               geometry.metrics_line.y, palette.text_muted);

    panel_mode_control(
        renderer, small_font, &geometry.constant_speed_button,
        panel->w >= 460 ? "CONST SPEED" : "CONST",
        state->traversal_mode ==
            SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CONSTANT_SPEED,
        panel->w >= 460 ? 17 : 16, &palette);
    panel_mode_control(
        renderer, small_font, &geometry.equal_segments_button,
        panel->w >= 460 ? "EQUAL TIME" : "EQUAL",
        state->traversal_mode ==
            SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_EQUAL_SEGMENTS,
        panel->w >= 460 ? 17 : 16, &palette);
    panel_mode_status(
        renderer, small_font, &geometry.custom_mode_indicator,
        state->traversal_mode ==
            SCENE_EDITOR_LIGHT_TIMELINE_TRAVERSAL_CUSTOM,
        &palette);
    {
        TimelineInterpolation interpolation =
            TIMELINE_INTERPOLATION_STEP;
        bool selectable =
            state->selected_key_index >= 0 &&
            state->selected_key_index + 1 < (int)track->key_count;
        if (selectable) {
            interpolation =
                track->keys[state->selected_key_index].
                    interpolation_to_next;
        }
        panel_mode_control(
            renderer, small_font, &geometry.step_button, "STEP",
            selectable &&
                interpolation == TIMELINE_INTERPOLATION_STEP,
            9, &palette);
        panel_mode_control(
            renderer, small_font, &geometry.linear_button, "LINEAR",
            selectable &&
                interpolation == TIMELINE_INTERPOLATION_LINEAR,
            7, &palette);
        panel_mode_control(
            renderer, small_font, &geometry.bezier_button,
            panel->w >= 460 ? "BEZIER" : "BEZ",
            selectable &&
                interpolation ==
                    TIMELINE_INTERPOLATION_CUBIC_BEZIER,
            panel->w >= 460 ? 6 : 9, &palette);
    }

    {
        SDL_Color fill = panel_color_offset(palette.button_fill, -8, 255);
        SDL_Color text_color =
            ray_tracing_theme_choose_button_text(fill, palette);
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_RenderFillRect(renderer, &geometry.add_key_button);
        SDL_SetRenderDrawColor(
            renderer, palette.accent_primary.r, palette.accent_primary.g,
            palette.accent_primary.b, palette.accent_primary.a);
        SDL_RenderDrawRect(renderer, &geometry.add_key_button);
        panel_text(renderer, small_font, "+ KEY",
                   geometry.add_key_button.x + 10,
                   geometry.add_key_button.y + 4, text_color);
    }
    {
        SDL_Color fill = state->playing
            ? ray_tracing_theme_resolve_button_active_fill(palette)
            : panel_color_offset(palette.button_fill, -8, 255);
        SDL_Color text_color =
            ray_tracing_theme_choose_button_text(fill, palette);
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_RenderFillRect(renderer, &geometry.play_button);
        SDL_SetRenderDrawColor(
            renderer, palette.panel_border.r, palette.panel_border.g,
            palette.panel_border.b, palette.panel_border.a);
        SDL_RenderDrawRect(renderer, &geometry.play_button);
        panel_text(renderer, small_font,
                   state->playing ? "PAUSE" : "PLAY",
                   geometry.play_button.x + (state->playing ? 10 : 14),
                   geometry.play_button.y + 4, text_color);
    }

    SDL_SetRenderDrawColor(
        renderer, graph_fill.r, graph_fill.g, graph_fill.b, graph_fill.a);
    SDL_RenderFillRect(renderer, &graph);
    SDL_SetRenderDrawColor(
        renderer, palette.panel_border.r, palette.panel_border.g,
        palette.panel_border.b, palette.panel_border.a);
    SDL_RenderDrawRect(renderer, &graph);

    for (int division = 0; division <= 4; ++division) {
        int y = graph.y + graph.h -
                (int)llround((double)division / 4.0 * graph.h);
        SDL_SetRenderDrawColor(
            renderer, grid_color.r, grid_color.g, grid_color.b, grid_color.a);
        SDL_RenderDrawLine(renderer, graph.x, y, graph.x + graph.w, y);
        snprintf(label, sizeof(label), "%d", division * 25);
        panel_text(renderer, small_font, label, graph.x - 28, y - 5,
                   palette.text_muted);
    }
    for (int division = 0; division <= grid_x; ++division) {
        double normalized = visible_start +
            (double)division / (double)grid_x * visible_span;
        int x = graph.x +
            (int)llround((double)division / (double)grid_x * graph.w);
        int64_t frame = document->timeline.range.start_frame +
            (int64_t)llround(normalized *
                (double)(document->timeline.range.frame_count - 1u));
        SDL_SetRenderDrawColor(
            renderer, grid_color.r, grid_color.g, grid_color.b, grid_color.a);
        SDL_RenderDrawLine(renderer, x, graph.y, x, graph.y + graph.h);
        snprintf(label, sizeof(label), "%lld", (long long)frame);
        int label_x = x - 7;
        if (division == 0) label_x = graph.x;
        if (division == grid_x) label_x = graph.x + graph.w - 18;
        panel_text(renderer, small_font, label, label_x,
                   graph.y + graph.h + 2,
                   palette.text_muted);
    }
    panel_text(renderer, small_font, "PATH %", graph.x,
               graph.y - 12, timing_color);
    panel_text(renderer, small_font, "FRAME", graph.x + graph.w - 34,
               graph.y - 12, palette.text_muted);

    for (int i = 0; i < document->spatial_path.numPoints; ++i) {
        double progress;
        int y;
        if (!scene_editor_light_timeline_path_anchor_progress(
                document, i, &progress)) {
            continue;
        }
        y = graph.y + graph.h -
            (int)llround(progress * graph.h);
        {
            SDL_Color guide =
                panel_color_offset(palette.accent_primary, -36, 105);
            SDL_SetRenderDrawColor(
                renderer, guide.r, guide.g, guide.b, guide.a);
        }
        for (int x = graph.x; x < graph.x + graph.w; x += 8) {
            SDL_RenderDrawLine(renderer, x, y,
                               x + 3 < graph.x + graph.w ? x + 3
                                                         : graph.x + graph.w,
                               y);
        }
    }

    if (selected_segment >= 0 &&
        selected_segment + 1 < (int)track->key_count) {
        const TimelineKeyframe* left = &track->keys[selected_segment];
        const TimelineKeyframe* right = &track->keys[selected_segment + 1];
        double n0 = (double)(left->frame -
            document->timeline.range.start_frame) /
            (double)(document->timeline.range.frame_count - 1u);
        double n1 = (double)(right->frame -
            document->timeline.range.start_frame) /
            (double)(document->timeline.range.frame_count - 1u);
        int x0 = scene_editor_light_timeline_view_x_at_normalized(
            state->view, &graph, n0);
        int x1 = scene_editor_light_timeline_view_x_at_normalized(
            state->view, &graph, n1);
        int clipped_x0 = x0 < graph.x ? graph.x : x0;
        int clipped_x1 =
            x1 > graph.x + graph.w ? graph.x + graph.w : x1;
        SDL_Rect selected = {
            clipped_x0,
            graph.y,
            clipped_x1 - clipped_x0,
            graph.h
        };
        if (selected.w > 0) {
            SDL_SetRenderDrawColor(
                renderer, timing_color.r, timing_color.g,
                timing_color.b, 28);
            SDL_RenderFillRect(renderer, &selected);
        }
    }

    {
        int prior_x = graph.x;
        int prior_y = graph.y + graph.h;
        bool have_prior = false;
        for (int x = graph.x; x <= graph.x + graph.w; ++x) {
            double normalized = visible_start +
                (double)(x - graph.x) / (double)(graph.w > 0 ? graph.w : 1) *
                    visible_span;
            double frame_position =
                (double)document->timeline.range.start_frame +
                normalized *
                    (double)(document->timeline.range.frame_count - 1u);
            double progress;
            if (panel_evaluate_progress(document, track, frame_position,
                                        &progress)) {
                int y = graph.y + graph.h -
                    (int)llround(progress * graph.h);
                if (have_prior) {
                    SDL_SetRenderDrawColor(
                        renderer, timing_color.r, timing_color.g,
                        timing_color.b, 255);
                    SDL_RenderDrawLine(renderer, prior_x, prior_y, x, y);
                    {
                        SDL_Color shadow =
                            panel_color_offset(timing_color, -60, 210);
                        SDL_SetRenderDrawColor(
                            renderer, shadow.r, shadow.g,
                            shadow.b, shadow.a);
                    }
                    SDL_RenderDrawLine(renderer, prior_x, prior_y + 1,
                                       x, y + 1);
                }
                prior_x = x;
                prior_y = y;
                have_prior = true;
            }
        }
    }

    if (state->selected_key_index >= 0 &&
        (size_t)state->selected_key_index < track->key_count) {
        const size_t key_index =
            (size_t)state->selected_key_index;
        int key_x = 0;
        int key_y = 0;
        scene_editor_light_timeline_key_point(
            state->view, document, &graph, &track->keys[key_index],
            &key_x, &key_y);
        const SceneEditorLightTimelineHandle handles[] = {
            SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_INCOMING,
            SCENE_EDITOR_LIGHT_TIMELINE_HANDLE_OUTGOING
        };
        for (size_t i = 0u;
             i < sizeof(handles) / sizeof(handles[0]); ++i) {
            int handle_x = 0;
            int handle_y = 0;
            if (!scene_editor_light_timeline_handle_point(
                    state->view, document, track, key_index,
                    handles[i], &graph, &handle_x, &handle_y)) {
                continue;
            }
            SDL_SetRenderDrawColor(
                renderer, 255, 218, 92, 185);
            SDL_RenderDrawLine(
                renderer, key_x, key_y, handle_x, handle_y);
            {
                SDL_Rect handle_rect = {
                    handle_x - 4, handle_y - 4, 9, 9
                };
                SDL_SetRenderDrawColor(
                    renderer, 255, 218, 92, 255);
                SDL_RenderFillRect(renderer, &handle_rect);
                SDL_SetRenderDrawColor(
                    renderer, palette.panel_border.r,
                    palette.panel_border.g, palette.panel_border.b,
                    palette.panel_border.a);
                SDL_RenderDrawRect(renderer, &handle_rect);
            }
        }
    }

    for (size_t i = 0u; i < track->key_count; ++i) {
        int x = 0;
        int y = 0;
        bool active = (int)i == state->selected_key_index;
        scene_editor_light_timeline_key_point(
            state->view, document, &graph, &track->keys[i], &x, &y);
        if (x < graph.x || x > graph.x + graph.w) continue;
        SDL_SetRenderDrawColor(renderer,
                               active ? 255 : 226,
                               active ? 218 : 181,
                               active ? 92 : 88, 255);
        SDL_Rect key = {x - (active ? 5 : 4), y - (active ? 5 : 4),
                        active ? 11 : 9, active ? 11 : 9};
        SDL_RenderFillRect(renderer, &key);
        SDL_SetRenderDrawColor(
            renderer, palette.panel_border.r, palette.panel_border.g,
            palette.panel_border.b, palette.panel_border.a);
        SDL_RenderDrawRect(renderer, &key);
        if (active) {
            snprintf(label, sizeof(label), "F%lld  %.1f%%",
                     (long long)track->keys[i].frame,
                     track->keys[i].value.as.scalar * 100.0);
            panel_text(renderer, small_font, label,
                       x > graph.x + graph.w - 84 ? x - 80 : x + 8,
                       y > graph.y + 18 ? y - 15 : y + 8,
                       (SDL_Color){255, 230, 151, 255});
        }
    }

    SDL_SetRenderDrawColor(
        renderer, graph_fill.r, graph_fill.g, graph_fill.b, graph_fill.a);
    SDL_RenderFillRect(renderer, &path_point_strip);
    SDL_SetRenderDrawColor(
        renderer, palette.panel_border.r, palette.panel_border.g,
        palette.panel_border.b, palette.panel_border.a);
    SDL_RenderDrawRect(renderer, &path_point_strip);
    panel_text(renderer, small_font, "PATH PTS",
               path_point_strip.x - 48,
               path_point_strip.y + path_point_strip.h / 2 - 5,
               timing_color);
    {
        int prior_label_right = path_point_strip.x - 1;
        for (int i = 0; i < document->spatial_path.numPoints; ++i) {
            double progress;
            double frame_position;
            double normalized;
            int x;
            int center_y = path_point_strip.y + path_point_strip.h / 2;
            bool active = i == state->selected_path_point_index;
            if (!scene_editor_light_timeline_path_anchor_progress(
                    document, i, &progress) ||
                !scene_editor_light_timeline_frame_at_progress(
                    document, track, progress, &frame_position)) {
                continue;
            }
            normalized =
                (frame_position -
                 (double)document->timeline.range.start_frame) /
                (double)(document->timeline.range.frame_count - 1u);
            x = scene_editor_light_timeline_view_x_at_normalized(
                state->view, &path_point_strip, normalized);
            if (x < path_point_strip.x ||
                x > path_point_strip.x + path_point_strip.w) {
                continue;
            }
            SDL_SetRenderDrawColor(renderer,
                                   active ? 255 : 207,
                                   active ? 218 : 163,
                                   active ? 92 : 75, 255);
            {
                SDL_Rect marker = {
                    x - (active ? 5 : 4),
                    center_y - (active ? 5 : 4),
                    active ? 11 : 9,
                    active ? 11 : 9
                };
                SDL_RenderFillRect(renderer, &marker);
            }
            snprintf(label, sizeof(label), "P%d", i);
            {
                int label_x = x + 6;
                int label_width = panel_text_width(small_font, label);
                bool endpoint =
                    i == 0 || i + 1 == document->spatial_path.numPoints;
                bool show_label = active || endpoint;
                if (label_x >
                    path_point_strip.x + path_point_strip.w - 20) {
                    label_x = x - label_width - 6;
                }
                if (!show_label && label_x >= prior_label_right + 5) {
                    show_label = true;
                }
                if (show_label) {
                    panel_text(renderer, small_font, label, label_x,
                               path_point_strip.y + 2,
                               palette.text_primary);
                    prior_label_right = label_x + label_width;
                }
            }
        }
    }

    SDL_SetRenderDrawColor(
        renderer, graph_fill.r, graph_fill.g, graph_fill.b, graph_fill.a);
    SDL_RenderFillRect(renderer, &speed_strip);
    SDL_SetRenderDrawColor(
        renderer, palette.panel_border.r, palette.panel_border.g,
        palette.panel_border.b, palette.panel_border.a);
    SDL_RenderDrawRect(renderer, &speed_strip);
    panel_text(renderer, small_font, "AVG SPEED world/s", speed_strip.x + 6,
               speed_strip.y + 2, speed_color);
    speed_header_width = panel_text_width(small_font, "AVG SPEED world/s");
    panel_text(renderer, small_font, "0", speed_strip.x - 16,
               speed_strip.y + speed_strip.h - 10, palette.text_muted);
    snprintf(label, sizeof(label), "%.2g", speed_axis_max);
    panel_text(renderer, small_font, label, speed_strip.x - 38,
               speed_strip.y + 13, palette.text_muted);
    snprintf(label, sizeof(label), "%.2g", speed_axis_max * 0.5);
    panel_text(
        renderer, small_font, label, speed_strip.x - 38,
        speed_strip.y + 15 + (speed_strip.h - 16) / 2 - 5,
        palette.text_muted);
    SDL_SetRenderDrawColor(
        renderer, grid_color.r, grid_color.g, grid_color.b, grid_color.a);
    SDL_RenderDrawLine(
        renderer, speed_strip.x, speed_strip.y + 15,
        speed_strip.x + speed_strip.w, speed_strip.y + 15);
    SDL_RenderDrawLine(
        renderer, speed_strip.x,
        speed_strip.y + 15 + (speed_strip.h - 16) / 2,
        speed_strip.x + speed_strip.w,
        speed_strip.y + 15 + (speed_strip.h - 16) / 2);

    for (size_t i = 0u; i + 1u < track->key_count; ++i) {
        double n0 = (double)(track->keys[i].frame -
            document->timeline.range.start_frame) /
            (double)(document->timeline.range.frame_count - 1u);
        double n1 = (double)(track->keys[i + 1u].frame -
            document->timeline.range.start_frame) /
            (double)(document->timeline.range.frame_count - 1u);
        double speed = panel_segment_speed(track, i, path_length, fps);
        int x0 = scene_editor_light_timeline_view_x_at_normalized(
            state->view, &speed_strip, n0);
        int x1 = scene_editor_light_timeline_view_x_at_normalized(
            state->view, &speed_strip, n1);
        int clipped_x0 = x0 < speed_strip.x ? speed_strip.x : x0;
        int clipped_x1 = x1 > speed_strip.x + speed_strip.w
                             ? speed_strip.x + speed_strip.w
                             : x1;
        int bar_height = speed_strip.h - 16;
        int height = (int)llround(
            speed / speed_axis_max * (double)bar_height);
        SDL_Rect bar = {
            clipped_x0,
            speed_strip.y + speed_strip.h - height - 1,
            clipped_x1 - clipped_x0,
            height
        };
        if (bar.w <= 0 || bar.h <= 0) continue;
        SDL_SetRenderDrawColor(
            renderer, speed_color.r, speed_color.g, speed_color.b,
            (int)i == selected_segment ? 220 : 125);
        SDL_RenderFillRect(renderer, &bar);
        SDL_SetRenderDrawColor(
            renderer, speed_color.r, speed_color.g, speed_color.b, 255);
        SDL_RenderDrawLine(renderer, bar.x, bar.y,
                           bar.x + bar.w, bar.y);
        if (i > 0u && x0 >= speed_strip.x &&
            x0 <= speed_strip.x + speed_strip.w) {
            SDL_SetRenderDrawColor(
                renderer, palette.panel_border.r,
                palette.panel_border.g, palette.panel_border.b, 205);
            SDL_RenderDrawLine(
                renderer, x0, speed_strip.y + 15,
                x0, speed_strip.y + speed_strip.h);
        }
    }

    {
        double play_n = (double)(state->current_frame -
            document->timeline.range.start_frame) /
            (double)(document->timeline.range.frame_count - 1u);
        int play_x = scene_editor_light_timeline_view_x_at_normalized(
            state->view, &graph, play_n);
        if (play_x >= graph.x && play_x <= graph.x + graph.w) {
            SDL_SetRenderDrawColor(renderer, 255, 91, 91, 225);
            SDL_RenderDrawLine(renderer, play_x, graph.y,
                               play_x, speed_strip.y + speed_strip.h);
        }
    }

    if (selected_segment >= 0 &&
        selected_segment + 1 < (int)track->key_count) {
        const TimelineKeyframe* left = &track->keys[selected_segment];
        const TimelineKeyframe* right = &track->keys[selected_segment + 1];
        double seconds = (double)(right->frame - left->frame) / fps;
        double progress = right->value.as.scalar - left->value.as.scalar;
        double speed =
            panel_segment_speed(track, (size_t)selected_segment,
                                path_length, fps);
        if (speed_strip.w >= 620) {
            snprintf(label, sizeof(label),
                     "S%d | F%lld-%lld | %.1f%% path | %.2fs | %.2f world/s",
                     selected_segment + 1,
                     (long long)left->frame, (long long)right->frame,
                     progress * 100.0, seconds, speed);
        } else if (speed_strip.w >= 360) {
            snprintf(label, sizeof(label),
                     "S%d | F%lld-%lld | %.2fs | %.2f world/s",
                     selected_segment + 1,
                     (long long)left->frame, (long long)right->frame,
                     seconds, speed);
        } else {
            snprintf(label, sizeof(label),
                     "S%d | %.2f world/s", selected_segment + 1, speed);
        }
        panel_text(renderer, small_font, label,
                   speed_strip.x + speed_header_width + 14,
                   speed_strip.y + 2,
                   palette.text_primary);
    }

    if (state->pointer_over_panel &&
        (panel_point_in_rect(state->mouse_x, state->mouse_y, &graph) ||
         panel_point_in_rect(
             state->mouse_x, state->mouse_y, &speed_strip))) {
        double normalized =
            scene_editor_light_timeline_view_normalized_at_x(
                state->view, &graph, state->mouse_x);
        double frame_position =
            (double)document->timeline.range.start_frame +
            normalized *
                (double)(document->timeline.range.frame_count - 1u);
        double progress = 0.0;
        int64_t hover_frame = (int64_t)llround(frame_position);
        int hover_segment = panel_segment_for_frame(track, hover_frame);
        double hover_speed =
            hover_segment >= 0
                ? panel_segment_speed(
                      track, (size_t)hover_segment, path_length, fps)
                : 0.0;
        int guide_x =
            scene_editor_light_timeline_view_x_at_normalized(
                state->view, &graph, normalized);
        if (panel_evaluate_progress(
                document, track, frame_position, &progress)) {
            int text_width;
            int box_x;
            SDL_Rect box;
            snprintf(label, sizeof(label),
                     "F%lld | %.2fs | %.1f%% | %.2f world/s",
                     (long long)hover_frame,
                     (frame_position -
                      (double)document->timeline.range.start_frame) / fps,
                     progress * 100.0, hover_speed);
            text_width = panel_text_width(small_font, label);
            box_x = guide_x + 7;
            if (box_x + text_width + 10 > graph.x + graph.w) {
                box_x = guide_x - text_width - 17;
            }
            if (box_x < graph.x + 3) box_x = graph.x + 3;
            box = (SDL_Rect){
                box_x, graph.y + 4, text_width + 10, 16
            };
            SDL_SetRenderDrawColor(
                renderer, speed_color.r, speed_color.g,
                speed_color.b, 115);
            SDL_RenderDrawLine(
                renderer, guide_x, graph.y,
                guide_x, speed_strip.y + speed_strip.h);
            SDL_SetRenderDrawColor(
                renderer, graph_fill.r, graph_fill.g,
                graph_fill.b, 242);
            SDL_RenderFillRect(renderer, &box);
            SDL_SetRenderDrawColor(
                renderer, palette.panel_border.r,
                palette.panel_border.g, palette.panel_border.b,
                palette.panel_border.a);
            SDL_RenderDrawRect(renderer, &box);
            panel_text(
                renderer, small_font, label, box.x + 5, box.y + 3,
                palette.text_primary);
        }
    }

    panel_text(
        renderer, small_font,
        panel->w >= 620
            ? "Select key, choose STEP/LINEAR/BEZIER | Drag Bezier handles | Double-click: add | Wheel: zoom | F: fit"
            : "Drag | Double-click add | Wheel zoom | F fit",
        geometry.footer_hint.x, geometry.footer_hint.y,
        palette.text_muted);
}

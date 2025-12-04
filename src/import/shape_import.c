#include "import/shape_import.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ShapeLib/shape_flatten.h"
#include "ShapeLib/shape_json.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void mask_set(uint8_t *mask, int w, int h, int x, int y) {
    if (!mask) return;
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    mask[(size_t)y * (size_t)w + (size_t)x] = 1;
}

bool shape_import_load(const char *path, ShapeDocument *out_doc) {
    if (!path || !out_doc) return false;
    return ShapeDocument_LoadFromJsonFile(path, out_doc);
}

static ShapeVec2 rotate_about(ShapeVec2 p, float cx, float cy, float cos_a, float sin_a) {
    float dx = p.x - cx;
    float dy = p.y - cy;
    ShapeVec2 r;
    r.x = dx * cos_a - dy * sin_a + cx;
    r.y = dx * sin_a + dy * cos_a + cy;
    return r;
}

bool shape_import_bounds(const Shape *shape, ShapeBounds *out_bounds) {
    if (!shape || !out_bounds) return false;
    PolylineSet set;
    if (!Shape_FlattenToPolylines(shape, 0.5f, &set)) {
        memset(out_bounds, 0, sizeof(*out_bounds));
        out_bounds->valid = false;
        return false;
    }
    float min_x = FLT_MAX, min_y = FLT_MAX;
    float max_x = -FLT_MAX, max_y = -FLT_MAX;
    bool has_points = false;
    for (size_t i = 0; i < set.count; ++i) {
        const Polyline *line = &set.lines[i];
        for (size_t j = 0; j < line->count; ++j) {
            ShapeVec2 p = line->points[j];
            if (p.x < min_x) min_x = p.x;
            if (p.x > max_x) max_x = p.x;
            if (p.y < min_y) min_y = p.y;
            if (p.y > max_y) max_y = p.y;
            has_points = true;
        }
    }
    ShapeBounds b = {0};
    if (has_points) {
        b.min_x = min_x;
        b.min_y = min_y;
        b.max_x = max_x;
        b.max_y = max_y;
        b.valid = true;
    }
    *out_bounds = b;
    PolylineSet_Free(&set);
    return has_points;
}

static void fit_transform(const ShapeBounds *b,
                          int grid_w,
                          int grid_h,
                          float margin,
                          float *out_scale,
                          float *out_offset_x,
                          float *out_offset_y,
                          float *out_center_x,
                          float *out_center_y) {
    float width = b->max_x - b->min_x;
    float height = b->max_y - b->min_y;
    float avail_w = (float)(grid_w - 1) - margin * 2.0f;
    float avail_h = (float)(grid_h - 1) - margin * 2.0f;
    if (avail_w <= 0.0f) avail_w = (float)(grid_w - 1);
    if (avail_h <= 0.0f) avail_h = (float)(grid_h - 1);

    float scale_x = width > 1e-5f ? (avail_w / width) : avail_w;
    float scale_y = height > 1e-5f ? (avail_h / height) : avail_h;
    float scale = fminf(scale_x, scale_y);
    if (scale <= 0.0f) scale = 1.0f;

    float center_x = 0.5f * (b->min_x + b->max_x);
    float center_y = 0.5f * (b->min_y + b->max_y);
    float offset_x = 0.5f * (float)(grid_w - 1) - center_x * scale;
    float offset_y = 0.5f * (float)(grid_h - 1) - center_y * scale;

    if (out_scale) *out_scale = scale;
    if (out_offset_x) *out_offset_x = offset_x;
    if (out_offset_y) *out_offset_y = offset_y;
    if (out_center_x) *out_center_x = center_x;
    if (out_center_y) *out_center_y = center_y;
}

static void world_to_grid(float px, float py,
                          float scale, float ox, float oy,
                          int *out_x, int *out_y) {
    float gx = px * scale + ox;
    float gy = py * scale + oy;
    if (out_x) *out_x = (int)lroundf(gx);
    if (out_y) *out_y = (int)lroundf(gy);
}

static void rasterize_segment(uint8_t *mask,
                              int w, int h,
                              ShapeVec2 a,
                              ShapeVec2 b,
                              float center_x, float center_y,
                              float cos_a, float sin_a,
                              float scale, float ox, float oy,
                              float stroke) {
    ShapeVec2 ra = rotate_about(a, center_x, center_y, cos_a, sin_a);
    ShapeVec2 rb = rotate_about(b, center_x, center_y, cos_a, sin_a);
    int ax, ay, bx, by;
    world_to_grid(ra.x, ra.y, scale, ox, oy, &ax, &ay);
    world_to_grid(rb.x, rb.y, scale, ox, oy, &bx, &by);
    int dx = bx - ax;
    int dy = by - ay;
    int steps = (int)ceilf(fmaxf(fabsf((float)dx), fabsf((float)dy)));
    if (steps < 1) steps = 1;
    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / (float)steps;
        float gx = (1.0f - t) * (float)ax + t * (float)bx;
        float gy = (1.0f - t) * (float)ay + t * (float)by;
        int cx = (int)lroundf(gx);
        int cy = (int)lroundf(gy);
        int half = (int)ceilf(fmaxf(stroke * 0.5f, 0.5f));
        for (int yy = cy - half; yy <= cy + half; ++yy) {
            for (int xx = cx - half; xx <= cx + half; ++xx) {
                mask_set(mask, w, h, xx, yy);
            }
        }
    }
}

static bool point_in_polygon(const ShapeVec2 *pts, size_t count, float x, float y) {
    bool inside = false;
    if (!pts || count < 3) return false;
    for (size_t i = 0, j = count - 1; i < count; j = i++) {
        float xi = pts[i].x, yi = pts[i].y;
        float xj = pts[j].x, yj = pts[j].y;
        bool intersect = ((yi > y) != (yj > y)) &&
                         (x < (xj - xi) * (y - yi) / ((yj - yi) + 1e-6f) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

static void fill_polygon(uint8_t *mask,
                         int w, int h,
                         const Polyline *line,
                         float center_x, float center_y,
                         float cos_a, float sin_a,
                         float scale, float ox, float oy) {
    if (!line || !line->closed || line->count < 3) return;
    float inv_scale = (scale > 1e-6f) ? (1.0f / scale) : 1.0f;
    const ShapeVec2 *pts = line->points;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float wx = ((float)x - ox) * inv_scale;
            float wy = ((float)y - oy) * inv_scale;
            float dx = wx - center_x;
            float dy = wy - center_y;
            float lx = dx * cos_a + dy * sin_a + center_x;
            float ly = -dx * sin_a + dy * cos_a + center_y;
            if (point_in_polygon(pts, line->count, lx, ly)) {
                mask_set(mask, w, h, x, y);
            }
        }
    }
}

bool shape_import_rasterize(const Shape *shape,
                            int grid_w,
                            int grid_h,
                            const ShapeRasterOptions *opts,
                            uint8_t *mask_out) {
    if (!shape || grid_w <= 0 || grid_h <= 0 || !mask_out) return false;
    memset(mask_out, 0, (size_t)grid_w * (size_t)grid_h);

    float max_error = 0.5f;
    float margin = 2.0f;
    float stroke = 1.0f;
    float pos_x = 0.5f;
    float pos_y = 0.5f;
    float rotation_deg = 0.0f;
    float user_scale = 1.0f;
    bool center_fit = true;
    if (opts) {
        if (opts->max_error > 0.0f) max_error = opts->max_error;
        if (opts->margin_cells >= 0.0f) margin = opts->margin_cells;
        if (opts->stroke > 0.0f) stroke = opts->stroke;
        if (opts->position_x_norm >= 0.0f && opts->position_x_norm <= 1.0f) pos_x = opts->position_x_norm;
        if (opts->position_y_norm >= 0.0f && opts->position_y_norm <= 1.0f) pos_y = opts->position_y_norm;
        rotation_deg = opts->rotation_deg;
        if (opts->scale > 0.0f) user_scale = opts->scale;
        center_fit = opts->center_fit;
    }

    PolylineSet set;
    if (!Shape_FlattenToPolylines(shape, max_error, &set)) {
        return false;
    }

    ShapeBounds bounds = {0};
    if (!shape_import_bounds(shape, &bounds) || !bounds.valid) {
        PolylineSet_Free(&set);
        return false;
    }

    float scale = 1.0f, ox = 0.0f, oy = 0.0f;
    float center_x = 0.5f * (bounds.min_x + bounds.max_x);
    float center_y = 0.5f * (bounds.min_y + bounds.max_y);

    if (center_fit) {
        fit_transform(&bounds, grid_w, grid_h, margin, &scale, &ox, &oy, &center_x, &center_y);
    } else {
        scale = user_scale;
        float target_x = pos_x * (float)(grid_w - 1);
        float target_y = pos_y * (float)(grid_h - 1);
        ox = target_x - center_x * scale;
        oy = target_y - center_y * scale;
    }

    float radians = rotation_deg * (float)M_PI / 180.0f;
    float cos_a = cosf(radians);
    float sin_a = sinf(radians);

    for (size_t i = 0; i < set.count; ++i) {
        Polyline *line = &set.lines[i];
        if (!line || line->count < 2) continue;
        for (size_t j = 1; j < line->count; ++j) {
            ShapeVec2 a = line->points[j - 1];
            ShapeVec2 b = line->points[j];
            rasterize_segment(mask_out, grid_w, grid_h, a, b,
                              center_x, center_y, cos_a, sin_a,
                              scale, ox, oy, stroke);
        }
        if (line->closed) {
            ShapeVec2 a = line->points[line->count - 1];
            ShapeVec2 b = line->points[0];
            rasterize_segment(mask_out, grid_w, grid_h, a, b,
                              center_x, center_y, cos_a, sin_a,
                              scale, ox, oy, stroke);
            fill_polygon(mask_out, grid_w, grid_h, line,
                         center_x, center_y, cos_a, sin_a,
                         scale, ox, oy);
        }
    }

    PolylineSet_Free(&set);
    return true;
}

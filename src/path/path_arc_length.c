#include "path/path_system.h"

#include <math.h>
#include <string.h>

#define PATH_ARC_LENGTH_MAX_SAMPLES 8192
#define PATH_ARC_LENGTH_MAX_DEPTH 14
#define PATH_ARC_LENGTH_LEAF_SUBSTEPS 8

#ifndef PATH_ARC_LENGTH_REL_TOLERANCE
#define PATH_ARC_LENGTH_REL_TOLERANCE 0.0005
#endif

#ifndef PATH_ARC_LENGTH_ABS_TOLERANCE
#define PATH_ARC_LENGTH_ABS_TOLERANCE 0.001
#endif

typedef struct PathArcLengthSample {
    double global_t;
    Point point;
    double cumulative_length;
} PathArcLengthSample;

typedef struct PathArcLengthTable {
    PathArcLengthSample samples[PATH_ARC_LENGTH_MAX_SAMPLES];
    int count;
} PathArcLengthTable;

static double path_arc_length_clamp01(double value) {
    if (value < 0.0) return 0.0;
    if (value > 1.0) return 1.0;
    return value;
}

static double path_arc_length_distance(Point a, Point b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}

static double path_arc_length_interval_tolerance(double chord_length) {
    double relative_tol = chord_length * PATH_ARC_LENGTH_REL_TOLERANCE;
    if (relative_tol < PATH_ARC_LENGTH_ABS_TOLERANCE) {
        return PATH_ARC_LENGTH_ABS_TOLERANCE;
    }
    return relative_tol;
}

static bool path_arc_length_append_sample(PathArcLengthTable* table,
                                          double global_t,
                                          Point point) {
    PathArcLengthSample* previous = NULL;
    PathArcLengthSample* current = NULL;
    if (!table || table->count >= PATH_ARC_LENGTH_MAX_SAMPLES) {
        return false;
    }
    if (table->count > 0) {
        previous = &table->samples[table->count - 1];
        if (fabs(previous->global_t - global_t) <= 1e-12) {
            previous->point = point;
            return true;
        }
    }
    current = &table->samples[table->count++];
    current->global_t = global_t;
    current->point = point;
    current->cumulative_length = 0.0;
    if (previous) {
        current->cumulative_length =
            previous->cumulative_length + path_arc_length_distance(previous->point, point);
    }
    return true;
}

static bool path_arc_length_emit_interval_samples(const Path* path,
                                                  PathArcLengthTable* table,
                                                  double t0,
                                                  double t1) {
    for (int i = 1; i <= PATH_ARC_LENGTH_LEAF_SUBSTEPS; ++i) {
        double alpha = (double)i / (double)PATH_ARC_LENGTH_LEAF_SUBSTEPS;
        double sample_t = t0 + (t1 - t0) * alpha;
        Point sample_point = GetPositionAlongPath((Path*)path, sample_t);
        if (!path_arc_length_append_sample(table, sample_t, sample_point)) {
            return false;
        }
    }
    return true;
}

static bool path_arc_length_subdivide(const Path* path,
                                      PathArcLengthTable* table,
                                      double t0,
                                      Point p0,
                                      double t1,
                                      Point p1,
                                      int depth) {
    double t25 = t0 + (t1 - t0) * 0.25;
    double t50 = t0 + (t1 - t0) * 0.50;
    double t75 = t0 + (t1 - t0) * 0.75;
    Point p25 = GetPositionAlongPath((Path*)path, t25);
    Point p50 = GetPositionAlongPath((Path*)path, t50);
    Point p75 = GetPositionAlongPath((Path*)path, t75);
    double polyline_length = path_arc_length_distance(p0, p25) +
                             path_arc_length_distance(p25, p50) +
                             path_arc_length_distance(p50, p75) +
                             path_arc_length_distance(p75, p1);
    double chord_length = path_arc_length_distance(p0, p1);
    double tolerance = path_arc_length_interval_tolerance(chord_length);

    if (depth >= PATH_ARC_LENGTH_MAX_DEPTH ||
        (polyline_length - chord_length) <= tolerance ||
        (t1 - t0) <= 1e-6) {
        return path_arc_length_emit_interval_samples(path, table, t0, t1);
    }

    return path_arc_length_subdivide(path, table, t0, p0, t50, p50, depth + 1) &&
           path_arc_length_subdivide(path, table, t50, p50, t1, p1, depth + 1);
}

static bool path_arc_length_build_table(const Path* path, PathArcLengthTable* out_table) {
    int segment_count = 0;
    if (!out_table) return false;
    memset(out_table, 0, sizeof(*out_table));
    if (!path || path->numPoints < 2) {
        path_arc_length_append_sample(out_table, 0.0, (Point){0.0, 0.0});
        return true;
    }

    segment_count = path->numPoints - 1;
    if (!path_arc_length_append_sample(out_table, 0.0, GetPositionAlongPath((Path*)path, 0.0))) {
        return false;
    }

    for (int seg = 0; seg < segment_count; ++seg) {
        double t0 = (double)seg / (double)segment_count;
        double t1 = (double)(seg + 1) / (double)segment_count;
        Point p0 = GetPositionAlongPath((Path*)path, t0);
        Point p1 = GetPositionAlongPath((Path*)path, t1);
        if (!path_arc_length_subdivide(path, out_table, t0, p0, t1, p1, 0)) {
            return false;
        }
    }
    return true;
}

double PathApproximateLength(Path* path) {
    PathArcLengthTable table;
    if (!path_arc_length_build_table(path, &table) || table.count <= 0) {
        return 0.0;
    }
    return table.samples[table.count - 1].cumulative_length;
}

double PathResolveNormalizedGlobalT(const Path* path, double t) {
    PathArcLengthTable table;
    double total_length = 0.0;
    double target_length = 0.0;
    int lo = 1;
    int hi = 0;
    int mid = 0;

    if (!path || path->numPoints < 2) {
        return path_arc_length_clamp01(t);
    }
    if (!path_arc_length_build_table(path, &table) || table.count <= 1) {
        return path_arc_length_clamp01(t);
    }

    total_length = table.samples[table.count - 1].cumulative_length;
    if (total_length <= 1e-9) {
        return path_arc_length_clamp01(t);
    }

    target_length = path_arc_length_clamp01(t) * total_length;
    if (target_length <= 0.0) {
        return 0.0;
    }
    if (target_length >= total_length) {
        return 1.0;
    }

    hi = table.count - 1;
    while (lo <= hi) {
        mid = lo + (hi - lo) / 2;
        if (table.samples[mid].cumulative_length < target_length) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    if (lo <= 0) {
        return table.samples[0].global_t;
    }
    if (lo >= table.count) {
        return table.samples[table.count - 1].global_t;
    }

    {
        const PathArcLengthSample* prev = &table.samples[lo - 1];
        const PathArcLengthSample* next = &table.samples[lo];
        double span = next->cumulative_length - prev->cumulative_length;
        double alpha = 0.0;
        if (span > 1e-9) {
            alpha = (target_length - prev->cumulative_length) / span;
        }
        return prev->global_t + (next->global_t - prev->global_t) * alpha;
    }
}

void PathMapNormalizedT(const Path* path, double t, int* out_segment, double* out_local_t) {
    int segments = 1;
    double global_t = 0.0;
    double scaled_t = 0.0;
    int segment = 0;
    double local_t = 0.0;

    if (out_segment) *out_segment = 0;
    if (out_local_t) *out_local_t = 0.0;
    if (!path || path->numPoints < 2) {
        if (out_local_t) *out_local_t = path_arc_length_clamp01(t);
        return;
    }

    segments = path->numPoints - 1;
    global_t = PathResolveNormalizedGlobalT(path, t);
    if (global_t >= 1.0) {
        if (out_segment) *out_segment = segments - 1;
        if (out_local_t) *out_local_t = 1.0;
        return;
    }

    scaled_t = global_t * (double)segments;
    segment = (int)floor(scaled_t);
    if (segment < 0) segment = 0;
    if (segment >= segments) segment = segments - 1;
    local_t = scaled_t - (double)segment;
    if (local_t < 0.0) local_t = 0.0;
    if (local_t > 1.0) local_t = 1.0;

    if (out_segment) *out_segment = segment;
    if (out_local_t) *out_local_t = local_t;
}

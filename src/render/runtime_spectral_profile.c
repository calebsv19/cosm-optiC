#include "render/runtime_spectral_profile.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    double lo, hi, left, right, cumulative;
} Component;

struct RuntimeSpectralProfile {
    size_t segments, count;
    double watts;
    Component parts[];
};

static bool valid_input(double nm, double power) {
    return nm >= RUNTIME_SPECTRAL_MIN_NM && nm <= RUNTIME_SPECTRAL_MAX_NM && isfinite(power) &&
           power >= 0;
}

RuntimeSpectralProfile *RuntimeSpectral_Prepare(const RuntimeSpectralKnot *knots, size_t knot_count,
                                                const RuntimeSpectralLine *lines, size_t line_count) {
    size_t segments = knot_count ? knot_count - 1 : 0;
    size_t limit = (SIZE_MAX - sizeof(RuntimeSpectralProfile)) / sizeof(Component);
    if (knot_count == 1 || (knot_count && !knots) || (line_count && !lines) || segments > limit ||
        line_count > limit - segments)
        return NULL;
    for (size_t i = 0; i < knot_count; ++i) {
        if (!valid_input(knots[i].nm, knots[i].watts_per_nm) || (i && knots[i].nm <= knots[i - 1].nm))
            return NULL;
    }
    for (size_t i = 0; i < line_count; ++i) {
        if (!valid_input(lines[i].nm, lines[i].watts) || (i && lines[i].nm <= lines[i - 1].nm))
            return NULL;
    }
    size_t count = segments + line_count;
    RuntimeSpectralProfile *p = malloc(sizeof(*p) + count * sizeof(Component));
    if (!p)
        return NULL;
    p->segments = segments;
    p->count = count;
    p->watts = 0;
    for (size_t i = 0; i < count; ++i) {
        Component c;
        double mass;
        if (i < segments) {
            c = (Component){knots[i].nm, knots[i + 1].nm, knots[i].watts_per_nm,
                            knots[i + 1].watts_per_nm, 0};
            mass = (0.5 * c.left + 0.5 * c.right) * (c.hi - c.lo);
        } else {
            RuntimeSpectralLine line = lines[i - segments];
            c = (Component){line.nm, line.nm, line.watts, line.watts, 0};
            mass = line.watts;
        }
        c.cumulative = p->watts + mass;
        if (!isfinite(c.cumulative) || (fmax(c.left, c.right) > 0 && c.cumulative == p->watts)) {
            free(p);
            return NULL;
        }
        p->parts[i] = c;
        p->watts = c.cumulative;
    }
    return p;
}

void RuntimeSpectral_Free(RuntimeSpectralProfile *profile) { free(profile); }

double RuntimeSpectral_TotalWatts(const RuntimeSpectralProfile *profile) {
    return profile ? profile->watts : 0;
}

static double density(const Component *c, double nm) {
    double t = (nm - c->lo) / (c->hi - c->lo);
    return (1 - t) * c->left + t * c->right;
}

double RuntimeSpectral_Value(const RuntimeSpectralProfile *p, double nm,
                             RuntimeSpectralMeasure measure) {
    if (!p || !isfinite(nm) || (measure != SPECTRAL_LINE && measure != SPECTRAL_CONTINUUM))
        return 0;
    size_t lo = measure == SPECTRAL_LINE ? p->segments : 0;
    size_t end = measure == SPECTRAL_LINE ? p->count : p->segments;
    size_t hi = end;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (p->parts[mid].hi < nm)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == end || nm < p->parts[lo].lo)
        return 0;
    return measure == SPECTRAL_LINE ? p->parts[lo].left : density(&p->parts[lo], nm);
}

bool RuntimeSpectral_Sample(const RuntimeSpectralProfile *p, double u, RuntimeSpectralDraw *draw) {
    if (!p || !draw || !(u >= 0 && u < 1) || p->watts <= 0)
        return false;
    double target = fmin(u * p->watts, nextafter(p->watts, 0));
    size_t lo = 0, hi = p->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (p->parts[mid].cumulative <= target)
            lo = mid + 1;
        else
            hi = mid;
    }
    const Component *c = &p->parts[lo];
    if (lo >= p->segments) {
        *draw = (RuntimeSpectralDraw){c->lo, SPECTRAL_LINE, c->left / p->watts};
    } else {
        double start = lo ? p->parts[lo - 1].cumulative : 0;
        double v = (target - start) / (c->cumulative - start);
        /* Invert the trapezoid integral. Scale endpoints before squaring. */
        double scale = fmax(c->left, c->right);
        double a = c->left / scale, b = c->right / scale;
        double t = v == 0 ? 0 : v * (a + b) / (a + sqrt((1 - v) * a * a + v * b * b));
        double nm = fmin(c->hi, fmax(c->lo, c->lo + t * (c->hi - c->lo)));
        if (nm == c->lo && c->left == 0)
            nm = nextafter(nm, c->hi);
        if (nm == c->hi && c->right == 0)
            nm = nextafter(nm, c->lo);
        *draw = (RuntimeSpectralDraw){nm, SPECTRAL_CONTINUUM, density(c, nm) / p->watts};
    }
    return isfinite(draw->probability) && draw->probability > 0;
}

bool RuntimeSpectral_Covers(const RuntimeSpectralProfile *p, const RuntimeSpectralProfile *source) {
    if (!p || !source)
        return false;
    size_t j = 0;
    for (size_t i = 0; i < source->segments; ++i) {
        const Component *s = &source->parts[i];
        if (s->left == 0 && s->right == 0)
            continue;
        double x = s->lo;
        while (x < s->hi) {
            while (j < p->segments && p->parts[j].hi <= x)
                ++j;
            if (j == p->segments || p->parts[j].lo > x ||
                (p->parts[j].left == 0 && p->parts[j].right == 0))
                return false;
            x = fmin(p->parts[j].hi, s->hi);
        }
    }
    for (size_t i = source->segments; i < source->count; ++i) {
        if (source->parts[i].left > 0 &&
            RuntimeSpectral_Value(p, source->parts[i].lo, SPECTRAL_LINE) <= 0)
            return false;
    }
    return true;
}

bool RuntimeSpectral_EmissionWatts(const RuntimeSpectralProfile *source, RuntimeSpectralDraw draw,
                                   double emitter_probability, size_t trials, double *watts) {
    if (!source || !watts || !trials || !(emitter_probability > 0 && emitter_probability <= 1) ||
        !isfinite(draw.probability) || draw.probability <= 0 ||
        !(draw.nm >= RUNTIME_SPECTRAL_MIN_NM && draw.nm <= RUNTIME_SPECTRAL_MAX_NM) ||
        (draw.measure != SPECTRAL_LINE && draw.measure != SPECTRAL_CONTINUUM))
        return false;
    double value = RuntimeSpectral_Value(source, draw.nm, draw.measure);
    *watts = (value / (double)trials) / draw.probability / emitter_probability;
    return isfinite(*watts);
}

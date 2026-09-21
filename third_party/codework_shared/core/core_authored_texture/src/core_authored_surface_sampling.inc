/* Included in core_authored_texture.c to preserve the single-object link API. */
#include "core_authored_surface_sampling.h"
#include <stdlib.h>
void core_authored_surface_pyramid_free(CoreAuthoredSurfacePyramid *p) {
    if (p) {
        free(p->values);
        memset(p, 0, sizeof(*p));
    }
}
bool core_authored_surface_pyramid_build(CoreAuthoredSurfacePyramid *p, unsigned w, unsigned h,
                                         unsigned channels, const float *data) {
    if (!p || !data || !w || !h || w > 1024 || h > 1024 || (w & (w - 1)) || (h & (h - 1)) ||
        !channels || channels > 8)
        return false;
    CoreAuthoredSurfacePyramid next = {0};
    next.width = w;
    next.height = h;
    next.channels = channels;
    unsigned x = w, y = h;
    do {
        next.offsets[next.levels++] = next.value_count;
        next.value_count += (size_t)x * y * channels;
        if (x == 1 && y == 1)
            break;
        x = x > 1 ? x / 2 : 1;
        y = y > 1 ? y / 2 : 1;
    } while (next.levels < 11);
    next.values = malloc(next.value_count * sizeof(float));
    if (!next.values)
        return false;
    for (size_t i = 0; i < (size_t)w * h * channels; ++i)
        if (!isfinite(data[i])) {
            core_authored_surface_pyramid_free(&next);
            return false;
        }
    memcpy(next.values, data, (size_t)w * h * channels * sizeof(float));
    x = w;
    y = h;
    for (unsigned level = 1; level < next.levels; ++level) {
        unsigned nx = x > 1 ? x / 2 : 1, ny = y > 1 ? y / 2 : 1;
        for (unsigned v = 0; v < ny; ++v)
            for (unsigned u = 0; u < nx; ++u)
                for (unsigned c = 0; c < channels; ++c) {
                    double sum = 0;
                    for (unsigned j = 0; j < 2; ++j)
                        for (unsigned i = 0; i < 2; ++i)
                            sum +=
                                next.values[next.offsets[level - 1] +
                                            (((v * 2 + j) % y) * x + (u * 2 + i) % x) * channels +
                                            c] *
                                .25;
                    next.values[next.offsets[level] + (v * nx + u) * channels + c] = (float)sum;
                }
        x = nx;
        y = ny;
    }
    core_authored_surface_pyramid_free(p);
    *p = next;
    return true;
}
static void surface_pyramid_level(const CoreAuthoredSurfacePyramid *p, const double uv[2],
                                  unsigned level, double *out) {
    unsigned w = p->width >> level, h = p->height >> level;
    if (!w)
        w = 1;
    if (!h)
        h = 1;
    double x = (uv[0] - floor(uv[0])) * w - .5, y = (uv[1] - floor(uv[1])) * h - .5;
    int ix = (int)floor(x), iy = (int)floor(y);
    double fx = x - floor(x), fy = y - floor(y);
    for (unsigned c = 0; c < p->channels; ++c)
        out[c] = 0;
    for (int j = 0; j < 2; ++j)
        for (int i = 0; i < 2; ++i) {
            size_t at =
                p->offsets[level] +
                (((iy + j + (int)h) % (int)h) * w + (ix + i + (int)w) % (int)w) * p->channels;
            double weight = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
            for (unsigned c = 0; c < p->channels; ++c)
                out[c] += p->values[at + c] * weight;
        }
}
bool core_authored_surface_pyramid_sample(const CoreAuthoredSurfacePyramid *p, const double uv[2],
                                          const double dx[2], const double dy[2], double *out,
                                          double *out_lod) {
    if (!p || !p->values || !uv || !dx || !dy || !out)
        return false;
    for (int i = 0; i < 2; ++i)
        if (!isfinite(uv[i]) || !isfinite(dx[i]) || !isfinite(dy[i]))
            return false;
    /* Largest singular value of the texel-space pixel Jacobian. */
    double a = dx[0] * p->width, b = dy[0] * p->width, c = dx[1] * p->height, d = dy[1] * p->height;
    double aa = a * a + c * c, bb = b * b + d * d, ab = a * b + c * d;
    double radius = sqrt(fmax(0, .5 * (aa + bb + hypot(aa - bb, 2 * ab))));
    double lod = fmin(p->levels - 1, fmax(0, log2(fmax(1, radius))));
    if (!isfinite(lod))
        lod = p->levels - 1;
    unsigned lo = (unsigned)floor(lod), hi = lo + 1 < p->levels ? lo + 1 : lo;
    double next[8];
    surface_pyramid_level(p, uv, lo, out);
    surface_pyramid_level(p, uv, hi, next);
    for (unsigned i = 0; i < p->channels; ++i)
        out[i] += (next[i] - out[i]) * (lod - lo);
    if (out_lod)
        *out_lod = lod;
    return true;
}
double core_authored_surface_srgb_to_linear(double v) {
    if (!isfinite(v))
        return 0;
    v = fmax(0, fmin(1, v));
    return v <= .04045 ? v / 12.92 : pow((v + .055) / 1.055, 2.4);
}
static double surface_sampling_dot(const double *a, const double *b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
static bool surface_sampling_unit(double *v) {
    double length = hypot(hypot(v[0], v[1]), v[2]);
    if (!isfinite(length) || length < 1e-12)
        return false;
    for (int i = 0; i < 3; ++i)
        v[i] /= length;
    return true;
}
bool core_authored_surface_normal(const double normal[3], const double tangent[3], double sign,
                                  const double value[3], double out[3]) {
    if (!normal || !tangent || !value || !out || (sign != 1 && sign != -1))
        return false;
    double n[3], t[3], v[3];
    memcpy(n, normal, sizeof(n));
    memcpy(t, tangent, sizeof(t));
    memcpy(v, value, sizeof(v));
    if (!surface_sampling_unit(n) || !surface_sampling_unit(v) || v[2] < 0)
        return false;
    double dot = surface_sampling_dot(n, t);
    for (int i = 0; i < 3; ++i)
        t[i] -= dot * n[i];
    if (!surface_sampling_unit(t))
        return false;
    double b[] = {sign * (n[1] * t[2] - n[2] * t[1]), sign * (n[2] * t[0] - n[0] * t[2]),
                  sign * (n[0] * t[1] - n[1] * t[0])};
    for (int i = 0; i < 3; ++i)
        out[i] = t[i] * v[0] + b[i] * v[1] + n[i] * v[2];
    return surface_sampling_unit(out);
}
bool core_authored_surface_bump(const double normal[3], const double du[3], const double dv[3],
                                const double gradient[2], double out[3]) {
    if (!normal || !du || !dv || !gradient || !out || !isfinite(gradient[0]) ||
        !isfinite(gradient[1]))
        return false;
    double n[3], u[3], v[3];
    memcpy(n, normal, sizeof(n));
    if (!surface_sampling_unit(n))
        return false;
    double un = surface_sampling_dot(du, n), vn = surface_sampling_dot(dv, n);
    for (int i = 0; i < 3; ++i) {
        u[i] = du[i] - un * n[i];
        v[i] = dv[i] - vn * n[i];
    }
    double uu = surface_sampling_dot(u, u), vv = surface_sampling_dot(v, v),
           uv = surface_sampling_dot(u, v), det = uu * vv - uv * uv;
    if (!isfinite(det) || det <= 1e-20 * fmax(1, uu * vv))
        return false;
    for (int i = 0; i < 3; ++i)
        out[i] =
            n[i] -
            ((u[i] * vv - v[i] * uv) * gradient[0] + (v[i] * uu - u[i] * uv) * gradient[1]) / det;
    return surface_sampling_unit(out);
}

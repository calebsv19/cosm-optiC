#include "procedural_surface_field_graph_internal.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

static uint64_t mix64(uint64_t value) {
    value ^= value >> 30u;
    value *= UINT64_C(0xbf58476d1ce4e5b9);
    value ^= value >> 27u;
    value *= UINT64_C(0x94d049bb133111eb);
    value ^= value >> 31u;
    return value;
}

static uint64_t lattice_hash(
    uint64_t seed,
    int64_t x,
    int64_t y,
    int64_t z,
    uint64_t lane) {
    uint64_t value = seed ^ lane;
    value = mix64(value ^ (uint64_t)x * UINT64_C(0x9e3779b185ebca87));
    value = mix64(value ^ (uint64_t)y * UINT64_C(0xc2b2ae3d27d4eb4f));
    value = mix64(value ^ (uint64_t)z * UINT64_C(0x165667b19e3779f9));
    return value;
}

static double unit_from_hash(uint64_t value) {
    return (double)(value >> 11u) * (1.0 / 9007199254740992.0);
}

static double fade(double value) {
    return value * value * value *
           (value * (value * 6.0 - 15.0) + 10.0);
}

static double lerp(double a, double b, double t) {
    return a + ((b - a) * t);
}

bool procedural_surface_field_graph_noise_value(
    uint64_t seed,
    double x,
    double y,
    double z,
    double *out_value) {
    int64_t ix;
    int64_t iy;
    int64_t iz;
    double tx;
    double ty;
    double tz;
    double values[2][2][2];
    if (!out_value || !isfinite(x) || !isfinite(y) || !isfinite(z) ||
        x < (double)INT64_MIN + 2.0 || x > (double)INT64_MAX - 2.0 ||
        y < (double)INT64_MIN + 2.0 || y > (double)INT64_MAX - 2.0 ||
        z < (double)INT64_MIN + 2.0 || z > (double)INT64_MAX - 2.0) {
        return false;
    }
    ix = (int64_t)floor(x);
    iy = (int64_t)floor(y);
    iz = (int64_t)floor(z);
    tx = fade(x - (double)ix);
    ty = fade(y - (double)iy);
    tz = fade(z - (double)iz);
    for (int dz = 0; dz < 2; ++dz) {
        for (int dy = 0; dy < 2; ++dy) {
            for (int dx = 0; dx < 2; ++dx) {
                values[dz][dy][dx] =
                    (unit_from_hash(lattice_hash(
                         seed, ix + dx, iy + dy, iz + dz, 0u)) *
                     2.0) -
                    1.0;
            }
        }
    }
    const double x00 = lerp(values[0][0][0], values[0][0][1], tx);
    const double x10 = lerp(values[0][1][0], values[0][1][1], tx);
    const double x01 = lerp(values[1][0][0], values[1][0][1], tx);
    const double x11 = lerp(values[1][1][0], values[1][1][1], tx);
    *out_value = lerp(lerp(x00, x10, ty), lerp(x01, x11, ty), tz);
    return isfinite(*out_value);
}

bool procedural_surface_field_graph_noise_fbm(
    uint64_t seed,
    double x,
    double y,
    double z,
    double feature_size,
    uint32_t octaves,
    double lacunarity,
    double persistence,
    bool ridged,
    double *out_value) {
    double frequency;
    double amplitude = 1.0;
    double weighted_sum = 0.0;
    double weight_sum = 0.0;
    if (!out_value || !isfinite(feature_size) || !(feature_size > 0.0) ||
        octaves == 0u || octaves > 12u || !isfinite(lacunarity) ||
        lacunarity < 1.0 || lacunarity > 8.0 ||
        !isfinite(persistence) || persistence <= 0.0 || persistence > 1.0) {
        return false;
    }
    frequency = 1.0 / feature_size;
    for (uint32_t octave = 0u; octave < octaves; ++octave) {
        double value;
        if (!procedural_surface_field_graph_noise_value(
                seed + (uint64_t)octave * UINT64_C(0x9e3779b97f4a7c15),
                x * frequency, y * frequency, z * frequency, &value)) {
            return false;
        }
        if (ridged) value = 1.0 - fabs(value);
        weighted_sum += value * amplitude;
        weight_sum += amplitude;
        frequency *= lacunarity;
        amplitude *= persistence;
    }
    if (!(weight_sum > 0.0) || !isfinite(weighted_sum)) return false;
    *out_value = weighted_sum / weight_sum;
    return isfinite(*out_value);
}

bool procedural_surface_field_graph_noise_cellular_f1(
    uint64_t seed,
    double x,
    double y,
    double z,
    double feature_size,
    double *out_value) {
    int64_t ix;
    int64_t iy;
    int64_t iz;
    double px;
    double py;
    double pz;
    double minimum_distance2 = DBL_MAX;
    if (!out_value || !isfinite(feature_size) || !(feature_size > 0.0)) {
        return false;
    }
    px = x / feature_size;
    py = y / feature_size;
    pz = z / feature_size;
    if (!isfinite(px) || !isfinite(py) || !isfinite(pz) ||
        px < (double)INT64_MIN + 3.0 || px > (double)INT64_MAX - 3.0 ||
        py < (double)INT64_MIN + 3.0 || py > (double)INT64_MAX - 3.0 ||
        pz < (double)INT64_MIN + 3.0 || pz > (double)INT64_MAX - 3.0) {
        return false;
    }
    ix = (int64_t)floor(px);
    iy = (int64_t)floor(py);
    iz = (int64_t)floor(pz);
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int64_t cx = ix + dx;
                const int64_t cy = iy + dy;
                const int64_t cz = iz + dz;
                const double fx =
                    (double)cx +
                    unit_from_hash(lattice_hash(seed, cx, cy, cz, 1u));
                const double fy =
                    (double)cy +
                    unit_from_hash(lattice_hash(seed, cx, cy, cz, 2u));
                const double fz =
                    (double)cz +
                    unit_from_hash(lattice_hash(seed, cx, cy, cz, 3u));
                const double ddx = px - fx;
                const double ddy = py - fy;
                const double ddz = pz - fz;
                const double distance2 =
                    (ddx * ddx) + (ddy * ddy) + (ddz * ddz);
                minimum_distance2 = fmin(minimum_distance2, distance2);
            }
        }
    }
    *out_value = fmin(1.0, sqrt(minimum_distance2) / 1.15);
    return isfinite(*out_value);
}

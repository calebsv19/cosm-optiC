#include "render/runtime_material_texture_3d.h"

#include <math.h>
#include <string.h>

static double runtime_material_texture_3d_clamp(double value,
                                                double min_value,
                                                double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double runtime_material_texture_3d_clamp01(double value) {
    return runtime_material_texture_3d_clamp(value, 0.0, 1.0);
}

static int runtime_material_texture_3d_clamp_pattern_mode(int pattern_mode) {
    if (pattern_mode < RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_DEFAULT) {
        return RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_DEFAULT;
    }
    if (pattern_mode > RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_FLOW) {
        return RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_DEFAULT;
    }
    return pattern_mode;
}

static double runtime_material_texture_3d_fract(double value) {
    return value - floor(value);
}

static double runtime_material_texture_3d_lerp(double a, double b, double t) {
    return a + ((b - a) * t);
}

static double runtime_material_texture_3d_smooth(double t) {
    t = runtime_material_texture_3d_clamp01(t);
    return t * t * (3.0 - (2.0 * t));
}

static double runtime_material_texture_3d_hash(int ix, int iy, int seed) {
    unsigned int x = (unsigned int)ix;
    unsigned int y = (unsigned int)iy;
    unsigned int h = 2166136261u;
    h = (h ^ x) * 16777619u;
    h = (h ^ (y + 0x9e3779b9u)) * 16777619u;
    h = (h ^ (unsigned int)seed) * 16777619u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return (double)(h & 0x00FFFFFFu) / (double)0x01000000u;
}

static double runtime_material_texture_3d_value_noise(double u, double v, int seed) {
    int x0 = (int)floor(u);
    int y0 = (int)floor(v);
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    double tx = runtime_material_texture_3d_smooth(u - (double)x0);
    double ty = runtime_material_texture_3d_smooth(v - (double)y0);
    double n00 = runtime_material_texture_3d_hash(x0, y0, seed);
    double n10 = runtime_material_texture_3d_hash(x1, y0, seed);
    double n01 = runtime_material_texture_3d_hash(x0, y1, seed);
    double n11 = runtime_material_texture_3d_hash(x1, y1, seed);
    double nx0 = runtime_material_texture_3d_lerp(n00, n10, tx);
    double nx1 = runtime_material_texture_3d_lerp(n01, n11, tx);
    return runtime_material_texture_3d_lerp(nx0, nx1, ty);
}

static double runtime_material_texture_3d_fbm(double u, double v, int seed) {
    double sum = 0.0;
    double amp = 0.5;
    double norm = 0.0;
    double freq = 1.0;
    for (int octave = 0; octave < 4; ++octave) {
        sum += runtime_material_texture_3d_value_noise(u * freq, v * freq, seed + octave * 97) * amp;
        norm += amp;
        amp *= 0.5;
        freq *= 2.0;
    }
    if (norm <= 1e-9) return 0.0;
    return runtime_material_texture_3d_clamp01(sum / norm);
}

RuntimeMaterialTexture3DParams RuntimeMaterialTexture3DDefaultParams(void) {
    RuntimeMaterialTexture3DParams params;
    memset(&params, 0, sizeof(params));
    params.patternMode = RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_DEFAULT;
    params.coverage = 0.5;
    params.grain = 0.5;
    params.edgeSoftness = 0.5;
    params.contrast = 0.5;
    params.flow = 0.0;
    params.colorDepth = 0.5;
    params.surfaceDamage = 0.5;
    params.seed = 0;
    return params;
}

RuntimeMaterialTexture3DParams RuntimeMaterialTexture3DNormalizeParams(
    RuntimeMaterialTexture3DParams params) {
    if (params.patternMode == 0 &&
        params.coverage == 0.0 &&
        params.grain == 0.0 &&
        params.edgeSoftness == 0.0 &&
        params.contrast == 0.0 &&
        params.flow == 0.0 &&
        params.colorDepth == 0.0 &&
        params.surfaceDamage == 0.0 &&
        params.seed == 0) {
        return RuntimeMaterialTexture3DDefaultParams();
    }
    params.patternMode = runtime_material_texture_3d_clamp_pattern_mode(params.patternMode);
    params.coverage = runtime_material_texture_3d_clamp01(params.coverage);
    params.grain = runtime_material_texture_3d_clamp01(params.grain);
    params.edgeSoftness = runtime_material_texture_3d_clamp01(params.edgeSoftness);
    params.contrast = runtime_material_texture_3d_clamp01(params.contrast);
    params.flow = runtime_material_texture_3d_clamp01(params.flow);
    params.colorDepth = runtime_material_texture_3d_clamp01(params.colorDepth);
    params.surfaceDamage = runtime_material_texture_3d_clamp01(params.surfaceDamage);
    return params;
}

RuntimeMaterialTexture3DParams RuntimeMaterialTexture3DParamsFromObject(
    const SceneObject* object) {
    RuntimeMaterialTexture3DParams params = RuntimeMaterialTexture3DDefaultParams();
    if (!object) return params;
    if (object->texturePatternMode == 0 &&
        object->textureCoverage == 0.0 &&
        object->textureGrain == 0.0 &&
        object->textureEdgeSoftness == 0.0 &&
        object->textureContrast == 0.0 &&
        object->textureFlow == 0.0 &&
        object->textureColorDepth == 0.0 &&
        object->textureSurfaceDamage == 0.0 &&
        object->textureSeed == 0) {
        return params;
    }
    params.patternMode = object->texturePatternMode;
    params.coverage = object->textureCoverage;
    params.grain = object->textureGrain;
    params.edgeSoftness = object->textureEdgeSoftness;
    params.contrast = object->textureContrast;
    params.flow = object->textureFlow;
    params.colorDepth = object->textureColorDepth;
    params.surfaceDamage = object->textureSurfaceDamage;
    params.seed = object->textureSeed;
    return RuntimeMaterialTexture3DNormalizeParams(params);
}

static double runtime_material_texture_3d_apply_threshold(double value,
                                                         double coverage,
                                                         double edge_softness,
                                                         double contrast,
                                                         double strength) {
    double threshold = runtime_material_texture_3d_lerp(0.88, 0.18, coverage);
    double fade = runtime_material_texture_3d_lerp(0.035, 0.42, edge_softness);
    double mask = runtime_material_texture_3d_clamp01((value - threshold) / fade);
    double contrast_power = runtime_material_texture_3d_lerp(2.3, 0.55, contrast);
    mask = pow(mask, contrast_power);
    return runtime_material_texture_3d_clamp01(mask * strength);
}

static double runtime_material_texture_3d_sample_speckle(double u,
                                                        double v,
                                                        double frequency,
                                                        int seed) {
    double cell_u = u * frequency;
    double cell_v = v * frequency;
    int ix = (int)floor(cell_u);
    int iy = (int)floor(cell_v);
    double local_u = runtime_material_texture_3d_fract(cell_u);
    double local_v = runtime_material_texture_3d_fract(cell_v);
    double center_u = runtime_material_texture_3d_hash(ix, iy, seed + 31);
    double center_v = runtime_material_texture_3d_hash(ix, iy, seed + 67);
    double dx = local_u - center_u;
    double dy = local_v - center_v;
    double dist = sqrt((dx * dx) + (dy * dy));
    double dot = runtime_material_texture_3d_clamp01(1.0 - (dist * 3.8));
    double noise = runtime_material_texture_3d_value_noise(cell_u * 1.7, cell_v * 1.7, seed + 149);
    return runtime_material_texture_3d_clamp01((dot * 0.82) + (noise * 0.18));
}

static double runtime_material_texture_3d_sample_patch(double u,
                                                      double v,
                                                      double frequency,
                                                      int seed) {
    double low = runtime_material_texture_3d_fbm(u * frequency * 0.45,
                                                v * frequency * 0.45,
                                                seed + 211);
    double high = runtime_material_texture_3d_fbm(u * frequency * 1.4,
                                                 v * frequency * 1.4,
                                                 seed + 307);
    return runtime_material_texture_3d_clamp01((low * 0.72) + (high * 0.28));
}

static double runtime_material_texture_3d_sample_flow(double u,
                                                     double v,
                                                     double frequency,
                                                     double flow,
                                                     int seed) {
    double warp = runtime_material_texture_3d_fbm(u * frequency * 0.35,
                                                 v * frequency * 0.35,
                                                 seed + 401);
    double streak_u = (u * frequency * runtime_material_texture_3d_lerp(0.35, 1.2, flow)) +
                      ((warp - 0.5) * flow * 4.0);
    double streak_v = (v * frequency * runtime_material_texture_3d_lerp(1.2, 0.28, flow)) +
                      (flow * 1.7);
    double streak = runtime_material_texture_3d_fbm(streak_u, streak_v, seed + 503);
    double cloud = runtime_material_texture_3d_fbm(u * frequency * 0.6,
                                                  v * frequency * 0.6,
                                                  seed + 601);
    return runtime_material_texture_3d_clamp01((streak * 0.68) + (cloud * 0.32));
}

static double runtime_material_texture_3d_sample_rust_mask(double u,
                                                          double v,
                                                          double strength,
                                                          const RuntimeMaterialTexture3DParams* params,
                                                          int seed) {
    double grain = params ? params->grain : 0.5;
    double coverage = params ? params->coverage : 0.5;
    double edge_softness = params ? params->edgeSoftness : 0.5;
    double contrast = params ? params->contrast : 0.5;
    double flow = params ? params->flow : 0.0;
    int pattern_mode = params ? params->patternMode : RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_DEFAULT;
    double frequency = runtime_material_texture_3d_lerp(4.0, 28.0, grain);
    double value = 0.0;

    if (pattern_mode == RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_SPECKLE) {
        value = runtime_material_texture_3d_sample_speckle(u, v, frequency * 1.6, seed);
    } else if (pattern_mode == RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_PATCH) {
        value = runtime_material_texture_3d_sample_patch(u, v, frequency, seed);
    } else if (pattern_mode == RUNTIME_MATERIAL_TEXTURE_3D_PATTERN_FLOW) {
        value = runtime_material_texture_3d_sample_flow(u, v, frequency, flow, seed);
    } else {
        double fbm = runtime_material_texture_3d_fbm(u * frequency, v * frequency, seed);
        double patch = runtime_material_texture_3d_sample_patch(u, v, frequency, seed + 701);
        value = runtime_material_texture_3d_lerp(fbm, patch, flow * 0.45);
    }
    return runtime_material_texture_3d_apply_threshold(value,
                                                       coverage,
                                                       edge_softness,
                                                       contrast,
                                                       strength);
}

static double runtime_material_texture_3d_sample_fog_mask(double u,
                                                         double v,
                                                         double strength,
                                                         const RuntimeMaterialTexture3DParams* params,
                                                         int seed) {
    double grain = params ? params->grain : 0.5;
    double coverage = params ? params->coverage : 0.5;
    double flow = params ? params->flow : 0.0;
    double frequency = runtime_material_texture_3d_lerp(2.0, 14.0, grain);
    double noise = runtime_material_texture_3d_sample_flow(u,
                                                          v,
                                                          frequency,
                                                          flow,
                                                          seed + 809);
    double soft_noise = runtime_material_texture_3d_lerp(noise,
                                                        1.0 - fabs((noise * 2.0) - 1.0),
                                                        0.35);
    return runtime_material_texture_3d_clamp01(soft_noise *
                                               runtime_material_texture_3d_lerp(0.45, 1.25, coverage) *
                                               strength);
}

bool RuntimeMaterialTexture3D_Sample(const SceneObject* object,
                                     const HitInfo3D* hit,
                                     RuntimeMaterialTexture3DSample* out_sample) {
    if (!hit) {
        RuntimeMaterialTexture3DSample sample;
        if (!out_sample) return false;
        memset(&sample, 0, sizeof(sample));
        sample.kind = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
        *out_sample = sample;
        return false;
    }
    return RuntimeMaterialTexture3D_SampleUV(object,
                                             hit->triangleIndex,
                                             hit->baryV,
                                             hit->baryW,
                                             out_sample);
}

bool RuntimeMaterialTexture3D_SampleUV(const SceneObject* object,
                                       int triangle_index,
                                       double bary_v,
                                       double bary_w,
                                       RuntimeMaterialTexture3DSample* out_sample) {
    RuntimeMaterialTexture3DPlacement placement;
    memset(&placement, 0, sizeof(placement));
    if (object) {
        placement.textureId = object->textureId;
        placement.offsetU = object->textureOffsetU;
        placement.offsetV = object->textureOffsetV;
        placement.scale = object->textureScale;
        placement.strength = object->textureStrength;
        placement.params = RuntimeMaterialTexture3DParamsFromObject(object);
    }
    return RuntimeMaterialTexture3D_SamplePlacedUV(object,
                                                  bary_v,
                                                  bary_w,
                                                  triangle_index + 1,
                                                  &placement,
                                                  out_sample);
}

bool RuntimeMaterialTexture3D_SamplePlacedUV(const SceneObject* object,
                                             double u,
                                             double v,
                                             int seed_key,
                                             const RuntimeMaterialTexture3DPlacement* placement,
                                             RuntimeMaterialTexture3DSample* out_sample) {
    RuntimeMaterialTexture3DSample sample;
    double scale = 1.0;
    double strength = 0.0;
    double rotation = 0.0;
    double centered_u = 0.0;
    double centered_v = 0.0;
    double rotated_u = 0.0;
    double rotated_v = 0.0;
    double cos_r = 1.0;
    double sin_r = 0.0;
    int seed = 0;
    RuntimeMaterialTexture3DParams params = RuntimeMaterialTexture3DDefaultParams();

    if (!out_sample) return false;
    memset(&sample, 0, sizeof(sample));
    sample.kind = RUNTIME_MATERIAL_TEXTURE_3D_NONE;
    if (!object) {
        *out_sample = sample;
        return false;
    }

    sample.kind = (RuntimeMaterialTexture3DKind)(placement ? placement->textureId : object->textureId);
    if (sample.kind == RUNTIME_MATERIAL_TEXTURE_3D_NONE) {
        *out_sample = sample;
        return false;
    }

    strength = placement ? placement->strength : object->textureStrength;
    strength = runtime_material_texture_3d_clamp01(strength);
    if (strength <= 1e-9) {
        *out_sample = sample;
        return false;
    }

    scale = placement ? placement->scale : object->textureScale;
    if (!(scale > 1e-6)) {
        scale = 1.0;
    }
    rotation = placement ? placement->rotation : 0.0;
    cos_r = cos(rotation);
    sin_r = sin(rotation);
    centered_u = u - 0.5;
    centered_v = v - 0.5;
    rotated_u = (centered_u * cos_r) - (centered_v * sin_r) + 0.5;
    rotated_v = (centered_u * sin_r) + (centered_v * cos_r) + 0.5;

    sample.u = runtime_material_texture_3d_fract((rotated_u * scale) +
                                                 (placement ? placement->offsetU : object->textureOffsetU));
    sample.v = runtime_material_texture_3d_fract((rotated_v * scale) +
                                                 (placement ? placement->offsetV : object->textureOffsetV));
    seed = seed_key * 73856093;
    params = placement ? placement->params : RuntimeMaterialTexture3DParamsFromObject(object);
    params = RuntimeMaterialTexture3DNormalizeParams(params);
    if (params.seed != 0) {
        seed ^= params.seed * 83492791;
    }
    sample.colorDepth = params.colorDepth;
    sample.surfaceDamage = params.surfaceDamage;

    if (sample.kind == RUNTIME_MATERIAL_TEXTURE_3D_RUST) {
        sample.mask = runtime_material_texture_3d_sample_rust_mask(sample.u,
                                                                   sample.v,
                                                                   strength,
                                                                   &params,
                                                                   seed);
    } else if (sample.kind == RUNTIME_MATERIAL_TEXTURE_3D_FOG) {
        sample.mask = runtime_material_texture_3d_sample_fog_mask(sample.u,
                                                                  sample.v,
                                                                  strength,
                                                                  &params,
                                                                  seed);
    } else {
        *out_sample = sample;
        return false;
    }

    sample.active = sample.mask > 1e-9;
    *out_sample = sample;
    return sample.active;
}

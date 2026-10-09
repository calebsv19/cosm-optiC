#include "render/runtime_native_3d_denoise_guidance.h"
#include <math.h>

static bool has_material(const RuntimeNative3DFeatureBuffer* f, size_t i) {
    return f && f->albedoBuffer && f->shadingNormalBuffer &&
        f->materialGuideMaskBuffer && f->materialGuideMaskBuffer[i] &&
        (f->varianceOfMeanBuffer || f->borrowedRawM2Buffer) && f->sampleCountBuffer;
}

bool RuntimeNative3DDenoiseGuidance_HasMaterial(
    const RuntimeNative3DFeatureBuffer* f, size_t i) {
    f = RuntimeNative3DFeatureBuffer_GuideSource(f, i, &i);
    return has_material(f, i);
}

float RuntimeNative3DDenoiseGuidance_Blend(
    const RuntimeNative3DFeatureBuffer* f, size_t i, float luma) {
    /* Raw variance deliberately avoids hiding fireflies behind accumulation
     * clamping. It is an uncertainty proxy, not an exact variance of the clamped
     * estimator. Invalid / insufficient measurements conservatively pass through. */
    f = RuntimeNative3DFeatureBuffer_GuideSource(f, i, &i);
    if (!has_material(f, i) ||
        f->sampleCountBuffer[i] < 4u || !isfinite(luma)) return 0.0f;
    float variance = RuntimeNative3DFeatureBuffer_VarianceAt(f, i);
    if (!isfinite(variance) || variance < 0.0f) return 0.0f;
    float target = 0.002f + 0.01f * fabsf(luma);
    if (variance < 0.25f * target * target) return 0.0f;
    return fminf(0.90f, variance / (variance + target * target));
}

float RuntimeNative3DDenoiseGuidance_NeighborWeight(
    const RuntimeNative3DFeatureBuffer* f, size_t a, size_t b) {
    size_t local_a = 0, local_b = 0;
    const RuntimeNative3DFeatureBuffer* fa = RuntimeNative3DFeatureBuffer_GuideSource(f, a, &local_a);
    const RuntimeNative3DFeatureBuffer* fb = RuntimeNative3DFeatureBuffer_GuideSource(f, b, &local_b);
    if (!has_material(fa, local_a) || !has_material(fb, local_b)) return 0.0f;
    float color_delta = 0.0f, dot = 0.0f;
    for (size_t c = 0; c < 3u; ++c) {
        float ca = fa->albedoBuffer[local_a * 3u + c], cb = fb->albedoBuffer[local_b * 3u + c];
        float na = fa->shadingNormalBuffer[local_a * 3u + c];
        float nb = fb->shadingNormalBuffer[local_b * 3u + c];
        if (!isfinite(ca) || !isfinite(cb) || !isfinite(na) || !isfinite(nb)) return 0.0f;
        color_delta = fmaxf(color_delta, fabsf(ca - cb));
        dot += na * nb;
    }
    float rough_delta = f->roughnessBuffer[a] - f->roughnessBuffer[b];
    if (!isfinite(rough_delta) || dot < 0.0f) return 0.0f;
    /* Tight material gates preserve subtle grain and perturbed normals while
     * allowing noisy illumination to mix on compatible surface neighborhoods. */
    return expf(-color_delta * color_delta / (2.0f * 0.006f * 0.006f)) *
        expf(-rough_delta * rough_delta / (2.0f * 0.08f * 0.08f)) *
        powf(fminf(dot, 1.0f), 2048.0f);
}

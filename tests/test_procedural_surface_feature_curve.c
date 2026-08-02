#include "procedural/procedural_surface_feature_curve.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    ProceduralSurfaceFeatureCurveFieldV1 field = {0};
    ProceduralSurfaceFeatureCurveSampleV1 sample;
    ProceduralSurfaceFeatureVec3 shading;
    field.segment_count = 1u;
    field.normal_compatibility_cosine = .5;
    field.segments[0] = (ProceduralSurfaceFeatureCurveSegmentV1){
        .curve_id = 24u, .segment_id = 1u, .source_triangle = 7u,
        .barycentric_root = {.2, .3, .5}, .start = {-1, 0, 0}, .end = {1, 0, 0},
        .normal = {0, 0, 1}, .tangent = {1, 0, 0}, .width = .2,
        .depth = .08, .edge_softness = .2};
    assert(ProceduralSurfaceFeatureCurveFieldV1_Validate(&field));
    assert(ProceduralSurfaceFeatureCurveFieldV1_BuildIndex(&field));
    assert(ProceduralSurfaceFeatureCurveFieldV1_Sample(
        &field, (ProceduralSurfaceFeatureVec3){0,0,0},
        (ProceduralSurfaceFeatureVec3){0,0,1}, &sample));
    assert(sample.curve_id == 24u && sample.segment_id == 1u &&
           sample.signed_depth < 0.0 && sample.coverage > 0.0 &&
           sample.interior > 0.0 && sample.candidates_considered == 1u);
    assert(ProceduralSurfaceFeatureCurveSampleV1_ApplyShadingNormal(
        &sample, (ProceduralSurfaceFeatureVec3){0,0,1}, 1.0, &shading));
    assert(shading.y != 0.0 && shading.z > 0.0);
    assert(ProceduralSurfaceFeatureCurveFieldV1_Sample(
        &field, (ProceduralSurfaceFeatureVec3){0,0,0},
        (ProceduralSurfaceFeatureVec3){0,0,-1}, &sample));
    assert(sample.coverage == 0.0);
    puts("PSG-24B surface feature curve contract passed");
    return 0;
}

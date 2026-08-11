#include "procedural/procedural_surface_feature_curve.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    ProceduralSurfaceFeatureCurveFieldV1 field = {0};
    ProceduralSurfaceFeatureCurveSampleV1 sample;
    ProceduralSurfaceFeatureVec3 shading;
    char canonical[8192];
    char digest[PROCEDURAL_SURFACE_FEATURE_CURVE_DIGEST_CAPACITY];
    char loaded_digest[PROCEDURAL_SURFACE_FEATURE_CURVE_DIGEST_CAPACITY];
    ProceduralSurfaceFeatureCurveFieldV1 loaded = {0};
    const char *roundtrip_path = "/tmp/psg24b_curve_field_roundtrip.json";
    FILE *roundtrip;
    memset(field.source_mesh_digest_sha256, 'a', 64u);
    memset(field.authoring_digest_sha256, 'b', 64u);
    field.segment_count = 1u;
    field.normal_compatibility_cosine = .5;
    field.segments[0] = (ProceduralSurfaceFeatureCurveSegmentV1){
        .curve_id = 24u, .segment_id = 1u, .source_triangle = 7u,
        .barycentric_start = {.2, .3, .5},
        .barycentric_end = {.3, .2, .5},
        .start = {-1, 0, 0}, .end = {1, 0, 0},
        .normal_start = {0, 0, 1}, .normal_end = {0, 0, 1},
        .tangent = {1, 0, 0},
        .width_start = .2, .width_end = .1,
        .depth_start = .08, .depth_end = .04,
        .edge_softness = .2, .rim_width = .16};
    assert(ProceduralSurfaceFeatureCurveFieldV1_Validate(&field));
    assert(ProceduralSurfaceFeatureCurveFieldV1_CanonicalJson(
        &field, canonical, sizeof(canonical)));
    assert(strstr(canonical, "surface_feature_curve_field_v1"));
    assert(ProceduralSurfaceFeatureCurveFieldV1_Digest(&field, digest));
    assert(strlen(digest) == 64u);
    roundtrip = fopen(roundtrip_path, "wb");
    assert(roundtrip);
    assert(fwrite(canonical, 1u, strlen(canonical), roundtrip) ==
           strlen(canonical));
    assert(fclose(roundtrip) == 0);
    assert(ProceduralSurfaceFeatureCurveFieldV1_LoadJsonFile(
        roundtrip_path, &loaded));
    assert(ProceduralSurfaceFeatureCurveFieldV1_Digest(
        &loaded, loaded_digest));
    assert(strcmp(digest, loaded_digest) == 0);
    assert(remove(roundtrip_path) == 0);
    assert(ProceduralSurfaceFeatureCurveFieldV1_BuildIndex(&field));
    assert(ProceduralSurfaceFeatureCurveFieldV1_Sample(
        &field, (ProceduralSurfaceFeatureVec3){0,.04,0},
        (ProceduralSurfaceFeatureVec3){0,0,1}, &sample));
    assert(sample.curve_id == 24u && sample.segment_id == 1u &&
           sample.signed_depth < 0.0 && sample.coverage > 0.0 &&
           sample.interior > 0.0 && sample.candidates_considered == 1u);
    assert(ProceduralSurfaceFeatureCurveSampleV1_ApplyShadingNormal(
        &sample, (ProceduralSurfaceFeatureVec3){0,0,1}, 1.0, &shading));
    assert(sample.depth_slope != 0.0);
    assert(shading.y != 0.0 && shading.z > 0.0);
    assert(ProceduralSurfaceFeatureCurveFieldV1_Sample(
        &field, (ProceduralSurfaceFeatureVec3){0,0,0},
        (ProceduralSurfaceFeatureVec3){0,0,-1}, &sample));
    assert(sample.coverage == 0.0);
    puts("PSG-24B surface feature curve contract passed");
    return 0;
}

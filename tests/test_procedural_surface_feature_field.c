#include "procedural/procedural_surface_feature_field.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    ProceduralSurfaceFeatureFieldV1 f={0}; ProceduralSurfaceFeatureSampleV1 a,b; char first[65], second[65];
    f.feature_count=2; f.normal_compatibility_cosine=0.5; f.seed=24;
    memset(f.source_mesh_digest_sha256, 'a', 64u); f.source_mesh_digest_sha256[64] = '\0';
    memset(f.authoring_digest_sha256, 'b', 64u); f.authoring_digest_sha256[64] = '\0';
    for(size_t i=0;i<2;i++) { ProceduralSurfaceFeatureRootV1 *r=&f.features[i]; r->source_triangle=(uint32_t)i; r->barycentric[0]=1; r->normal.z=1; r->tangent.x=1; r->bitangent.y=1; r->radius=.5; r->aspect=1; r->edge_softness=.15; r->rim_width=.2; r->height_or_depth=i ? -.03 : .04; r->feature_id=(uint32_t)(100+i); }
    f.features[1].position.x=2.0;
    assert(ProceduralSurfaceFeatureFieldV1_Validate(&f));
    assert(ProceduralSurfaceFeatureFieldV1_BuildIndex(&f));
    assert(ProceduralSurfaceFeatureFieldV1_Digest(&f, first));
    assert(ProceduralSurfaceFeatureFieldV1_SaveJsonFileAtomic(
        "/private/tmp/psg24a_surface_feature_field_test.json", &f));
    { ProceduralSurfaceFeatureFieldV1 loaded = {0}; char loaded_digest[65];
      assert(ProceduralSurfaceFeatureFieldV1_LoadJsonFile(
          "/private/tmp/psg24a_surface_feature_field_test.json", &loaded));
      assert(ProceduralSurfaceFeatureFieldV1_Digest(&loaded, loaded_digest));
      assert(strcmp(first, loaded_digest) == 0); }
    assert(ProceduralSurfaceFeatureFieldV1_Sample(&f,(ProceduralSurfaceFeatureVec3){.1,0,0},(ProceduralSurfaceFeatureVec3){0,0,1},&a));
    assert(a.feature_id==100 && a.coverage>0 && a.rim>0 && a.interior>0 && a.rim!=a.interior && a.height_or_depth>0.0 && a.candidates_considered==1);
    assert(ProceduralSurfaceFeatureFieldV1_Sample(&f,(ProceduralSurfaceFeatureVec3){2.1,0,0},(ProceduralSurfaceFeatureVec3){0,0,1},&b));
    assert(b.feature_id==101 && b.height_or_depth<0.0);
    /* This point is several grid cells from the root.  A root-only index would
     * miss it, while the bounded footprint-cell index must retain it. */
    assert(ProceduralSurfaceFeatureFieldV1_Sample(&f,(ProceduralSurfaceFeatureVec3){.49,0,0},(ProceduralSurfaceFeatureVec3){0,0,1},&b));
    assert(b.feature_id == 100 && b.coverage > 0.0 &&
           b.candidates_considered <= PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_CELL_CAPACITY);
    assert(ProceduralSurfaceFeatureFieldV1_Sample(&f,(ProceduralSurfaceFeatureVec3){.1,0,0},(ProceduralSurfaceFeatureVec3){0,0,-1},&b));
    assert(b.coverage==0 && b.feature_id==0); /* opposing-fold contract */
    assert(ProceduralSurfaceFeatureFieldV1_Sample(&f,(ProceduralSurfaceFeatureVec3){.1,0,0},(ProceduralSurfaceFeatureVec3){0,0,1},&b));
    assert(memcmp(&a,&b,sizeof(a))==0); /* exact repeat */
    assert(ProceduralSurfaceFeatureFieldV1_Digest(&f, second) && strcmp(first, second)==0);
    puts("PSG-24A surface feature field contract passed"); return 0;
}

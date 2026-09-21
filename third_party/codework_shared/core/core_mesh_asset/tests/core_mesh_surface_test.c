#include "core_mesh_asset.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    CoreMeshAssetRuntimeDocument d;core_mesh_asset_runtime_document_init(&d);
    assert(core_mesh_asset_runtime_document_load_file("tests/fixtures/mesh_asset_runtime_v1_sample.json",&d).code==CORE_OK);
    assert(core_mesh_asset_surface_allocate(&d,"paint_uv").code==CORE_OK);
    for(size_t t=0;t<d.triangle_count;++t) {
        d.surface_corners[3*t+1].uv[0]=t%2?-1:1;
        d.surface_corners[3*t+2].uv[1]=1;
    }
    assert(core_mesh_asset_surface_generate_tangents(&d).code==CORE_OK);
    for(size_t t=0;t<d.triangle_count;++t) for(size_t k=0;k<3;++k) {
        assert(d.surface_corners[3*t+k].tangent_valid);
        assert(d.surface_corners[3*t+k].handedness==(t%2?-1:1));
    }
    assert(core_mesh_asset_runtime_document_save_file(&d,"build/surface-roundtrip.json").code==CORE_OK);
    CoreMeshAssetRuntimeDocument copy;core_mesh_asset_runtime_document_init(&copy);
    assert(core_mesh_asset_runtime_document_load_file("build/surface-roundtrip.json",&copy).code==CORE_OK);
    assert(!strcmp(copy.uv_set_id,"paint_uv") && copy.surface_corner_count==d.surface_corner_count);
    assert(!memcmp(copy.surface_corners,d.surface_corners,d.surface_corner_count*sizeof(*d.surface_corners)));
    copy.surface_corners[0].uv[0]=NAN;assert(core_mesh_asset_surface_validate(&copy).code!=CORE_OK);
    core_mesh_asset_runtime_document_free(&copy);
    for(size_t k=0;k<3;++k) d.surface_corners[k].uv[0]=d.surface_corners[k].uv[1]=0;
    assert(core_mesh_asset_surface_generate_tangents(&d).code==CORE_OK);
    for(size_t k=0;k<3;++k) assert(!d.surface_corners[k].tangent_valid && d.surface_corners[k].handedness==0);
    d.surface_corner_count--;assert(core_mesh_asset_surface_validate(&d).code!=CORE_OK);d.surface_corner_count++;
    core_mesh_asset_runtime_document_free(&d);assert(!d.surface_corners && !d.uv_set_id[0]);
    puts("surface UV identity, mirrored tangent, degenerate UV and JSON roundtrip passed");return 0;
}

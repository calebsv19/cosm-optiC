#include "core_mesh_compile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    const char *path="build/surface.obj";FILE *f=fopen(path,"wb");assert(f);
    fputs("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 2 0\nvt 1 1\nvt 2 1\nvn 0 0 1\nf 1/1/1 2/2/1 3/3/1\nf -4/4/1 -2/5/1 -1/6/1\n",f);assert(!fclose(f));
    CoreMeshAssetAuthoringDocument a;core_mesh_asset_authoring_document_init(&a);
    assert(core_mesh_asset_authoring_document_load_file("tests/fixtures/mesh_asset_authoring_v1_imported_stl_tetrahedron.json",&a).code==CORE_OK);
    a.imported_mesh_source.source_format=CORE_MESH_ASSET_IMPORTED_MESH_SOURCE_FORMAT_OBJ;
    snprintf(a.imported_mesh_source.source_uri,sizeof(a.imported_mesh_source.source_uri),"%s",path);
    snprintf(a.imported_mesh_source.uv_set_id,sizeof(a.imported_mesh_source.uv_set_id),"paint_uv");
    a.imported_mesh_source.source_to_asset_scale=1;a.imported_mesh_source.normal_mode=CORE_MESH_ASSET_IMPORTED_NORMAL_MODE_NONE;
    a.imported_mesh_source.preserve_source_normals=true;
    CoreMeshAssetRuntimeDocument d;core_mesh_asset_runtime_document_init(&d);
    CoreResult r=core_mesh_compile_imported_mesh_to_runtime_document(&a,".","uv_asset",&d);
    if(r.code!=CORE_OK) fprintf(stderr,"%s\n",r.message);assert(r.code==CORE_OK);
    assert(d.vertex_count==4 && d.triangle_count==2 && d.surface_corner_count==6 && !strcmp(d.uv_set_id,"paint_uv"));
    assert(d.surface_corners[0].uv[0]==0 && d.surface_corners[3].uv[0]==2);
    assert(d.surface_corners[0].handedness==1 && d.surface_corners[3].handedness==-1);
    assert(core_mesh_asset_runtime_document_save_file(&d,"build/obj-uv.runtime.json").code==CORE_OK);
    core_mesh_asset_runtime_document_free(&d);
    f=fopen(path,"ab");assert(f);fputs("f 1/1 2/2 3/3 4/4\n",f);fclose(f);
    assert(core_mesh_compile_imported_mesh_to_runtime_document(&a,".","uv_asset",&d).code!=CORE_OK);
    core_mesh_asset_authoring_document_free(&a);
    puts("OBJ corner indices, negative references, source normals, UV seams, mirrored charts and polygon rejection passed");return 0;
}

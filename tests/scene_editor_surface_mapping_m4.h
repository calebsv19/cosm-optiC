#include "editor/scene_editor_mesh_preview_store.h"
#include "import/runtime_mesh_asset_pack.h"

static double m4_compare(const RuntimeMaterialSurfaceEval *a,const RuntimeMaterialSurfaceEval *b) {
    const double av[]={a->colorR,a->colorG,a->colorB,a->roughness,a->reflectivity,a->specWeight,a->diffuseWeight,a->transparency};
    const double bv[]={b->colorR,b->colorG,b->colorB,b->roughness,b->reflectivity,b->specWeight,b->diffuseWeight,b->transparency};
    double error=0;for(int i=0;i<8;++i)error=fmax(error,fabs(av[i]-bv[i]));assert(error<1e-6);return error;
}
static void surface_mapping_m4_probe(SceneEditor* editor) {
    const RayTracingRuntimeMeshAssetSet *assets=ray_tracing_runtime_mesh_assets_last();
    assert(assets && assets->asset_count==1);
    const CoreMeshAssetRuntimeDocument *d=&assets->assets[0].document;
    assert(d->surface_corner_count==d->triangle_count*3 && !strcmp(d->uv_set_id,"paint_uv"));
    CoreMeshPreviewLodMesh lod;core_mesh_preview_lod_mesh_init(&lod);
    assert(core_mesh_preview_build_lod_mesh(d,1,&lod).code==CORE_OK);
    assert(lod.attribute_protected && lod.triangle_count==d->triangle_count);
    assert(lod.surface_corner_count==d->surface_corner_count && !memcmp(lod.surface_corners,d->surface_corners,d->surface_corner_count*sizeof(*d->surface_corners)));
    char diagnostic[512];CoreMeshAssetRuntimeDocument packed;core_mesh_asset_runtime_document_init(&packed);
    assert(ray_tracing_runtime_mesh_asset_pack_write_file("roundtrip.rtmpack",d,diagnostic,sizeof(diagnostic)));
    assert(ray_tracing_runtime_mesh_asset_pack_read_file("roundtrip.rtmpack",&packed,diagnostic,sizeof(diagnostic)));
    assert(!strcmp(packed.uv_set_id,d->uv_set_id) && packed.surface_corner_count==d->surface_corner_count);
    assert(!memcmp(packed.surface_corners,d->surface_corners,d->surface_corner_count*sizeof(*d->surface_corners)));
    core_mesh_asset_runtime_document_free(&packed);
    FILE *original=fopen("roundtrip.rtmpack","rb"),*bad=fopen("truncated.rtmpack","wb");assert(original && bad);
    assert(fseek(original,0,SEEK_END)==0);long bytes=ftell(original);rewind(original);
    for(long b=0;b<bytes-1;++b) assert(fputc(fgetc(original),bad)!=EOF);
    fclose(original);fclose(bad);core_mesh_asset_runtime_document_init(&packed);
    assert(!ray_tracing_runtime_mesh_asset_pack_read_file("truncated.rtmpack",&packed,diagnostic,sizeof(diagnostic)));
    core_mesh_asset_runtime_document_free(&packed);

    RuntimeScene3D scene;RuntimeScene3D_Init(&scene);assert(RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene));
    size_t samples=0,valid_frames=0,invalid_frames=0;double max_error=0;
    for(int t=0;t<scene.triangleMesh.triangleCount;++t) {
        const RuntimeTriangle3D *tri=&scene.triangleMesh.triangles[t];assert(tri->hasSurfaceUV);
        for(int i=1;i<15;++i) for(int j=1;j<15-i;++j) {
            double w[3]={i/15.0,j/15.0,1-(i+j)/15.0};
            Vec3 point=vec3_add(vec3_add(vec3_scale(tri->p0,w[0]),vec3_scale(tri->p1,w[1])),vec3_scale(tri->p2,w[2]));
            Vec3 n=vec3_normalize(vec3_cross(vec3_sub(tri->p1,tri->p0),vec3_sub(tri->p2,tri->p0)));
            Ray3D ray=RuntimeRay3D_Make(vec3_add(point,vec3_scale(n,2)),vec3_scale(n,-1));HitInfo3D hit;
            assert(RuntimeRay3D_TraceSceneFirstHit(&scene,&ray,1e-5,4,&hit));assert(hit.hasSurfaceUV);
            assert(hit.localTriangleIndex==tri->localTriangleIndex);
            double uv[2]={0};for(int k=0;k<3;++k) for(int a=0;a<2;++a) uv[a]+=w[k]*d->surface_corners[tri->localTriangleIndex*3+k].uv[a];
            assert(fabs(hit.surfaceUV[0]-uv[0])<1e-9 && fabs(hit.surfaceUV[1]-uv[1])<1e-9);
            if(hit.hasSurfaceTangent) {
                valid_frames++;assert(fabs(vec3_dot(hit.surfaceTangent,hit.shadingNormal))<1e-9);
                assert(fabs(vec3_length(hit.surfaceTangent)-1)<1e-9 && fabs(hit.surfaceHandedness)==1);
                /* Independent differential oracle: T aligns with increasing U. */
                const CoreMeshAssetSurfaceCorner *c=tri->surfaceCorners;
                double du1=c[1].uv[0]-c[0].uv[0],dv1=c[1].uv[1]-c[0].uv[1],du2=c[2].uv[0]-c[0].uv[0],dv2=c[2].uv[1]-c[0].uv[1];
                double det=du1*dv2-du2*dv1;
                Vec3 tu=vec3_scale(vec3_sub(vec3_scale(vec3_sub(tri->p1,tri->p0),dv2),vec3_scale(vec3_sub(tri->p2,tri->p0),dv1)),1/det);
                Vec3 bv=vec3_scale(vec3_sub(vec3_scale(vec3_sub(tri->p2,tri->p0),du1),vec3_scale(vec3_sub(tri->p1,tri->p0),du2)),1/det);
                assert(vec3_dot(vec3_normalize(tu),hit.surfaceTangent)>1-1e-9);
                assert(vec3_dot(vec3_scale(vec3_cross(hit.shadingNormal,hit.surfaceTangent),hit.surfaceHandedness),bv)>0);
            } else {invalid_frames++;assert(!tri->surfaceCorners[0].tangent_valid);}
            RuntimeMaterialPayload3D payload;assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit,&payload));
            RuntimeMaterialSurfaceEval expected=RuntimeMaterialSurfaceEvalMakeBase(payload.baseColorR,payload.baseColorG,payload.baseColorB,payload.bsdf.roughness,payload.bsdf.reflectivity,payload.bsdf.specWeight,payload.bsdf.diffuseWeight,payload.transparency),preview;
            assert(RuntimeSurfaceMaterialSampleMesh(0,0,(size_t)tri->localTriangleIndex,w,point,hit.shadingNormal,&lod,&preview));max_error=fmax(max_error,m4_compare(&preview,&expected));samples++;
        }
    }
    assert(samples>100 && valid_frames>0);
    for(int quality=0;quality<2;++quality) {
        const CoreMeshPreviewLodMesh *prepared=SceneEditorMeshPreviewStoreGetForQuality(0,quality!=0);
        assert(prepared && prepared->surface_corner_count==d->surface_corner_count && prepared->attribute_protected);
    }
    FILE *report=fopen("mapping_m4.json","w");assert(report);
    fprintf(report,"{\"samples\":%zu,\"valid_frames\":%zu,\"invalid_frames\":%zu,\"uv_set\":\"paint_uv\",\"lod_attribute_protected\":true,\"packed_roundtrip\":true,\"max_material_error\":%.17g}\n",samples,valid_frames,invalid_frames,max_error);fclose(report);
    ObjectEditorSetSelectedObjectIndex(0);assert(SceneEditorFrameViewport(true));choose_menu(editor,3,SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);
    m2_control(editor,"expand");m2_edit(editor,"offset_u","0.23");
    CoreAuthoredSurfaceMapping changed;assert(RuntimeSurfaceMappingDefinition(0,&changed) && fabs(changed.uv_offset[0]-.23)<1e-12);
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));assert(SceneEditorDocumentRedo(diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    choose_menu(editor,3,SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    capture(editor,"m4-uv-material.ppm");
    assert(SceneEditorRuntimeScenePersistAuthoring(diagnostic,sizeof(diagnostic)));
    core_mesh_preview_lod_mesh_free(&lod);RuntimeScene3D_Free(&scene);
}
/* Adapter fixture: the normal loader path is covered by solid-graph tests.
 * Install a real compiled geometry-field program in this process-owned asset,
 * then compare exact-LOD viewport sampling against actual ray intersections. */
static void surface_mapping_m4_graph_probe(SceneEditor *editor,const char *scene_path) {
    RayTracingRuntimeMeshAssetSet *assets=(RayTracingRuntimeMeshAssetSet *)ray_tracing_runtime_mesh_assets_last();
    assert(assets && assets->asset_count==1);
    RayTracingRuntimeMeshAsset *asset=&assets->assets[0];
    ProceduralSolidMaterialGraphReport report={0};ProceduralSolidAuthoredMaterialV1 materials[2];
    assert(ProceduralSolidMaterialGraphV1_FromTemplate("snow_accumulation","m4-geometry","m4-binding",
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
        &asset->procedural_solid_material_graph,&report));
    assert(ProceduralSolidAuthoredMaterialV1_FromTemplate("weathered_rock","base_material",&materials[0],NULL));
    assert(ProceduralSolidAuthoredMaterialV1_FromTemplate("snow","snow_material",&materials[1],NULL));
    snprintf(asset->procedural_solid_material_graph.surface_mapping_ref,sizeof(asset->procedural_solid_material_graph.surface_mapping_ref),"m4-chart");
    for(int m=0;m<2;++m) {materials[m].surface.texture.enabled=true;
        snprintf(materials[m].surface.texture.kind,sizeof(materials[m].surface.texture.kind),"brick");
        materials[m].surface.texture.microdetail_normal_strength=0;}
    const char *kinds[]={"retained","retained"};assert(asset->document.triangle_count==2);
    assert(ProceduralSolidMaterialRuntimeProgramV1_Build(&asset->procedural_solid_material_graph,materials,2,
        &asset->document,kinds,&asset->procedural_solid_material_runtime_program,&report));
    asset->procedural_solid_authored_material_valid=true;asset->procedural_solid_material_graph_valid=true;
    asset->procedural_solid_composed_triangle_material_count=2;
    asset->procedural_solid_composed_triangle_materials=calloc(2,sizeof(*asset->procedural_solid_composed_triangle_materials));
    assert(asset->procedural_solid_composed_triangle_materials);
    for(int t=0;t<2;++t)asset->procedural_solid_composed_triangle_materials[t]=materials[0].surface;
    json_object *root=json_object_from_file(scene_path);assert(root);
    json_object *objects=NULL;assert(json_object_object_get_ex(root,"objects",&objects));
    json_object *object=json_object_array_get_idx(objects,0),*reference=json_object_new_object();
    json_object_object_add(reference,"graph_path",json_object_new_string("compiled-test-fixture"));
    json_object_object_add(object,"procedural_solid_material_ref",reference);
    json_object *ext=json_object_object_get(root,"extensions"),*ray_ext=json_object_object_get(ext,"ray_tracing");
    json_object *row=json_object_array_get_idx(json_object_object_get(json_object_object_get(ray_ext,"authoring"),"object_materials"),0);
    json_object *maps=json_object_new_array(),*entry=json_object_new_object(),*binding=json_object_new_object();
    json_object_object_add(entry,"id",json_object_new_string("m4-chart"));
    json_object *mapping=json_object_object_get(json_object_object_get(json_object_object_get(object,"extensions"),"ray_tracing"),"surface_mapping");
    json_object_object_add(entry,"definition",json_object_get(mapping));json_object_array_add(maps,entry);
    json_object_object_add(binding,"mappings",maps);json_object_object_add(row,"surface_material_binding",binding);
    RuntimeSurfaceMappingLoadScene(root,json_object_get_double(json_object_object_get(root,"world_scale")));json_object_put(root);
    assert(RuntimeSurfaceMappingPreviewSupported(0));SceneEditorMeshPreviewStorePrepare(assets);
    const CoreMeshPreviewLodMesh *lod=SceneEditorMeshPreviewStoreGetForQuality(0,false);
    assert(lod && lod->attribute_protected && lod->triangle_count==2);
    RuntimeScene3D scene;RuntimeScene3D_Init(&scene);assert(RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene));
    size_t samples=0;double max_error=0,min_color=1e9,max_color=-1e9;
    for(int t=0;t<scene.triangleMesh.triangleCount;++t) {
        const RuntimeTriangle3D *tri=&scene.triangleMesh.triangles[t];assert(tri->proceduralSolidMaterialRuntimeProgram);
        for(int i=1;i<15;++i)for(int j=1;j<15-i;++j) {
            double w[]={i/15.0,j/15.0,1-(i+j)/15.0};
            Vec3 point=vec3_add(vec3_add(vec3_scale(tri->p0,w[0]),vec3_scale(tri->p1,w[1])),vec3_scale(tri->p2,w[2]));
            Vec3 n=vec3_normalize(vec3_cross(vec3_sub(tri->p1,tri->p0),vec3_sub(tri->p2,tri->p0)));
            Ray3D ray=RuntimeRay3D_Make(vec3_add(point,vec3_scale(n,2)),vec3_scale(n,-1));HitInfo3D hit;
            assert(RuntimeRay3D_TraceSceneFirstHit(&scene,&ray,1e-5,4,&hit));
            RuntimeMaterialPayload3D payload;assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit,&payload));
            RuntimeMaterialSurfaceEval expected=RuntimeMaterialSurfaceEvalMakeBase(payload.baseColorR,payload.baseColorG,payload.baseColorB,payload.bsdf.roughness,payload.bsdf.reflectivity,payload.bsdf.specWeight,payload.bsdf.diffuseWeight,payload.transparency),preview;
            assert(RuntimeSurfaceMaterialSampleMesh(0,0,(size_t)tri->localTriangleIndex,w,point,hit.shadingNormal,lod,&preview));
            max_error=fmax(max_error,m4_compare(&preview,&expected));min_color=fmin(min_color,preview.colorR);max_color=fmax(max_color,preview.colorR);samples++;
        }
    }
    assert(samples==182 && max_color-min_color>.01);
    FILE *f=fopen("mapping_m4_graph.json","w");assert(f);fprintf(f,"{\"samples\":%zu,\"color_range\":%.9g,\"max_material_error\":%.17g,\"exact_lod\":true}\n",samples,max_color-min_color,max_error);fclose(f);
    choose_menu(editor,3,SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    capture(editor,"m4-geometry-field-material.ppm");RuntimeScene3D_Free(&scene);
}

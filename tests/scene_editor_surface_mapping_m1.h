#include "render/runtime_surface_mapping.h"
#include "editor/scene_editor_viewport_material.h"

static const char* m1_mapping_json =
    "{\"version\":1,\"required_capability\":\"optic.planar_surface_v1\","
    "\"method\":\"planar\",\"space\":\"object_rest\",\"source_domain\":\"brick_cells_v1\","
    "\"scale_policy\":\"stretch_with_object\",\"origin_m\":[-2,-1,0],"
    "\"axis_u\":[1,0,0],\"axis_v\":[0,1,0],\"tile_m\":[0.5,0.5],"
    "\"offset_m\":[0,0],\"pivot_m\":[0,0],\"rotation_rad\":0,\"seed\":1729,"
    "\"producer_note\":{\"keep\":\"editable-source\"}}";

static void m1_same(const RuntimeMaterialSurfaceEval* a,const RuntimeMaterialSurfaceEval* b) {
    const double av[]={a->colorR,a->colorG,a->colorB,a->roughness,a->reflectivity,a->specWeight,a->diffuseWeight,a->transparency};
    const double bv[]={b->colorR,b->colorG,b->colorB,b->roughness,b->reflectivity,b->specWeight,b->diffuseWeight,b->transparency};
    for(int i=0;i<8;++i) assert(fabs(av[i]-bv[i])<1e-6);
}
static Vec3 m1_world_point(const RuntimeSceneBridgePrimitiveSeed* p,double x,double y) {
    return vec3(p->origin_x+p->axis_u_x*x*p->width/4+p->axis_v_x*y*p->height/2,
                p->origin_y+p->axis_u_y*x*p->width/4+p->axis_v_y*y*p->height/2,
                p->origin_z+p->axis_u_z*x*p->width/4+p->axis_v_z*y*p->height/2);
}
static void m1_points(void) {
    RuntimeSceneBridge3DPrimitiveSeedState seeds={0};runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    const RuntimeSceneBridgePrimitiveSeed* p=&seeds.primitives[0];
    RuntimeScene3D scene;RuntimeScene3D_Init(&scene);assert(RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene));
    Vec3 n=vec3(p->normal_x,p->normal_y,p->normal_z);
    for(int j=0;j<9;++j) for(int i=0;i<17;++i) {
        double x=-1.97+i*.241,y=-.97+j*.239;
        Vec3 point=m1_world_point(p,x,y);
        Ray3D ray=RuntimeRay3D_Make(vec3_add(point,vec3_scale(n,10)),vec3_scale(n,-1));
        HitInfo3D hit;assert(RuntimeRay3D_TraceSceneFirstHit(&scene,&ray,.0001,20,&hit));
        CoreAuthoredSurfaceCoordinates q;assert(RuntimeSurfaceMappingCoordinates(&hit,&q));
        assert(fabs(q.uv_tiles[0]-(x+2)*2)<1e-9 && fabs(q.uv_tiles[1]-(y+1)*2)<1e-9);
        RuntimeMaterialSurfaceEval vp;
        assert(RuntimeSurfaceMaterialSamplePrimitive(0,0,(x+2)/4,(y+1)/2,&vp));
        RuntimeMaterialPayload3D payload;assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit,&payload));
        RuntimeMaterialSurfaceEval actual=RuntimeMaterialSurfaceEvalMakeBase(payload.baseColorR,payload.baseColorG,payload.baseColorB,
            payload.bsdf.roughness,payload.bsdf.reflectivity,payload.bsdf.specWeight,payload.bsdf.diffuseWeight,payload.transparency);
        m1_same(&vp,&actual);
        hit.triangleIndex+=113;hit.localTriangleIndex=91;hit.baryU=.4;hit.baryV=.2;hit.baryW=.4;
        assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit,&payload));
        assert(fabs(payload.baseColorR-actual.colorR)<1e-9);
    }
    /* Actual intersection on alternate diagonals and exact subdivisions of the
     * same plane; new mapping consumes positions, not barycentrics or indices. */
    for(int variant=0;variant<3;++variant) {
        int nx=variant==2?8:1,ny=variant==2?4:1;
        for(int y=0;y<ny;++y) for(int x=0;x<nx;++x) {
            Vec3 corners[4]={m1_world_point(p,-2+4.*x/nx,-1+2.*y/ny),m1_world_point(p,-2+4.*(x+1)/nx,-1+2.*y/ny),
                m1_world_point(p,-2+4.*(x+1)/nx,-1+2.*(y+1)/ny),m1_world_point(p,-2+4.*x/nx,-1+2.*(y+1)/ny)};
            int indices[2][3]={{0,1,2},{0,2,3}};
            if(variant==1) {int alt[2][3]={{0,1,3},{1,2,3}};memcpy(indices,alt,sizeof(indices));}
            for(int t=0;t<2;++t) {
                RuntimeTriangle3D triangle={0};triangle.p0=corners[indices[t][0]];triangle.p1=corners[indices[t][1]];triangle.p2=corners[indices[t][2]];
                triangle.normal=n;triangle.sceneObjectIndex=0;triangle.twoSided=true;
                Vec3 point=vec3_scale(vec3_add(vec3_add(triangle.p0,triangle.p1),triangle.p2),1./3);
                Ray3D ray=RuntimeRay3D_Make(vec3_add(point,n),vec3_scale(n,-1));HitInfo3D hit;
                assert(RuntimeRay3D_IntersectTriangle(&ray,&triangle,71,.0001,2,&hit));
                CoreAuthoredSurfaceCoordinates q;assert(RuntimeSurfaceMappingCoordinates(&hit,&q));
                Vec3 d=vec3_sub(point,vec3(p->origin_x,p->origin_y,p->origin_z));
                double localx=vec3_dot(d,vec3(p->axis_u_x,p->axis_u_y,p->axis_u_z))*4/p->width;
                double localy=vec3_dot(d,vec3(p->axis_v_x,p->axis_v_y,p->axis_v_z))*2/p->height;
                assert(fabs(q.uv_tiles[0]-(localx+2)*2)<1e-9 && fabs(q.uv_tiles[1]-(localy+1)*2)<1e-9);
            }
        }
    }
    RuntimeScene3D_Free(&scene);
}
static void surface_mapping_m1_probe(SceneEditor* editor,bool reopen) {
    char diagnostic[512];
    if(reopen) {assert(RuntimeSurfaceMappingActive(0));m1_points();return;}
    assert(!RuntimeSurfaceMappingActive(0));
    /* In-memory legacy face edits invalidate without a retained-document edit. */
    assert(SceneEditorViewportMaterialPrepareFace(0,0));
    unsigned long long legacy_builds=SceneEditorViewportMaterialBuildCount();
    unsigned long long face_revision=SceneEditorMaterialFacePlacementRevision();
    SceneEditorMaterialFacePlacement placement=SceneEditorMaterialFacePlacementGetEffectiveForLayer(
        &sceneSettings.sceneObjects[0],0,0,"base");
    snprintf(placement.layerId,sizeof(placement.layerId),"base");
    placement.layerIndex=0;placement.scale=2.3;
    assert(SceneEditorMaterialFacePlacementSetOverride(&placement));
    assert(SceneEditorMaterialFacePlacementRevision()>face_revision);
    assert(SceneEditorViewportMaterialPrepareFace(0,0));
    assert(SceneEditorViewportMaterialBuildCount()>legacy_builds);
    assert(SceneEditorMaterialFacePlacementResetFaceLayer(0,0,"base"));
    assert(SceneEditorDocumentSetSurfaceMappingForSceneIndex(0,m1_mapping_json,diagnostic,sizeof(diagnostic)));
    m1_points();
    RuntimeMaterialSurfaceEval original,duplicated;
    assert(RuntimeSurfaceMaterialSamplePrimitive(0,0,.37,.61,&original));
    SceneEditorViewportMaterialReset();assert(SceneEditorViewportMaterialPrepareFace(0,0));
    unsigned long long builds=SceneEditorViewportMaterialBuildCount();
    for(int workspace=0;workspace<5;++workspace) {
        SceneEditorWorkspaceProfileSelect(editor,workspace);
        assert(SceneEditorViewportMaterialPrepareFace(0,0));
    }
    assert(SceneEditorViewportMaterialBuildCount()==builds);
    unsigned long long revision=SceneEditorDocumentRevision();
    assert(!SceneEditorDocumentSetSurfaceMappingForSceneIndex(0,"{\"version\":99}",diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentRevision()==revision && RuntimeSurfaceMappingActive(0));
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));assert(!RuntimeSurfaceMappingActive(0));
    assert(SceneEditorDocumentRedo(diagnostic,sizeof(diagnostic)));assert(RuntimeSurfaceMappingActive(0));
    assert(SceneEditorViewportMaterialPrepareFace(0,0));assert(SceneEditorViewportMaterialBuildCount()>builds);
    SceneEditorDocumentTransform transform;
    assert(SceneEditorDocumentGetTransformForSceneIndex(0,&transform,diagnostic,sizeof(diagnostic)));
    transform.position[0]+=1.3;transform.position[1]-=.7;
    transform.rotation_degrees[0]=17;transform.rotation_degrees[1]=31;transform.rotation_degrees[2]=73;
    transform.scale[0]=1.7;transform.scale[1]=.8;
    assert(SceneEditorDocumentSetTransformForSceneIndex(0,&transform,diagnostic,sizeof(diagnostic)));m1_points();
    assert(RuntimeSurfaceMaterialSamplePrimitive(0,0,.37,.61,&duplicated));m1_same(&original,&duplicated);
    int new_index=-1;
    assert(SceneEditorDocumentDuplicateForSceneIndex(0,&new_index,diagnostic,sizeof(diagnostic)));
    assert(RuntimeSurfaceMaterialSamplePrimitive(new_index,0,.37,.61,&duplicated));m1_same(&original,&duplicated);
    assert(SceneEditorDocumentRemoveForSceneIndex(0,diagnostic,sizeof(diagnostic)));
    assert(RuntimeSurfaceMaterialSamplePrimitive(0,0,.37,.61,&duplicated));m1_same(&original,&duplicated);
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    json_object* world=json_tokener_parse(m1_mapping_json);
    json_object_object_add(world,"space",json_object_new_string("world"));
    json_object_object_add(world,"rotation_rad",json_object_new_double(0.37));
    assert(SceneEditorDocumentSetSurfaceMappingForSceneIndex(0,json_object_to_json_string(world),diagnostic,sizeof(diagnostic)));
    HitInfo3D world_hit;HitInfo3D_Reset(&world_hit);world_hit.sceneObjectIndex=0;
    double world_scale=SceneEditorDocumentWorldScale();
    world_hit.position=vec3(1.2*world_scale,-.4*world_scale,.7*world_scale);
    CoreAuthoredSurfaceCoordinates coordinates;assert(RuntimeSurfaceMappingCoordinates(&world_hit,&coordinates));
    assert(fabs(coordinates.uv_tiles[0]-(cos(.37)*3.2-sin(.37)*.6)*2)<1e-9);
    assert(fabs(coordinates.uv_tiles[1]-(sin(.37)*3.2+cos(.37)*.6)*2)<1e-9);
    /* World coordinates follow the point, independently of the object frame. */
    CoreAuthoredSurfaceCoordinates moved;
    world_hit.position.x+=world_scale;
    assert(RuntimeSurfaceMappingCoordinates(&world_hit,&moved));
    assert(fabs(moved.uv_tiles[0]-coordinates.uv_tiles[0]-2*cos(.37))<1e-9);
    assert(fabs(moved.uv_tiles[1]-coordinates.uv_tiles[1]-2*sin(.37))<1e-9);
    RuntimeMaterialPayload3D invalid_payload;
    world_hit.position.x=1e20;
    assert(!RuntimeMaterialPayload3D_ResolveFromHit(&world_hit,&invalid_payload));
    assert(!invalid_payload.valid);
    json_object_put(world);
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentSave(diagnostic,sizeof(diagnostic)));
    FILE* f=fopen("mapping_m1.json","w");assert(f);
    fprintf(f,"{\"coordinates_and_channels\":true,\"subdivision\":true,\"transform\":true,\"duplicate\":true,\"undo_redo\":true,\"workspace_cache_rebuilds\":0}\n");fclose(f);
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);ObjectEditorSetSelectedObjectIndex(0);assert(SceneEditorFrameViewport(true));
    choose_menu(editor,3,SCENE_EDITOR_MESH_DISPLAY_MATERIAL);ObjectEditorSetSelectedObjectIndex(-1);capture(editor,"mapping_m1.ppm");
}

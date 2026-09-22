#include "editor/scene_editor_surface_mapping_panel.h"
#include "editor/scene_editor_surface_mapping_cache.h"

static RuntimeMaterialSurfaceEval m2_base(int index) {
    RuntimeMaterialPayload3D p;assert(RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(index,&p));
    return RuntimeMaterialSurfaceEvalMakeBase(p.baseColorR,p.baseColorG,p.baseColorB,p.bsdf.roughness,p.bsdf.reflectivity,p.bsdf.specWeight,p.bsdf.diffuseWeight,p.transparency);
}
static void m2_control(SceneEditor* editor,const char* name) {
    SDL_Rect rect;SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorSurfaceMappingPanelControl(name,&rect));
    assert(rect.y+rect.h<=800);click(editor,rect);
}
static void m2_edit_attempt(SceneEditor* editor,const char* name,const char* value,bool accepted) {
    m2_control(editor,name);assert(SceneEditorSurfaceMappingPanelActive());
    SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_a;e.key.keysym.mod=KMOD_GUI;
    SceneEditorSessionRuntimeHandleEvent(editor,&e);
    e=(SDL_Event){0};e.type=SDL_TEXTINPUT;snprintf(e.text.text,sizeof(e.text.text),"%s",value);SceneEditorSessionRuntimeHandleEvent(editor,&e);
    key(editor,SDLK_RETURN);assert(SceneEditorSurfaceMappingPanelActive()==!accepted);
    if(!accepted){key(editor,SDLK_ESCAPE);assert(!SceneEditorSurfaceMappingPanelActive());}
}
static void m2_edit(SceneEditor* editor,const char* name,const char* value) {
    m2_edit_attempt(editor,name,value,true);
}
static void surface_mapping_m2_probe(SceneEditor* editor,bool reopen) {
    CoreAuthoredSurfaceMapping m;assert(RuntimeSurfaceMappingDefinition(0,&m) && m.version==2);
    RuntimeMaterialSurfaceEval base=m2_base(0),left,right;
    double tau=6.2831853071795864769,scale=SceneEditorDocumentWorldScale();
    RuntimeScene3D scene;RuntimeScene3D_Init(&scene);assert(RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene));
    const SceneEditorSurfaceMappingCache* cache=SceneEditorSurfaceMappingCachePrepare(0);assert(cache);
    FILE* samples=fopen("mapping_m2_samples.csv","w");assert(samples);
    double error=0;int count=0;
    for(int j=0;j<7;++j) for(int i=0;i<24;++i) {
        double angle=(i+.271)*tau/24,z=-.79+j*.26;
        Vec3 n=vec3(cos(angle),sin(angle),0);
        Ray3D ray=RuntimeRay3D_Make(vec3(3*scale*n.x,3*scale*n.y,z*scale),vec3_scale(n,-1));
        HitInfo3D hit;assert(RuntimeRay3D_TraceSceneFirstHit(&scene,&ray,.0001,6*scale,&hit));assert(hit.sceneObjectIndex==0);
        CoreAuthoredSurfaceCoordinates q;assert(RuntimeSurfaceMappingCoordinates(&hit,&q));
        double turns=(atan2(hit.position.y,hit.position.x)-m.seam_rad)/tau+m.offset_m[0]/(tau*m.reference_radius_m);
        assert(fabs(q.uv_tiles[0]-(turns-floor(turns))*round(tau*m.reference_radius_m/m.tile_m[0]))<1e-9);
        assert(fabs(q.uv_tiles[1]-(hit.position.z/scale+m.offset_m[1])/m.tile_m[1])<1e-9);
        RuntimeMaterialSurfaceEval expected;
        assert(RuntimeSurfaceMappingEvaluateTiles(0,q.uv_tiles[0],q.uv_tiles[1],&base,&expected));RuntimeSurfaceMappingBlendPole(&base,q.source_weight,&expected);
        RuntimeMaterialPayload3D p;assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit,&p));
        RuntimeMaterialSurfaceEval actual=RuntimeMaterialSurfaceEvalMakeBase(p.baseColorR,p.baseColorG,p.baseColorB,p.bsdf.roughness,p.bsdf.reflectivity,p.bsdf.specWeight,p.bsdf.diffuseWeight,p.transparency);
        m1_same(&actual,&expected);
        fprintf(samples,"%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g\n",
            hit.position.x/scale,hit.position.y/scale,hit.position.z/scale,actual.colorR,actual.colorG,actual.colorB,
            actual.roughness,actual.reflectivity,actual.specWeight,actual.diffuseWeight,actual.transparency);
        Vec3 rest=vec3_scale(hit.position,1/scale);RuntimeMaterialSurfaceEval cached;
        assert(SceneEditorSurfaceMappingCacheSample(cache,hit.position,rest,hit.position,rest,hit.position,rest,&cached));
        error+=fabs(cached.colorR-actual.colorR)+fabs(cached.colorG-actual.colorG)+fabs(cached.colorB-actual.colorB);count+=3;
        hit.localTriangleIndex+=171;hit.triangleIndex+=243;hit.baryU=.4;hit.baryV=.5;hit.baryW=.1;
        assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit,&p));assert(fabs(p.baseColorR-actual.colorR)<1e-12);
    }
    /* A minified moving strip must vary less than unfiltered point samples. */
    double filtered_variation=0,point_variation=0;RuntimeMaterialSurfaceEval last_filtered={0},last_point={0};
    for(int i=0;i<96;++i) {
        double angle=.19+i*.012;Vec3 p=vec3(cos(angle),sin(angle),-.4+i*.007);
        Vec3 px=vec3_add(p,vec3(-.4*sin(angle),.4*cos(angle),0)),py=vec3_add(p,vec3(0,0,.4));
        RuntimeMaterialSurfaceEval point_sample,filtered;
        assert(SceneEditorSurfaceMappingCacheSample(cache,vec3_scale(p,scale),p,vec3_scale(p,scale),p,vec3_scale(p,scale),p,&point_sample));
        assert(SceneEditorSurfaceMappingCacheSample(cache,vec3_scale(p,scale),p,vec3_scale(px,scale),px,vec3_scale(py,scale),py,&filtered));
        if(i) {filtered_variation+=fabs(filtered.colorR-last_filtered.colorR);point_variation+=fabs(point_sample.colorR-last_point.colorR);}
        last_filtered=filtered;last_point=point_sample;
    }
    assert(filtered_variation<point_variation*.5);
    fclose(samples);
    assert(error/count<.05);
    double repeat=round(tau*m.reference_radius_m/m.tile_m[0]);
    assert(RuntimeSurfaceMappingEvaluateTiles(0,1e-9,1.37,&base,&left));
    assert(RuntimeSurfaceMappingEvaluateTiles(0,repeat+1e-9,1.37,&base,&right));m1_same(&left,&right);
    assert(RuntimeSurfaceMappingEvaluateTiles(0,repeat-1e-9,1.37,&base,&right));m1_same(&left,&right);
    HitInfo3D pole;HitInfo3D_Reset(&pole);pole.sceneObjectIndex=0;pole.position=vec3(0,0,scale);
    CoreAuthoredSurfaceCoordinates q;assert(RuntimeSurfaceMappingCoordinates(&pole,&q));assert(q.singular && q.source_weight==0);
    RuntimeMaterialPayload3D pole_payload;assert(RuntimeMaterialPayload3D_ResolveFromHit(&pole,&pole_payload));assert(fabs(pole_payload.baseColorR-base.colorR)<1e-12);
    RuntimeScene3D_Free(&scene);
    if(reopen) {fprintf(stderr,"M2 reopen channels/coords/cache passed MAE %.6f\n",error/count);return;}
    char diagnostic[512];
    /* UI edits the same agent-authored mapping while retaining producer fields. */
    ObjectEditorSetSelectedObjectIndex(0);SceneEditorSessionRuntimeRender(editor);
    assert(!MaterialEditorApplySliderValueToFocused(MATERIAL_EDITOR_SLIDER_SCALE,.8));
    assert(!MaterialEditorApplyLayerKindToFocused(RUNTIME_MATERIAL_TEXTURE_LAYER_KIND_WOOD));
    m2_control(editor,"expand");m2_edit(editor,"tile_width","0.4");
    assert(RuntimeSurfaceMappingDefinition(0,&m) && fabs(m.tile_m[0]-.4)<1e-12);
    unsigned long long revision=SceneEditorDocumentRevision();m2_edit_attempt(editor,"radius","0",false);assert(SceneEditorDocumentRevision()==revision);
    m2_edit(editor,"seam","0.23");m2_edit(editor,"offset_v","0.13");
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));assert(SceneEditorDocumentRedo(diagnostic,sizeof(diagnostic)));
    m2_control(editor,"axis");m2_control(editor,"axis");m2_control(editor,"axis");
    /* Restore the original seam reference through undo to avoid changing its orientation. */
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    m2_control(editor,"space");assert(RuntimeSurfaceMappingDefinition(0,&m) && m.space==CORE_AUTHORED_SURFACE_WORLD);
    HitInfo3D world_hit;HitInfo3D_Reset(&world_hit);world_hit.sceneObjectIndex=0;world_hit.position=vec3(scale,0,.4*scale);
    CoreAuthoredSurfaceCoordinates before,after;assert(RuntimeSurfaceMappingCoordinates(&world_hit,&before));
    world_hit.position=vec3(0,scale,.4*scale);assert(RuntimeSurfaceMappingCoordinates(&world_hit,&after));
    double current_repeat=round(tau*m.reference_radius_m/m.tile_m[0]);
    double diff=after.uv_tiles[0]-before.uv_tiles[0];diff-=floor(diff/current_repeat)*current_repeat;
    assert(fabs(diff-round(tau*m.reference_radius_m/m.tile_m[0])*.25)<1e-9 ||
           fabs(diff-(round(tau*m.reference_radius_m/m.tile_m[0])*.25+repeat))<1e-9);
    m2_control(editor,"space");assert(RuntimeSurfaceMappingDefinition(0,&m) && m.space==CORE_AUTHORED_SURFACE_OBJECT_REST);
    RuntimeMaterialSurfaceEval original,copy;assert(RuntimeSurfaceMappingEvaluateTiles(0,2.34,1.29,&base,&original));
    int index=-1;assert(SceneEditorDocumentDuplicateForSceneIndex(0,&index,diagnostic,sizeof(diagnostic)));
    assert(RuntimeSurfaceMappingEvaluateTiles(index,2.34,1.29,&base,&copy));m1_same(&original,&copy);
    assert(SceneEditorDocumentRemoveForSceneIndex(0,diagnostic,sizeof(diagnostic)));assert(RuntimeSurfaceMappingEvaluateTiles(0,2.34,1.29,&base,&copy));m1_same(&original,&copy);
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    SceneEditorDocumentTransform t;assert(SceneEditorDocumentGetTransformForSceneIndex(0,&t,diagnostic,sizeof(diagnostic)));
    t.position[0]=1.3;t.position[1]=-.7;t.rotation_degrees[2]=37;t.scale[0]=1.7;t.scale[1]=.8;
    assert(SceneEditorDocumentSetTransformForSceneIndex(0,&t,diagnostic,sizeof(diagnostic)));
    double a=.73,r=37*tau/360;Vec3 point=vec3(cos(a),sin(a),.27);
    HitInfo3D transformed;HitInfo3D_Reset(&transformed);transformed.sceneObjectIndex=0;
    transformed.position=vec3((point.x*1.7*cos(r)-point.y*.8*sin(r)+1.3)*scale,(point.x*1.7*sin(r)+point.y*.8*cos(r)-.7)*scale,point.z*scale);
    CoreAuthoredSurfaceCoordinates expected;assert(RuntimeSurfaceMappingDefinition(0,&m));double pp[]={point.x,point.y,point.z};assert(core_authored_surface_coordinates(&m,pp,&expected));
    assert(RuntimeSurfaceMappingCoordinates(&transformed,&q));assert(fabs(q.uv_tiles[0]-expected.uv_tiles[0])<1e-9 && fabs(q.uv_tiles[1]-expected.uv_tiles[1])<1e-9);
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentSave(diagnostic,sizeof(diagnostic)));
    ObjectEditorSetSelectedObjectIndex(0);assert(SceneEditorFrameViewport(true));choose_menu(editor,3,SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    capture(editor,"mapping_m2_inspector.ppm");m2_control(editor,"expand");ObjectEditorSetSelectedObjectIndex(-1);
    capture(editor,"mapping_m2.ppm");
    SceneEditorDigestOverlayNavState nav=*SceneEditorGetViewportNavState();
    unsigned long long builds=SceneEditorSurfaceMappingCacheBuildCount();Uint64 start=SDL_GetPerformanceCounter();
    for(int i=0;i<12;++i) {SceneEditorDigestOverlayNavState orbit=nav;orbit.orbit_yaw_deg+=i*3;SceneEditorRestoreViewportNav(&orbit);SceneEditorSessionRuntimeRender(editor);}
    double ms=(SDL_GetPerformanceCounter()-start)*1000./SDL_GetPerformanceFrequency()/12;
    assert(SceneEditorSurfaceMappingCacheBuildCount()==builds);capture(editor,"mapping_m2_orbit.ppm");
    SceneEditorDigestOverlayNavState distant=nav;distant.overlay_zoom*=.25;distant.orbit_pitch_deg=80;
    SceneEditorRestoreViewportNav(&distant);capture(editor,"mapping_m2_distant.ppm");
    assert(SceneEditorSurfaceMappingCacheBuildCount()==builds);
    FILE* f=fopen("mapping_m2.json","w");assert(f);fprintf(f,"{\"ray_hits\":%d,\"cache_rgb_mae\":%.9g,\"orbit_ms\":%.6f,\"orbit_rebuilds\":0,\"filtered_motion_variation\":%.9g,\"point_motion_variation\":%.9g,\"seam_poles_ui_transform_duplicate\":true}\n",count/3,error/count,ms,filtered_variation,point_variation);fclose(f);
}

static void surface_mapping_panel_probe(SceneEditor* editor) {
    char diagnostic[512];CoreAuthoredSurfaceMapping m;
    ObjectEditorSetSelectedObjectIndex(0);assert(!RuntimeSurfaceMappingActive(0));
    SceneEditorSessionRuntimeRender(editor);m2_control(editor,"expand");m2_control(editor,"planar");
    assert(RuntimeSurfaceMappingDefinition(0,&m) && m.version==1);
    m2_edit(editor,"tile_width","0.7");m2_edit(editor,"offset_u","0.15");m2_edit(editor,"rotation","0.3");
    assert(RuntimeSurfaceMappingDefinition(0,&m) && m.tile_m[0]==.7 && m.offset_m[0]==.15 && m.rotation_rad==.3);
    m2_control(editor,"legacy");assert(!RuntimeSurfaceMappingActive(0));
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));assert(RuntimeSurfaceMappingActive(0));
    assert(SceneEditorDocumentSave(diagnostic,sizeof(diagnostic)));
    assert(SceneEditorFrameViewport(true));choose_menu(editor,3,SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    capture(editor,"mapping_plane_inspector.ppm");
}

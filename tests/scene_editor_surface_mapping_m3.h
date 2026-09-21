#include "procedural/procedural_solid_authored_material_runtime.h"
#include "procedural/procedural_solid_material_runtime_program.h"
#include "editor/scene_editor_surface_material_panel.h"
#include "editor/scene_editor_material_graph.h"

static json_object* m3_member(json_object* o,const char* key) {
    json_object* v=NULL;assert(o && json_object_object_get_ex(o,key,&v));return v;
}
static void surface_mapping_m3_image_probe(SceneEditor* editor) {
    char ref[64];assert(RuntimeMaterialAuthoredTextureGetMappingReference(0,ref,sizeof(ref)));
    assert(!strcmp(ref,"image-chart"));
    RuntimeMaterialSurfaceEval sample;
    assert(RuntimeSurfaceMaterialSamplePrimitive(0,0,.31,.62,&sample));
    HitInfo3D hit;HitInfo3D_Reset(&hit);hit.sceneObjectIndex=0;hit.position=vec3((.31-.5)*4,(.62-.5)*2,.5);
    CoreAuthoredSurfaceCoordinates q;assert(RuntimeSurfaceMappingCoordinates(&hit,&q));
    RuntimeMaterialAuthoredTextureSample image;
    assert(RuntimeMaterialAuthoredTextureSampleFace(0,0,q.uv_tiles[0]-floor(q.uv_tiles[0]),q.uv_tiles[1]-floor(q.uv_tiles[1]),&image));
    assert(fabs(sample.colorR-image.colorR)<1e-9 && fabs(sample.colorG-image.colorG)<1e-9);
    ObjectEditorSetSelectedObjectIndex(0);SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);assert(SceneEditorFrameViewport(true));
    choose_menu(editor,3,SCENE_EDITOR_MESH_DISPLAY_MATERIAL);capture(editor,"m3-authored-image.ppm");
}
static void m3_solid_graph_sample(void) {
    CoreMeshAssetRuntimeVertex vertices[4]={
        {{-1,-1,0},{0,0,1}},{{1,-1,0},{0,0,1}},
        {{1,1,1},{0,0,1}},{{-1,1,1},{0,0,1}}};
    CoreMeshAssetRuntimeTriangle triangles[2]={{0,1,2,"retained.shell"},{0,2,3,"retained.shell"}};
    const char* kinds[2]={"retained","retained"};
    CoreMeshAssetRuntimeDocument mesh={0};mesh.vertex_count=mesh.vertex_normal_count=4;
    mesh.vertices=vertices;mesh.triangle_count=2;mesh.triangles=triangles;
    mesh.contract.local_bounds.min=(CoreObjectVec3){-1,-1,0};mesh.contract.local_bounds.max=(CoreObjectVec3){1,1,1};
    ProceduralSolidMaterialGraphV1 graph;ProceduralSolidMaterialGraphReport report;
    assert(ProceduralSolidMaterialGraphV1_FromTemplate("snow_accumulation","mapped-solid","binding",
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",&graph,&report));
    graph.node_count=graph.layer_count=1;
    snprintf(graph.surface_mapping_ref,sizeof(graph.surface_mapping_ref),"front-chart");
    ProceduralSolidAuthoredMaterialV1 material;
    assert(ProceduralSolidAuthoredMaterialV1_FromTemplate("weathered_rock","base_material",&material,NULL));
    material.surface.texture.enabled=true;
    snprintf(material.surface.texture.kind,sizeof(material.surface.texture.kind),"brick");
    material.surface.texture.strength=1;material.surface.texture.scale_units=1;
    material.surface.texture.microdetail_normal_strength=0;
    ProceduralSolidMaterialRuntimeProgramV1 program;ProceduralSolidMaterialRuntimeProgramV1_Init(&program);
    assert(ProceduralSolidMaterialRuntimeProgramV1_Build(&graph,&material,1,&mesh,kinds,&program,&report));
    HitInfo3D hit;HitInfo3D_Reset(&hit);hit.sceneObjectIndex=0;hit.position=vec3(.17,.12,.5);hit.normal=hit.geometricNormal=hit.shadingNormal=vec3(0,0,1);
    hit.hasRegionAuthoredMaterial=true;hit.regionAuthoredMaterial.base_color_r=.7;hit.regionAuthoredMaterial.base_color_g=.4;hit.regionAuthoredMaterial.base_color_b=.2;
    hit.regionAuthoredMaterial.roughness=.8;hit.regionAuthoredMaterial.ior=1.5;
    hit.regionAuthoredMaterial.texture.enabled=true;snprintf(hit.regionAuthoredMaterial.texture.kind,sizeof(hit.regionAuthoredMaterial.texture.kind),"brick");
    hit.regionAuthoredMaterial.texture.strength=1;hit.regionAuthoredMaterial.texture.scale_units=1;
    hit.proceduralSolidMaterialRuntimeProgram=&program;hit.localTriangleIndex=0;hit.baryU=.2;hit.baryV=.3;hit.baryW=.5;
    RuntimeMaterialPayload3D first;assert(RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0,&first));
    assert(ProceduralSolidAuthoredMaterial_ApplyHitToPayload(&sceneSettings.sceneObjects[0],&hit,&first));
    hit.triangleIndex=923;hit.localTriangleIndex=1;hit.baryU=.1;hit.baryV=.7;hit.baryW=.2;
    RuntimeMaterialPayload3D reordered;assert(RuntimeMaterialPayload3D_ResolveFromSceneObjectIndex(0,&reordered));
    assert(ProceduralSolidAuthoredMaterial_ApplyHitToPayload(&sceneSettings.sceneObjects[0],&hit,&reordered));
    assert(fabs(first.baseColorR-reordered.baseColorR)<1e-12 && fabs(first.bsdf.roughness-reordered.bsdf.roughness)<1e-12);
    snprintf(program.graph.surface_mapping_ref,sizeof(program.graph.surface_mapping_ref),"missing");
    assert(!ProceduralSolidAuthoredMaterial_ApplyHitToPayload(&sceneSettings.sceneObjects[0],&hit,&reordered));assert(!reordered.valid);
    ProceduralSolidMaterialRuntimeProgramV1_Free(&program);
}
static void surface_mapping_m3_probe(SceneEditor* editor,bool reopen) {
    (void)editor;
    char diagnostic[512],row_text[65536];
    RuntimeMaterialGraphDocument graph;
    assert(SceneEditorMaterialGraphGetObjectGraph(0,&graph));
    assert(!strcmp(graph.graphId,"agent-graph") && !strcmp(graph.nodes[0].nodeId,"agent-node"));
    assert(SceneEditorDocumentGetSurfaceMaterialJSON(0,row_text,sizeof(row_text)));
    json_object* row=json_tokener_parse(row_text);
    assert(!strcmp(json_object_get_string(m3_member(row,"producer_note")),"retain this"));
    if(reopen) {
        assert(fabs(graph.nodes[0].layer.opacity-.43)<1e-12);
        RuntimeMaterialSurfaceEval sample;
        for(int face=0;face<6;++face) {
            assert(RuntimeSurfaceMaterialSamplePrimitive(0,face,.31,.62,&sample));
            assert(fabs(sample.roughness-(.1+face*.12))<1e-9);
        }
        json_object_put(row);return;
    }
    ObjectEditorSetSelectedObjectIndex(0);
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_MATERIALS);
    SceneEditorSessionRuntimeRender(editor);
    SDL_Rect control;assert(SceneEditorSurfaceMaterialPanelControl("opacity",&control));
    click(editor,control);assert(SceneEditorSurfaceMaterialPanelActive());
    SDL_Event input={0};input.type=SDL_TEXTINPUT;snprintf(input.text.text,sizeof(input.text.text),"0.43");
    SceneEditorSessionRuntimeHandleEvent(editor,&input);key(editor,SDLK_RETURN);
    assert(!SceneEditorSurfaceMaterialPanelActive());
    assert(SceneEditorMaterialGraphGetObjectGraph(0,&graph) && fabs(graph.nodes[0].layer.opacity-.43)<1e-12);
    assert(SceneEditorFrameViewport(true));
    choose_menu(editor,3,SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    capture(editor,"m3-source-panel.ppm");
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    RuntimeMaterialSurfaceEval ui_before[6];
    for(int face=0;face<6;++face) assert(RuntimeSurfaceMaterialSamplePrimitive(0,face,.31,.62,&ui_before[face]));
    SceneEditorSessionRuntimeRender(editor);assert(SceneEditorSurfaceMaterialPanelControl("scope",&control));click(editor,control);
    SceneEditorSessionRuntimeRender(editor);assert(SceneEditorSurfaceMaterialPanelControl("opacity",&control));click(editor,control);
    input=(SDL_Event){0};input.type=SDL_TEXTINPUT;snprintf(input.text.text,sizeof(input.text.text),"0.37");
    SceneEditorSessionRuntimeHandleEvent(editor,&input);key(editor,SDLK_RETURN);
    RuntimeMaterialSurfaceEval front_changed;assert(RuntimeSurfaceMaterialSamplePrimitive(0,0,.31,.62,&front_changed));
    assert(fabs(front_changed.colorR-ui_before[0].colorR)>1e-5);
    for(int face=1;face<6;++face) {RuntimeMaterialSurfaceEval sample;assert(RuntimeSurfaceMaterialSamplePrimitive(0,face,.31,.62,&sample));m1_same(&sample,&ui_before[face]);}
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));assert(SceneEditorDocumentRedo(diagnostic,sizeof(diagnostic)));
    SceneEditorSessionRuntimeRender(editor);assert(SceneEditorSurfaceMaterialPanelControl("reset",&control));click(editor,control);
    for(int face=0;face<6;++face) {RuntimeMaterialSurfaceEval sample;assert(RuntimeSurfaceMaterialSamplePrimitive(0,face,.31,.62,&sample));m1_same(&sample,&ui_before[face]);}
    unsigned long long revision=SceneEditorDocumentRevision();
    assert(SceneEditorDocumentSetSurfaceLayerValue(0,"base","","opacity",.43,revision,diagnostic,sizeof(diagnostic)));
    assert(!SceneEditorDocumentSetSurfaceLayerValue(0,"base","","opacity",.2,revision,diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
    assert(SceneEditorMaterialGraphGetObjectGraph(0,&graph) && graph.nodes[0].layer.opacity==1);
    assert(SceneEditorDocumentRedo(diagnostic,sizeof(diagnostic)));
    assert(SceneEditorMaterialGraphGetObjectGraph(0,&graph) && fabs(graph.nodes[0].layer.opacity-.43)<1e-12);
    assert(!SceneEditorDocumentSetSurfaceLayerValue(0,"missing","","opacity",.2,SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));
    const char* roles[]={"front","back","left","right","top","bottom"};
    json_object* binding=m3_member(row,"surface_material_binding"),*regions=json_object_new_array();
    json_object_object_add(binding,"regions",regions);
    RuntimeMaterialSurfaceEval previous[6];
    for(int face=0;face<6;++face) assert(RuntimeSurfaceMaterialSamplePrimitive(0,face,.31,.62,&previous[face]));
    for(int face=0;face<6;++face) {
        char text[1024];snprintf(text,sizeof(text),"{\"id\":\"region-%d\",\"face_role\":\"%s\",\"source\":{\"material_texture_stack\":{\"layers\":[{\"id\":\"solid\",\"kind\":\"solid\",\"roughness_influence\":%.17g}]}}}",face,roles[face],.1+face*.12);
        json_object_array_add(regions,json_tokener_parse(text));
        assert(SceneEditorDocumentSetSurfaceBinding(0,json_object_to_json_string(binding),SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));
        for(int other=0;other<6;++other) {
            RuntimeMaterialSurfaceEval sample;assert(RuntimeSurfaceMaterialSamplePrimitive(0,other,.31,.62,&sample));
            if(other!=face) m1_same(&sample,&previous[other]);
            else assert(fabs(sample.roughness-(.1+face*.12))<1e-9);
            previous[other]=sample;
        }
    }
    /* Mapping references are resolved before rendering. A changed region map
       changes only that face, independently of triangle storage order. */
    char mapping_text[8192];assert(SceneEditorDocumentGetSurfaceMappingJSON(0,mapping_text,sizeof(mapping_text)));
    json_object* mapping=json_tokener_parse(mapping_text),*maps=NULL,*entry=json_object_new_object();
    if(!json_object_object_get_ex(binding,"mappings",&maps)) {maps=json_object_new_array();json_object_object_add(binding,"mappings",maps);}
    json_object_object_add(entry,"id",json_object_new_string("front-chart"));
    json_object_object_add(entry,"definition",mapping);json_object_array_add(maps,entry);
    json_object* front=json_object_array_get_idx(regions,0);
    json_object* saved_source=json_object_get(m3_member(front,"source"));
    json_object* source=json_tokener_parse("{\"material_texture_stack\":{\"layers\":[{\"id\":\"brick\",\"kind\":\"brick\",\"mapping_ref\":\"front-chart\"}]}}");
    json_object_object_add(front,"source",source);
    assert(SceneEditorDocumentSetSurfaceBinding(0,json_object_to_json_string(binding),SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));
    RuntimeMaterialSurfaceEval before_map;assert(RuntimeSurfaceMaterialSamplePrimitive(0,0,.31,.62,&before_map));
    json_object_object_add(mapping,"offset_m",json_tokener_parse("[0.19,0.07]"));
    assert(SceneEditorDocumentSetSurfaceBinding(0,json_object_to_json_string(binding),SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));
    RuntimeMaterialSurfaceEval after_map;assert(RuntimeSurfaceMaterialSamplePrimitive(0,0,.31,.62,&after_map));
    assert(fabs(before_map.colorR-after_map.colorR)>1e-5);
    for(int face=1;face<6;++face) {RuntimeMaterialSurfaceEval sample;assert(RuntimeSurfaceMaterialSamplePrimitive(0,face,.31,.62,&sample));m1_same(&sample,&previous[face]);}
    json_object_object_add(front,"source",saved_source);
    assert(SceneEditorDocumentSetSurfaceBinding(0,json_object_to_json_string(binding),SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));
    /* Invalid named references and out-of-range source values do not commit. */
    revision=SceneEditorDocumentRevision();
    json_object_object_add(front,"mapping_ref",json_object_new_string("missing-chart"));
    assert(!SceneEditorDocumentSetSurfaceBinding(0,json_object_to_json_string(binding),revision,diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentRevision()==revision);json_object_object_del(front,"mapping_ref");
    m3_solid_graph_sample();
    /* Overlapping face assignments are rejected atomically. */
    revision=SceneEditorDocumentRevision();
    json_object_array_add(regions,json_object_get(json_object_array_get_idx(regions,0)));
    assert(!SceneEditorDocumentSetSurfaceBinding(0,json_object_to_json_string(binding),revision,diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentRevision()==revision);
    json_object_array_del_idx(regions,6,1);
    assert(SceneEditorRuntimeScenePersistAuthoring(diagnostic,sizeof(diagnostic)));
    json_object_put(row);
    FILE* report=fopen("mapping_m3.json","w");assert(report);
    fputs("{\"graph_roundtrip\":true,\"stale_edit_rejected\":true,\"six_face_isolation\":true,\"overlap_rejected\":true}\n",report);fclose(report);
}

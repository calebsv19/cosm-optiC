/* Real inspector events author a composition from an ordinary primitive. Image
 * import uses the deterministic document boundary, not an unattended OS dialog. */
static void composition_authoring_t3_click(SceneEditor *editor,const char *name) {
    resources_t2_click(editor,name);
}
static void composition_authoring_t3_pick(SceneEditor *editor,const char *button,
                                          const char *prefix,const char *id) {
    composition_authoring_t3_click(editor,button);
    composition_authoring_t3_click(editor,"node_search");
    authoring_t1_text(editor,id);
    char control[100];snprintf(control,sizeof(control),"%s:%s",prefix,id);
    composition_authoring_t3_click(editor,control);
}
static json_object *composition_authoring_t3_snapshot(void) {
    json_object *result=json_object_new_object();assert(result);
    json_object_object_add(result,"row",authoring_t1_row(0));
    json_object_object_add(result,"sampling",resources_t2_sampling(0));
    char mapping[8192];assert(SceneEditorDocumentGetSurfaceMappingJSON(0,mapping,sizeof(mapping)));
    json_object_object_add(result,"mapping",json_tokener_parse(mapping));return result;
}
static void composition_authoring_t3_same(json_object *expected) {
    json_object *actual=composition_authoring_t3_snapshot();assert(json_object_equal(actual,expected));json_object_put(actual);
}
static void composition_authoring_t3_probe(SceneEditor *editor,bool reopen) {
    char diagnostic[1024];SDL_Rect unused;
    ObjectEditorSetSelectedObjectIndex(0);
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_MATERIALS);
    SceneEditorSessionRuntimeRender(editor);
    composition_authoring_t3_click(editor,"section:Sources");
    if(reopen) {
        json_object *expected=json_object_from_file("composition_authoring_t3_expected.json");assert(expected);
        composition_authoring_t3_same(expected);json_object_put(expected);
        assert(RuntimeSurfaceGraphActive(0) && RuntimeSurfaceSamplingActive(0));
        composition_authoring_t3_click(editor,"composition:resources");
        composition_authoring_t3_click(editor,"resource:base_color:check");
        capture(editor,"t3-authoring-reopen.ppm");
        FILE *receipt=fopen("composition_authoring_t3_reopen.json","w");assert(receipt);
        fputs("{\"fresh_process_document_match\":true,\"resources_healthy\":true}\n",receipt);assert(!fclose(receipt));return;
    }
    json_object *before=authoring_t1_row(0);
    unsigned long long revision=SceneEditorDocumentRevision();
    composition_authoring_t3_click(editor,"replace");
    composition_authoring_t3_click(editor,"preset:composition");
    composition_authoring_t3_click(editor,"cancel");
    assert(SceneEditorDocumentRevision()==revision);authoring_t1_same(0,before);json_object_put(before);
    composition_authoring_t3_click(editor,"replace");
    composition_authoring_t3_click(editor,"preset:composition");
    composition_authoring_t3_click(editor,"confirm");
    assert(SceneEditorDocumentRevision()>revision && RuntimeSurfaceGraphActive(0) && RuntimeSurfaceSamplingActive(0));
    json_object *row=authoring_t1_row(0),*graph=authoring_t1_member(row,"surface_graph");
    assert(json_object_get_int(authoring_t1_member(graph,"version"))==2);json_object_put(row);
    const char *channels[]={"base_color","roughness"},*variables[]={"OPTIC_T3_BASE_COLOR","OPTIC_T3_ROUGHNESS"};
    for(int i=0;i<2;++i){const char *path=getenv(variables[i]);assert(path && path[0]);
        assert(SceneEditorDocumentSurfaceSamplingSetChannel(0,channels[i],path,i?"data":"linear",SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));}
    composition_authoring_t3_click(editor,"composition:resources");
    composition_authoring_t3_click(editor,"resource:base_color:check");
    assert(!SceneEditorSurfaceMaterialPanelControl("node_list",&unused));
    assert(!SceneEditorSurfaceMaterialPanelControl("resource:disable",&unused));
    capture(editor,"t3-authoring-resources.ppm");
    composition_authoring_t3_click(editor,"composition:graph");
    composition_authoring_t3_pick(editor,"add_node","kind","image_color");
    composition_authoring_t3_pick(editor,"add_node","kind","image_scalar");
    composition_authoring_t3_pick(editor,"add_node","kind","roughness_mix");
    composition_authoring_t3_pick(editor,"node_list","node","finish");
    composition_authoring_t3_pick(editor,"input0","candidate","image_color_1");
    composition_authoring_t3_pick(editor,"node_list","node","roughness_mix_1");
    composition_authoring_t3_pick(editor,"input1","candidate","image_scalar_1");
    composition_authoring_t3_pick(editor,"input2","candidate","pattern");
    composition_authoring_t3_pick(editor,"output_roughness","candidate","roughness_mix_1");
    composition_authoring_t3_pick(editor,"node_list","node","image_scalar_1");
    composition_authoring_t3_click(editor,"image_resource");
    row=authoring_t1_row(0);graph=authoring_t1_member(row,"surface_graph");
    assert(!strcmp(json_object_get_string(authoring_t1_member(authoring_t1_node(graph,"image_scalar_1"),"resource")),"base_color_alpha"));json_object_put(row);
    composition_authoring_t3_click(editor,"image_resource"); /* Returns to the assigned roughness channel. */
    before=composition_authoring_t3_snapshot();revision=SceneEditorDocumentRevision();
    composition_authoring_t3_click(editor,"delete_node");
    assert(SceneEditorDocumentRevision()==revision);composition_authoring_t3_same(before);json_object_put(before);
    assert(!SceneEditorDocumentSurfaceSamplingSetChannel(0,"roughness",NULL,NULL,revision,diagnostic,sizeof(diagnostic)));
    assert(SceneEditorDocumentRevision()==revision);
    /* Every untouched scope must render inherited outputs without a regions array. */
    SceneEditorDocumentObjectInfo scope_object;assert(SceneEditorDocumentObjectAt(0,&scope_object));
    int scope_count=!strcmp(scope_object.type,"rect_prism_primitive")?7:2;
    for(int i=0;i<scope_count;++i)composition_authoring_t3_click(editor,"graph_scope");
    assert(SceneEditorDocumentRevision()==revision);
    composition_authoring_t3_click(editor,"graph_scope"); /* Object -> front. */
    composition_authoring_t3_pick(editor,"output_color","candidate","light");
    before=composition_authoring_t3_snapshot();
    composition_authoring_t3_pick(editor,"output_roughness","candidate","rough");
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));composition_authoring_t3_same(before);
    composition_authoring_t3_pick(editor,"output_color","candidate","");
    row=authoring_t1_row(0);graph=authoring_t1_member(row,"surface_graph");
    json_object *regions=NULL;assert(!json_object_object_get_ex(graph,"regions",&regions));json_object_put(row);
    assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));composition_authoring_t3_same(before);json_object_put(before);
    capture(editor,"t3-authoring-graph.ppm");
    composition_authoring_t3_click(editor,"section:Coordinates");
    assert(!SceneEditorSurfaceMaterialPanelControl("node_list",&unused));
    assert(!SceneEditorSurfaceMappingPanelControl("legacy",&unused));
    assert(!SceneEditorSurfaceMappingPanelControl("axial",&unused));
    composition_authoring_t3_click(editor,"section:Sources");
    before=composition_authoring_t3_snapshot();
    assert(SceneEditorDocumentSave(diagnostic,sizeof(diagnostic)));
    composition_authoring_t3_same(before);
    assert(!json_object_to_file_ext("composition_authoring_t3_expected.json",before,JSON_C_TO_STRING_PRETTY));json_object_put(before);
    FILE *receipt=fopen("composition_authoring_t3.json","w");assert(receipt);
    fputs("{\"preset_cancel_replace\":true,\"image_import\":true,\"native_file_chooser\":false,\"typed_nodes_wiring\":true,\"resource_property\":true,\"dependent_delete_rejected\":true,\"face_override_inherit_undo\":true,\"save\":true}\n",receipt);assert(!fclose(receipt));
}

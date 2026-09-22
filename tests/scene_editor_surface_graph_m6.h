#include "editor/scene_editor_surface_material_panel.h"
#include "render/runtime_surface_graph.h"
/* Cross-adapter, retained document and actual typed-control acceptance. */
static void surface_graph_m6_probe(SceneEditor *editor) {
    assert(RuntimeSurfaceGraphActive(0));
    char original[65536], after[65536], diagnostic[512];
    assert(SceneEditorDocumentGetSurfaceMaterialJSON(0, original, sizeof(original)));
    json_object *row = json_tokener_parse(original), *graph = NULL, *nodes = NULL;
    assert(json_object_object_get_ex(row, "surface_graph", &graph));
    assert(json_object_object_get_ex(graph, "nodes", &nodes));
    RuntimeScene3D scene;
    RuntimeScene3D_Init(&scene);
    assert(RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene));
    const CoreMeshPreviewLodMesh *lod = SceneEditorMeshPreviewStoreGetForQuality(0, true);
    double error = 0;
    int count = 0;
    for (int t = 0; t < scene.triangleMesh.triangleCount; ++t)
        for (int sample = 1; sample <= 24; ++sample) {
            const RuntimeTriangle3D *tri = &scene.triangleMesh.triangles[t];
            double w[] = {.05 + sample * .01, .31, .64 - sample * .01};
            Vec3 point = vec3_add(vec3_add(vec3_scale(tri->p0, w[0]), vec3_scale(tri->p1, w[1])),
                                  vec3_scale(tri->p2, w[2]));
            Vec3 n =
                vec3_normalize(vec3_cross(vec3_sub(tri->p1, tri->p0), vec3_sub(tri->p2, tri->p0)));
            Ray3D ray = RuntimeRay3D_Make(vec3_add(point, vec3_scale(n, 2)), vec3_scale(n, -1));
            HitInfo3D h;
            assert(RuntimeRay3D_TraceSceneFirstHit(&scene, &ray, 1e-5, 4, &h));
            h.hasPixelFootprint = true;
            h.pixelDpDx = vec3(.012, .004, 0);
            h.pixelDpDy = vec3(0, .011, .003);
            RuntimeMaterialPayload3D payload;
            assert(RuntimeMaterialPayload3D_ResolveFromHit(&h, &payload));
            if (lod) {
                RuntimeMaterialSurfaceEval preview;
                assert(RuntimeSurfaceMaterialSampleMeshFootprint(
                    0, 0, tri->localTriangleIndex, w, point, h.shadingNormal, lod, &h.pixelDpDx,
                    &h.pixelDpDy, &preview));
                error = fmax(error, fabs(preview.colorR - payload.baseColorR));
                error = fmax(error, fabs(preview.colorG - payload.baseColorG));
                assert(preview.linearColor);
            }
            /* Independent inverse-frame position from the compiled asset's source vertices. */
            if (lod) {
                CoreSurfaceGraph program;
                assert(RuntimeSurfaceGraphParse(graph, &program, diagnostic, sizeof(diagnostic)));
                const CoreMeshAssetRuntimeDocument *asset =
                    &ray_tracing_runtime_mesh_assets_last()->assets[0].document;
                CoreMeshAssetRuntimeTriangle source = asset->triangles[tri->localTriangleIndex];
                size_t indices[] = {source.a, source.b, source.c};
                CoreSurfaceGraphQuery query = {0};
                query.position[1][0] = point.x;
                query.position[1][1] = point.y;
                query.position[1][2] = point.z;
                for (int k = 0; k < 3; ++k) {
                    CoreObjectVec3 v = asset->vertices[indices[k]].position;
                    query.position[0][0] += w[k] * v.x;
                    query.position[0][1] += w[k] * v.y;
                    query.position[0][2] += w[k] * v.z;
                }
                /* Noise is independent of normal weights, so this checks rest/world transforms
                 * separately. */
                if (program.nodes[1].kind == CORE_SG_NOISE3D) {
                    h.hasPixelFootprint = false;
                    RuntimeMaterialPayload3D point_payload;
                    assert(RuntimeMaterialPayload3D_ResolveFromHit(&h, &point_payload));
                    CoreSurfaceGraphResult expected;
                    assert(core_surface_graph_evaluate(&program, &query, &expected));
                    assert(fabs(point_payload.baseColorR - expected.color[0]) < 1e-8);
                }
            }
            assert(fabs(payload.bsdf.roughness - .65) < 1e-10);
            h.footprintUnbounded = true;
            assert(RuntimeMaterialPayload3D_ResolveFromHit(&h, &payload));
            assert(fabs(payload.baseColorR - .425) < 1e-10);
            ++count;
        }
    assert(count > 0 && error < 1e-8);
    unsigned long long rev = SceneEditorDocumentRevision();
    json_object *finish = json_object_array_get_idx(nodes, 5), *inputs = NULL;
    assert(json_object_object_get_ex(finish, "inputs", &inputs));
    json_object_array_put_idx(inputs, 0, json_object_new_string("finish"));
    assert(!SceneEditorDocumentSetSurfaceGraph(0, json_object_to_json_string(graph), rev,
                                               diagnostic, sizeof(diagnostic)));
    assert(SceneEditorDocumentRevision() == rev);
    json_object_array_put_idx(inputs, 0, json_object_new_string("dark"));
    assert(!SceneEditorDocumentSetSurfaceGraph(0, json_object_to_json_string(graph), rev + 1,
                                               diagnostic, sizeof(diagnostic)));
    /* Exercise the inspector's coordinate scale field with real SDL events. */
    ObjectEditorSetSelectedObjectIndex(0);
    assert(SceneEditorFrameViewport(true));
    SceneEditorWorkspaceProfileSelect(editor, SCENE_WORKSPACE_MATERIALS);
    SceneEditorSessionRuntimeRender(editor);
    SDL_Rect control;
    assert(SceneEditorSurfaceMaterialPanelControl("parameter0", &control));
    click(editor, control);
    assert(SceneEditorSurfaceMaterialPanelActive());
    SDL_Event event = {0};
    event.type = SDL_TEXTINPUT;
    snprintf(event.text.text, sizeof(event.text.text), "0.37");
    SceneEditorSessionRuntimeHandleEvent(editor, &event);
    key(editor, SDLK_RETURN);
    assert(!SceneEditorSurfaceMaterialPanelActive());
    assert(SceneEditorDocumentRevision() > rev);
    assert(SceneEditorDocumentGetSurfaceMaterialJSON(0, after, sizeof(after)));
    assert(strcmp(original, after));
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    assert(SceneEditorDocumentGetSurfaceMaterialJSON(0, after, sizeof(after)));
    assert(!strcmp(original, after));
    assert(SceneEditorDocumentRedo(diagnostic, sizeof(diagnostic)));
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    /* The real panel rewires a typed mix input and the command remains reversible. */
    for (int node = 0; node < 5; ++node) {
        SceneEditorSessionRuntimeRender(editor);
        assert(SceneEditorSurfaceMaterialPanelControl("layer", &control));
        click(editor, control);
    }
    SceneEditorSessionRuntimeRender(editor);
    assert(SceneEditorSurfaceMaterialPanelControl("input0", &control));
    click(editor, control);
    assert(SceneEditorDocumentGetSurfaceMaterialJSON(0, after, sizeof(after)));
    assert(strcmp(original, after));
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    capture(editor, "m6-typed-inspector.ppm");
    int copy = -1;
    assert(SceneEditorDocumentDuplicateForSceneIndex(0, &copy, diagnostic, sizeof(diagnostic)));
    assert(RuntimeSurfaceGraphActive(copy));
    assert(SceneEditorDocumentUndo(diagnostic, sizeof(diagnostic)));
    assert(SceneEditorRuntimeScenePersistAuthoring(diagnostic, sizeof(diagnostic)));
    choose_menu(editor, 3, SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    assert(SceneEditorMeshPreviewModeGet() == SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    capture(editor, "m6-material.ppm");
    FILE *report = fopen("mapping_m6.json", "w");
    assert(report);
    fprintf(report,
            "{\"samples\":%d,\"adapter_max_error\":%.12g,\"typed_ui_undo_redo\":true,\"duplicate\":"
            "true}\n",
            count, error);
    fclose(report);
    RuntimeScene3D_Free(&scene);
    json_object_put(row);
}

/* Diagnostic proof: compare the viewport's pre-cache albedo evaluator with
 * actual ray-hit payloads. Lighting is deliberately excluded from this metric. */
#include "editor/material_preview_surface_eval.h"
#include "editor/scene_editor_runtime_scene_persistence.h"
#include "render/runtime_scene_3d_builder.h"
#include "render/runtime_material_payload_3d.h"
#include "render/runtime_surface_mapping.h"
#include "editor/scene_editor_viewport_material.h"

static unsigned char parity_byte(double v) { return (unsigned char)(fmax(0,fmin(1,v))*255+.5); }
static void material_parity_probe(SceneEditor* editor, bool edit) {
    ObjectEditorSetSelectedObjectIndex(0);
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_MATERIALS);
    if(edit) {
        MaterialEditorSetActiveSubPane(MATERIAL_EDITOR_SUBPANE_TEXTURES);
        assert(MaterialEditorApplySliderValueToFocused(MATERIAL_EDITOR_SLIDER_SCALE,(2.0-.25)/7.75));
        assert(MaterialEditorApplySliderValueToFocused(MATERIAL_EDITOR_SLIDER_OFFSET_U,.23));
        assert(MaterialEditorApplySliderValueToFocused(MATERIAL_EDITOR_SLIDER_OFFSET_V,.19));
        char diagnostics[512];
        assert(SceneEditorRuntimeScenePersistAuthoring(diagnostics,sizeof(diagnostics)));
    }
    RuntimeMaterialTextureStack stack;
    assert(SceneEditorMaterialStackGetEffectiveObjectStack(&sceneSettings.sceneObjects[0],0,&stack));
    FILE* report=fopen("parity.json","w");assert(report);
    fprintf(report,"{\"scale\":%.12g,\"offset_u\":%.12g,\"offset_v\":%.12g,\"faces\":[",
        stack.layers[0].placement.scale,stack.layers[0].placement.offsetU,stack.layers[0].placement.offsetV);
    RuntimeScene3D scene;RuntimeScene3D_Init(&scene);
    assert(RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene));
    /* Fixtures are a centered 4 x 2 plane or a 4 x 2 x 1 prism. */
    const Vec3 normals[6]={{0,0,1},{0,0,-1},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}};
    for(int face=0;face<6;++face) {
        char a[80],b[80];snprintf(a,sizeof(a),"face_%d_viewport_albedo.ppm",face);
        snprintf(b,sizeof(b),"face_%d_render_albedo.ppm",face);
        FILE* va=fopen(a,"wb"),*ra=fopen(b,"wb");assert(va&&ra);
        fprintf(va,"P6\n256 256\n255\n");fprintf(ra,"P6\n256 256\n255\n");
        double error=0,channel_error=0,cache_error=0;int count=0,different=0,group=-1;
        for(int y=0;y<256;++y) for(int x=0;x<256;++x) {
            double u=(x+.5)/256,v=(y+.5)/256;
            Vec3 n=normals[face],p;
            if(face<2) p=(Vec3){(u-.5)*4,(v-.5)*2,n.z*.5};
            else if(face<4) p=(Vec3){n.x*2,(u-.5)*2,v-.5};
            else p=(Vec3){(u-.5)*4,n.y,v-.5};
            Ray3D ray=RuntimeRay3D_Make((Vec3){p.x+n.x*5,p.y+n.y*5,p.z+n.z*5},(Vec3){-n.x,-n.y,-n.z});
            HitInfo3D hit;unsigned char vp[3]={0},rp[3]={0};
            if(RuntimeRay3D_TraceSceneFirstHit(&scene,&ray,.001,20,&hit) && hit.sceneObjectIndex==0) {
                RuntimeMaterialPayload3D payload;RuntimeMaterialSurfaceEval eval;
                assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit,&payload));
                int query_face=0;double qu=0,qv=0;
                assert(RuntimeSurfaceMaterialPrimitiveIsland(0,hit.position,n,&query_face,&qu,&qv));
                assert(RuntimeSurfaceMaterialSamplePrimitive(0,query_face,qu,qv,&eval));
                double actual[]={payload.baseColorR,payload.baseColorG,payload.baseColorB,payload.bsdf.roughness,payload.bsdf.reflectivity,payload.bsdf.specWeight,payload.bsdf.diffuseWeight,payload.transparency};
                double expected[]={eval.colorR,eval.colorG,eval.colorB,eval.roughness,eval.reflectivity,eval.specWeight,eval.diffuseWeight,eval.transparency};
                for(int k=0;k<8;++k) channel_error=fmax(channel_error,fabs(actual[k]-expected[k]));
                RuntimeMaterialSurfaceEval cached;
                assert(SceneEditorViewportMaterialSample(SceneEditorViewportMaterialPrepareFace(0,query_face),qu,qv,&cached));
                cache_error+=fabs(cached.colorR-eval.colorR)+fabs(cached.colorG-eval.colorG)+fabs(cached.colorB-eval.colorB);
                vp[0]=parity_byte(eval.colorR);vp[1]=parity_byte(eval.colorG);vp[2]=parity_byte(eval.colorB);
                rp[0]=parity_byte(payload.baseColorR);rp[1]=parity_byte(payload.baseColorG);rp[2]=parity_byte(payload.baseColorB);
                int maxdiff=0;for(int c=0;c<3;++c){int d=abs((int)vp[c]-rp[c]);error+=d;if(d>maxdiff)maxdiff=d;}
                different+=maxdiff>3;count++;group=hit.localTriangleIndex/2;
            }
            fwrite(vp,1,3,va);fwrite(rp,1,3,ra);
        }
        fclose(va);fclose(ra);
        fprintf(report,"%s{\"probe_face\":%d,\"runtime_face_group\":%d,\"hits\":%d,\"mae_255\":%.6f,\"different_fraction\":%.6f,\"max_channel_error\":%.12g,\"cache_rgb_mae\":%.12g}",face?",":"",face,group,count,count?error/(count*3):0,count?(double)different/count:0,channel_error,count?cache_error/(count*3):0);
    }
    fprintf(report,"]}\n");fclose(report);RuntimeScene3D_Free(&scene);
    SceneEditorWorkspaceProfileSelect(editor,SCENE_WORKSPACE_SCENE);
    assert(SceneEditorFrameViewport(true));
    choose_menu(editor,3,SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    ObjectEditorSetSelectedObjectIndex(-1);
    capture(editor,"viewport.ppm");
}

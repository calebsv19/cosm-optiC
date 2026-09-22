/* Independent oracles: image constants are known PNG bytes, RGB blends are
 * linear and roughness is an RMS material mixture, not a linear interpolation. */
#include "scene_editor_surface_composition_t3_oracles.h"
static double composition_t3_normal_variance;
static json_object* composition_t3_row(void) {
    size_t size=SceneEditorDocumentSurfaceMaterialJSONSize(0);assert(size);
    char* bytes=malloc(size);assert(bytes);
    assert(SceneEditorDocumentGetSurfaceMaterialJSON(0,bytes,size));
    json_object* row=json_tokener_parse(bytes);free(bytes);assert(row);return row;
}
static json_object* composition_t3_member(json_object* object,const char* key) {
    json_object* value=NULL;assert(json_object_object_get_ex(object,key,&value));return value;
}
static void composition_t3_values(const RuntimeMaterialPayload3D* p,bool color_region,bool rough_region) {
    const double mask=128./255, image[]={64./255,128./255,192./255},base[]={.8,.2,.1},region[]={.1,.7,.3};
    const double actual[]={p->baseColorR,p->baseColorG,p->baseColorB};
    for(int k=0;k<3;++k) {
        double expected=color_region?region[k]:(1-mask)*base[k]+mask*image[k];
        assert(isfinite(actual[k]) && fabs(actual[k]-expected)<2e-6);
    }
    double expected_rough=rough_region?.9:sqrt((1-mask)*.2*.2+mask*(153./255)*(153./255)+composition_t3_normal_variance);
    assert(isfinite(p->bsdf.roughness) && fabs(p->bsdf.roughness-expected_rough)<2e-6);
}
static void composition_t3_region_samples(bool plane) {
    /* Explicit face ordering is the public primitive-island adapter contract. */
    for(int face=0;face<(plane?1:6);++face) {
        RuntimeMaterialSurfaceEval eval;assert(RuntimeSurfaceMaterialSamplePrimitive(0,face,.37,.61,&eval));
        RuntimeMaterialPayload3D p={0};p.baseColorR=eval.colorR;p.baseColorG=eval.colorG;p.baseColorB=eval.colorB;p.bsdf.roughness=eval.roughness;
        composition_t3_values(&p,face==0,face==1);
        assert(eval.linearColor);
    }
    RuntimeScene3D scene;RuntimeScene3D_Init(&scene);assert(RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene));
    const Vec3 normals[]={{0,0,1},{0,0,-1},{0,-1,0},{0,1,0},{-1,0,0},{1,0,0}};
    for(int face=0;face<(plane?1:6);++face) {
        Ray3D ray=RuntimeRay3D_Make(vec3_scale(normals[face],3),vec3_scale(normals[face],-1));
        HitInfo3D hit;assert(RuntimeRay3D_TraceSceneFirstHit(&scene,&ray,1e-5,6,&hit));
        assert(vec3_dot(hit.geometricNormal,normals[face])>.999);
        RuntimeMaterialPayload3D p;assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit,&p));
        composition_t3_values(&p,face==0,face==1);
    }
    RuntimeScene3D_Free(&scene);
}
static void composition_t3_regions(bool plane,bool reopen) {
    char diagnostic[512];
    json_object* original=composition_t3_row();
    composition_t3_region_samples(plane);
    if(!reopen) {
        unsigned long long revision=SceneEditorDocumentRevision();
        assert(!SceneEditorDocumentSurfaceGraphSetRegionOutput(0,"front","roughness","region_color",revision,diagnostic,sizeof(diagnostic)));
        assert(SceneEditorDocumentRevision()==revision);
        assert(SceneEditorDocumentSurfaceGraphSetRegionOutput(0,"front","roughness","region_rough",revision,diagnostic,sizeof(diagnostic)));
        RuntimeMaterialSurfaceEval eval;assert(RuntimeSurfaceMaterialSamplePrimitive(0,0,.37,.61,&eval));
        assert(fabs(eval.roughness-.9)<2e-6 && fabs(eval.colorR-.1)<2e-6);
        assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
        json_object* restored=composition_t3_row();assert(json_object_equal(original,restored));json_object_put(restored);
        assert(SceneEditorDocumentSurfaceGraphSetRegionOutput(0,"front","base_color",NULL,SceneEditorDocumentRevision(),diagnostic,sizeof(diagnostic)));
        assert(RuntimeSurfaceMaterialSamplePrimitive(0,0,.37,.61,&eval));
        RuntimeMaterialPayload3D p={0};p.baseColorR=eval.colorR;p.baseColorG=eval.colorG;p.baseColorB=eval.colorB;p.bsdf.roughness=eval.roughness;
        composition_t3_values(&p,false,false);
        assert(SceneEditorDocumentUndo(diagnostic,sizeof(diagnostic)));
        restored=composition_t3_row();assert(json_object_equal(original,restored));json_object_put(restored);
        assert(SceneEditorDocumentSave(diagnostic,sizeof(diagnostic)));
    }
    json_object_put(original);
}
static void surface_composition_t3_probe(SceneEditor* editor,bool reopen) {
    const char* kind=getenv("OPTIC_T3_CASE");assert(kind);
    composition_t3_normal_variance=!strcmp(kind,"variance")?1-(221./255)/hypot(64./127,221./255):0;
    assert(RuntimeSurfaceGraphActive(0) && RuntimeSurfaceSamplingActive(0));
    bool regions=!strcmp(kind,"regions") || !strcmp(kind,"plane_regions");
    size_t samples=0;double max_adapter_error=0;
    if(regions) {
        composition_t3_regions(!strcmp(kind,"plane_regions"),reopen);samples=!strcmp(kind,"regions")?6:1;
    } else {
        bool procedural=!strcmp(kind,"rest") || !strcmp(kind,"world");
        json_object* row=composition_t3_row();CoreSurfaceGraph graph;char diagnostic[512];
        assert(RuntimeSurfaceGraphParse(composition_t3_member(row,"surface_graph"),&graph,diagnostic,sizeof(diagnostic)));
        RuntimeScene3D scene;RuntimeScene3D_Init(&scene);assert(RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene));
        const CoreMeshPreviewLodMesh* lod=SceneEditorMeshPreviewStoreGetForQuality(0,true);assert(lod && lod->attribute_protected);
        for(int j=0;j<scene.triangleMesh.triangleCount;++j) {
            const RuntimeTriangle3D* tri=&scene.triangleMesh.triangles[j];
            for(int s=0;s<3;++s) {
                const double w[]={.13+s*.09,.31,.56-s*.09};
                Vec3 point=vec3_add(vec3_add(vec3_scale(tri->p0,w[0]),vec3_scale(tri->p1,w[1])),vec3_scale(tri->p2,w[2]));
                Vec3 normal=vec3_normalize(vec3_cross(vec3_sub(tri->p1,tri->p0),vec3_sub(tri->p2,tri->p0)));
                Ray3D ray=RuntimeRay3D_Make(vec3_add(point,vec3_scale(normal,2)),vec3_scale(normal,-1));
                HitInfo3D hit;assert(RuntimeRay3D_TraceSceneFirstHit(&scene,&ray,1e-5,4,&hit));
                assert(hit.hasSurfaceUV && hit.hasSurfaceDifferentials && !strcmp(hit.uvSetId,"paint_uv"));
                hit.hasPixelFootprint=true;hit.pixelDpDx=vec3_scale(hit.surfaceDpDu,.17);hit.pixelDpDy=vec3_scale(hit.surfaceDpDv,.03);
                if(!strcmp(kind,"variance")) {hit.pixelDpDx=vec3_scale(hit.surfaceDpDu,4096);hit.pixelDpDy=vec3_scale(hit.surfaceDpDv,4096);}
                CoreSurfaceGraphInputs expected_inputs;composition_t3_inputs(&hit,&graph,&expected_inputs);
                RuntimeMaterialPayload3D p;assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit,&p));
                if(!procedural)composition_t3_values(&p,false,false);
                else {
                    HitInfo3D point_hit=hit;point_hit.hasPixelFootprint=false;
                    CoreSurfaceGraphResult oracle=composition_t3_position_oracle(&point_hit,tri,w,&graph,&expected_inputs);
                    RuntimeMaterialPayload3D point_payload;assert(RuntimeMaterialPayload3D_ResolveFromHit(&point_hit,&point_payload));
                    assert(fabs(point_payload.baseColorR-oracle.color[0])<2e-6 && fabs(point_payload.baseColorG-oracle.color[1])<2e-6 && fabs(point_payload.baseColorB-oracle.color[2])<2e-6);
                    assert(fabs(point_payload.bsdf.roughness-oracle.roughness)<2e-6);
                }
                RuntimeMaterialSurfaceEval preview;
                assert(RuntimeSurfaceMaterialSampleMeshFootprint(0,0,tri->localTriangleIndex,w,point,hit.shadingNormal,lod,&hit.pixelDpDx,&hit.pixelDpDy,&preview));
                double errors[]={fabs(preview.colorR-p.baseColorR),fabs(preview.colorG-p.baseColorG),fabs(preview.colorB-p.baseColorB),fabs(preview.roughness-p.bsdf.roughness)};
                for(int k=0;k<4;++k)max_adapter_error=fmax(max_adapter_error,errors[k]);
                assert(preview.linearColor && max_adapter_error<2e-6);
                bool tilted=!strcmp(kind,"nonflat") || !strcmp(kind,"mirrored");
                if(tilted) {
                    Vec3 n=hit.shadingNormal;
                    Vec3 t=vec3_normalize(vec3_sub(hit.surfaceDpDu,vec3_scale(n,vec3_dot(hit.surfaceDpDu,n))));
                    double sign=vec3_dot(vec3_cross(n,t),hit.surfaceDpDv)<0?-1:1;
                    Vec3 b=vec3_scale(vec3_cross(n,t),sign);
                    Vec3 expected=vec3_normalize(vec3_add(vec3_add(vec3_scale(t,64./127),vec3_scale(b,32./127)),vec3_scale(n,221./255)));
                    assert(p.hasMicrodetailNormal && vec3_length(vec3_sub(p.microdetailShadingNormal,expected))<2e-6);
                    HitInfo3D applied=hit;assert(RuntimeMaterialPayload3D_ApplyShadingNormal(&p,&applied));
                    RuntimeMaterialPayload3D again;assert(RuntimeMaterialPayload3D_ResolveFromHit(&applied,&again));
                    assert(vec3_length(vec3_sub(again.microdetailShadingNormal,expected))<2e-6);
                    composition_t3_values(&again,false,false);
                } else if(!strcmp(kind,"height_ramp")) {
                    composition_t3_height_oracle(hit);
                } else if(p.hasMicrodetailNormal) {
                    assert(vec3_length(vec3_sub(p.microdetailShadingNormal,hit.shadingNormal))<2e-6);
                }
                if(!strcmp(kind,"variance")) {
                    /* The filtered normal is neutral; preserve its original basis
                     * explicitly to exercise response re-entry without compounding
                     * the already-added variance into graph roughness. */
                    HitInfo3D again_hit=hit;
                    again_hit.hasUnperturbedShadingNormal=true;
                    again_hit.unperturbedShadingNormal=hit.shadingNormal;
                    again_hit.shadingNormal=again_hit.normal=p.hasMicrodetailNormal?p.microdetailShadingNormal:hit.shadingNormal;
                    RuntimeMaterialPayload3D again;assert(RuntimeMaterialPayload3D_ResolveFromHit(&again_hit,&again));
                    composition_t3_values(&again,false,false);
                    assert(again.bsdf.roughness>=0 && again.bsdf.roughness<=1);
                }
                hit.footprintUnbounded=true;assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit,&p));
                if(!procedural)composition_t3_values(&p,false,false);
                else {
                    assert(fabs(p.baseColorR-(.8+64./255)*.5)<2e-6);
                    assert(fabs(p.baseColorG-(.2+128./255)*.5)<2e-6);
                    assert(fabs(p.baseColorB-(.1+192./255)*.5)<2e-6);
                }
                ++samples;
            }
        }
        assert(samples>=6);RuntimeScene3D_Free(&scene);json_object_put(row);
    }
    bool cache_probe=!strcmp(kind,"nonflat");
    if(cache_probe)composition_t3_decoder_cache();
    choose_menu(editor,3,SCENE_EDITOR_MESH_DISPLAY_MATERIAL);
    capture(editor,reopen?"t3-reopen.ppm":"t3-material.ppm");
    FILE* report=fopen(reopen?"composition_t3_reopen.json":"composition_t3.json","w");assert(report);
    bool procedural=!strcmp(kind,"rest") || !strcmp(kind,"world");
    fprintf(report,"{\"case\":\"%s\",\"samples\":%zu,\"reopen\":%s,\"constant_linear_rgb_rms_oracle\":%s,\"typed_image_input_oracle\":%s,\"asset_barycentric_rest_world_oracle\":%s,\"primitive_face_ray_and_preview_values\":%s,\"far_mip_variance_rms_oracle_and_reentry\":%s,\"m5_t3_m5_decoder_cache_separation_reuse\":%s",
        kind,samples,reopen?"true":"false",procedural?"false":"true",regions?"false":"true",procedural?"true":"false",regions?"true":"false",!strcmp(kind,"variance")?"true":"false",cache_probe?"true":"false");
    if(!regions)fprintf(report,",\"adapter_max_error\":%.12g",max_adapter_error);
    fprintf(report,"}\n");
    assert(!fclose(report));
}

#include "render/runtime_specular_reflection_3d.h"
#include "render/runtime_dielectric_transport_3d.h"
#ifdef RUNTIME_RAY_3D_IDEAL_FOOTPRINT_VERSION
#include "scene_editor_ideal_footprint_numeric_t4.h"
#endif
static unsigned long long footprint_t4_primary_hits,footprint_t4_secondary_hits,footprint_t4_actual_receivers,footprint_t4_reference_receivers;
/* Test-only analytical camera/plane transport. The reference does not reuse
 * production reflection/refraction or any transported footprint. */
static Vec3 footprint_t4_direction(double x,double y) {
    return vec3_normalize(vec3(((x+.5)/160*2-1)*.35,(1-(y+.5)/120*2)*.2625,-1));
}
static Ray3D footprint_t4_camera(double x,double y,int distance,int pose,bool differential) {
    /* Avoid aligning the complete pixel/subpixel lattice with the receiver's
     * two-triangle diagonal; this fixture measures filtering, not edge coverage. */
    Ray3D ray=RuntimeRay3D_Make(vec3(.013123+pose*.007,.019371,1.+distance),footprint_t4_direction(x,y));
    ray.hasDifferentials=differential;
    ray.directionDx=footprint_t4_direction(x+1,y);ray.directionDy=footprint_t4_direction(x,y+1);
#ifdef RUNTIME_RAY_3D_IDEAL_FOOTPRINT_VERSION
    ray.originDx=ray.originDy=ray.origin;
    ray.hasDifferentialOrigins=true;
#endif
    return ray;
}
static RuntimeMaterialPayload3D footprint_t4_sample(RuntimeScene3D* scene,int route,double x,double y,int distance,int pose,bool reference) {
    Ray3D primary=footprint_t4_camera(x,y,distance,pose,!reference);
    HitInfo3D receiver;bool found=false;
    if(reference && route) {
        /* Plane z=0, receiver z=+4 for reflection and -4 for refraction.
         * Snell's law is independently expressed by its tangential component. */
        double t=-primary.origin.z/primary.direction.z;
        Vec3 at=vec3_add(primary.origin,vec3_scale(primary.direction,t));
        Vec3 direction=primary.direction;
        if(route==1)direction.z=-direction.z;
        else {
            const double eta=1./1.5;
            direction.x*=eta;direction.y*=eta;
            direction.z=-sqrt(1-direction.x*direction.x-direction.y*direction.y);
        }
        Ray3D ray=RuntimeRay3D_Make(vec3_add(at,vec3(0,0,route==1?1e-4:-1e-4)),direction);
        found=RuntimeRay3D_TraceSceneFirstHit(scene,&ray,1e-6,100,&receiver);
    } else {
        HitInfo3D source;bool primary_hit=RuntimeRay3D_TraceSceneFirstHit(scene,&primary,1e-6,100,&source);
        if(!primary_hit)fprintf(stderr,"T4 primary miss route%d xy%.12g,%.12g distance%d pose%d reference%d triangles%d origin%.9g,%.9g,%.9g dir%.9g,%.9g,%.9g\n",route,x,y,distance,pose,reference,scene->triangleMesh.triangleCount,primary.origin.x,primary.origin.y,primary.origin.z,primary.direction.x,primary.direction.y,primary.direction.z);
        assert(primary_hit);
        if(!reference)++footprint_t4_primary_hits;
        if(route==0) {receiver=source;found=true;}
        else {
            RuntimeMaterialPayload3D source_payload;assert(RuntimeMaterialPayload3D_ResolveFromHit(&source,&source_payload));
            if(route==1) {
                RuntimeSpecularReflection3DResult result;
                assert(RuntimeSpecularReflection3D_Trace(scene,&source,&source_payload,vec3_scale(primary.direction,-1),NULL,&result));
                found=result.geometryHit;receiver=result.hitInfo;
            } else {
                assert(!source_payload.thinWalled);
                RuntimeDielectricTransport3D transport;
                assert(RuntimeDielectricTransport3D_ResolveInterface(&source_payload,source.shadingNormal,primary.direction,1,1.5,&transport));
                assert(transport.hasRefraction && !transport.totalInternalReflection);
                Ray3D ray=RuntimeRay3D_MakeOffset(source.position,HitInfo3D_OffsetNormal(&source),transport.refractionDir,1e-4);
#ifdef RUNTIME_RAY_3D_IDEAL_FOOTPRINT_VERSION
                (void)RuntimeRay3D_TransportIdealFootprint(&source,transport.orientedNormal,RUNTIME_RAY_IDEAL_REFRACTION,1,1.5,&ray);
#endif
                found=RuntimeRay3D_TraceSceneFirstHit(scene,&ray,1e-6,100,&receiver);
            }
        }
    }
    assert(found && receiver.sceneObjectIndex==(route?1:0));
    assert(RuntimeSurfaceGraphActive(receiver.sceneObjectIndex) && RuntimeSurfaceSamplingActive(receiver.sceneObjectIndex));
    if(reference)++footprint_t4_reference_receivers;
    else {++footprint_t4_actual_receivers;if(route)++footprint_t4_secondary_hits;}
    if(reference) {
        /* Explicit test-only point-reference override occurs only at receiver.
         * Production MakeOffset's chart-average policy cannot contaminate it. */
        receiver.hasPixelFootprint=false;receiver.footprintUnbounded=false;
    }
    RuntimeMaterialPayload3D p;assert(RuntimeMaterialPayload3D_ResolveFromHit(&receiver,&p));
    assert(isfinite(p.baseColorR) && isfinite(p.baseColorG) && isfinite(p.baseColorB));
    assert(p.baseColorR>=0 && p.baseColorR<=1 && p.baseColorG>=0 && p.baseColorG<=1 && p.baseColorB>=0 && p.baseColorB<=1);
    return p;
}
static void footprint_t4_write(const char* name,const float* image) {
    FILE* f=fopen(name,"wb");assert(f);assert(fwrite(image,sizeof(float),160*120*3,f)==160*120*3);assert(!fclose(f));
}
#ifdef RUNTIME_RAY_3D_IDEAL_FOOTPRINT_VERSION
static void footprint_t4_resource_eligibility(json_object* manifest) {
    const char* names[]={"fallback_normal","fallback_disabled"};
    for(int i=0;i<2;++i) {
        json_object* path=NULL;assert(json_object_object_get_ex(manifest,names[i],&path));
        RuntimeSceneBridgePreflight preflight={0};assert(runtime_scene_bridge_apply_file(json_object_get_string(path),&preflight));
        RuntimeScene3D scene;RuntimeScene3D_Init(&scene);assert(RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene));
        Ray3D incoming=footprint_t4_camera(79,59,0,0,true);HitInfo3D hit;
        assert(RuntimeRay3D_TraceSceneFirstHit(&scene,&incoming,1e-6,100,&hit));
        assert(hit.sceneObjectIndex==0 && hit.hasPixelFootprint);
        assert(RuntimeSurfaceSamplingNormalResponseActive(0)==(i==0));
        RuntimeMaterialPayload3D payload;assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit,&payload));
        /* A neutral sampled center does not establish known-zero source derivatives. */
        fprintf(stderr,"T4 resource eligibility %s: microdetail=%d roughness=%.12g active=%d\n",names[i],payload.hasMicrodetailNormal,payload.bsdf.roughness,RuntimeSurfaceSamplingNormalResponseActive(0));
        /* ApplySurfaceEval intentionally clamps authored zero to BSDF .02. */
        assert(!payload.hasMicrodetailNormal && fabs(payload.bsdf.roughness-.02)<1e-6);
        assert(payload.bsdf.roughness<=.08);
        RuntimeSpecularReflection3DResult reflected;
        assert(RuntimeSpecularReflection3D_Trace(&scene,&hit,&payload,vec3_scale(incoming.direction,-1),NULL,&reflected));
        assert(reflected.geometryHit && reflected.hitInfo.sceneObjectIndex==1);
        assert(reflected.ray.hasDifferentials==(i==1));
        assert(reflected.ray.footprintUnbounded==(i==0));
        RuntimeScene3D_Free(&scene);
    }
}
#endif
static void secondary_footprint_t4_probe(SceneEditor* editor) {
#ifdef RUNTIME_RAY_3D_IDEAL_FOOTPRINT_VERSION
    test_scene_editor_ideal_footprint_numeric_t4();
#endif
    (void)editor;const char* path=getenv("OPTIC_T4_MANIFEST");assert(path);
    json_object* manifest=json_object_from_file(path);assert(manifest);
    const char* names[]={"direct","mirror","refraction"};
    float *actual=calloc(160*120*3,sizeof(float)),*reference=calloc(160*120*3,sizeof(float)),*convergence=calloc(160*120*3,sizeof(float));
    assert(actual && reference && convergence);
    FILE* proof=fopen("native-proof.json","w");assert(proof);fprintf(proof,"{\"routes\":{");bool first=true;
    for(int route=0;route<3;++route) {
        json_object* selected=NULL;bool enabled=false;
        assert(json_object_object_get_ex(manifest,"_cases",&selected));
        for(size_t i=0;i<json_object_array_length(selected);++i)
            if(!strcmp(names[route],json_object_get_string(json_object_array_get_idx(selected,i))))enabled=true;
        if(!enabled)continue;
        footprint_t4_primary_hits=footprint_t4_secondary_hits=footprint_t4_actual_receivers=footprint_t4_reference_receivers=0;
        json_object* scene_path=NULL;assert(json_object_object_get_ex(manifest,names[route],&scene_path));
        RuntimeSceneBridgePreflight preflight={0};assert(runtime_scene_bridge_apply_file(json_object_get_string(scene_path),&preflight));
        RuntimeScene3D scene;RuntimeScene3D_Init(&scene);assert(RuntimeScene3DBuilder_BuildFromBridgeSeeds(&scene));
        assert(scene.primitiveCount==(route?2:1) && scene.triangleMesh.triangleCount==(route?4:2));
        for(int p=0;p<scene.primitiveCount;++p) {
            double expected=p==0?0:(route==1?4:-4);
            assert(scene.primitives[p].kind==RUNTIME_PRIMITIVE_3D_KIND_PLANE);
            assert(fabs(scene.primitives[p].shape.plane.origin.z-expected)<1e-12);
            assert(scene.primitives[p].shape.plane.width==20 && scene.primitives[p].shape.plane.height==20);
        }
#ifdef RUNTIME_RAY_3D_IDEAL_FOOTPRINT_VERSION
        if(route==1) {
            Ray3D ray=footprint_t4_camera(79,59,0,0,true);HitInfo3D hit;
            assert(RuntimeRay3D_TraceSceneFirstHit(&scene,&ray,1e-6,100,&hit));
            RuntimeMaterialPayload3D payload;assert(RuntimeMaterialPayload3D_ResolveFromHit(&hit,&payload));
            RuntimeSpecularReflection3DResult reflected;
            payload.bsdf.roughness=.2;
            assert(RuntimeSpecularReflection3D_Trace(&scene,&hit,&payload,vec3_scale(ray.direction,-1),NULL,&reflected));
            assert(!reflected.ray.hasDifferentials && reflected.ray.footprintUnbounded);
            payload.bsdf.roughness=0;payload.hasMicrodetailNormal=true;
            payload.microdetailShadingNormal=hit.shadingNormal;
            assert(RuntimeSpecularReflection3D_Trace(&scene,&hit,&payload,vec3_scale(ray.direction,-1),NULL,&reflected));
            assert(!reflected.ray.hasDifferentials && reflected.ray.footprintUnbounded);
        }
#endif
        for(int distance=0;distance<2;++distance)for(int pose=0;pose<2;++pose) {
            memset(convergence,0,160*120*3*sizeof(float));
            for(int y=0;y<120;++y)for(int x=0;x<160;++x) {
                size_t pixel=(y*160+x)*3;
                RuntimeMaterialPayload3D p=footprint_t4_sample(&scene,route,x,y,distance,pose,false);
                actual[pixel]=(float)p.baseColorR;actual[pixel+1]=(float)p.baseColorG;actual[pixel+2]=(float)p.baseColorB;
                double rgb[3]={0};
                for(int sy=0;sy<4;++sy)for(int sx=0;sx<4;++sx) {
                    p=footprint_t4_sample(&scene,route,x+(sx+.5)/4-.5,y+(sy+.5)/4-.5,distance,pose,true);
                    rgb[0]+=p.baseColorR/16;rgb[1]+=p.baseColorG/16;rgb[2]+=p.baseColorB/16;
                }
                for(int c=0;c<3;++c)reference[pixel+c]=(float)rgb[c];
                if(x>=60 && x<100 && y>=45 && y<75) {
                    double high[3]={0};
                    for(int sy=0;sy<8;++sy)for(int sx=0;sx<8;++sx) {
                        p=footprint_t4_sample(&scene,route,x+(sx+.5)/8-.5,y+(sy+.5)/8-.5,distance,pose,true);
                        high[0]+=p.baseColorR/64;high[1]+=p.baseColorG/64;high[2]+=p.baseColorB/64;
                    }
                    for(int c=0;c<3;++c)convergence[pixel+c]=(float)high[c];
                }
            }
            char name[128];snprintf(name,sizeof(name),"%s-d%d-p%d-actual.f32",names[route],distance,pose);footprint_t4_write(name,actual);
            snprintf(name,sizeof(name),"%s-d%d-p%d-reference.f32",names[route],distance,pose);footprint_t4_write(name,reference);
            snprintf(name,sizeof(name),"%s-d%d-p%d-convergence.f32",names[route],distance,pose);footprint_t4_write(name,convergence);
            fprintf(stderr,"T4 %s distance%d pose%d actual/reference complete\n",names[route],distance,pose);
        }
        RuntimeScene3D_Free(&scene);
        assert(footprint_t4_primary_hits==76800 && footprint_t4_actual_receivers==76800 && footprint_t4_reference_receivers==1536000);
        assert(footprint_t4_secondary_hits==(route?76800:0));
        fprintf(proof,"%s\"%s\":{\"actual_primary_hits\":%llu,\"actual_secondary_hits\":%llu,\"actual_receiver_hits\":%llu,\"reference_receiver_hits\":%llu,\"receiver_graph_and_sampling_active\":true}",first?"":",",names[route],footprint_t4_primary_hits,footprint_t4_secondary_hits,footprint_t4_actual_receivers,footprint_t4_reference_receivers);first=false;
    }
    int resource_cases=0;
#ifdef RUNTIME_RAY_3D_IDEAL_FOOTPRINT_VERSION
    footprint_t4_resource_eligibility(manifest);resource_cases=2;
#endif
    fprintf(proof,"},\"fixed_interior_mask_pixels\":12288,\"all_samples_hit_intended_receiver\":true,\"resource_normal_eligibility_cases\":%d}\n",resource_cases);assert(!fclose(proof));
    RuntimeSceneBridgePreflight restore={0};assert(runtime_scene_bridge_apply_file(SceneEditorDocumentPath(),&restore));
    free(actual);free(reference);free(convergence);json_object_put(manifest);
}

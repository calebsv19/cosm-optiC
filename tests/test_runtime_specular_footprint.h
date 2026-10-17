#include "render/runtime_specular_bsdf_3d.h"
#include "test_runtime_curved_reflection_footprint.h"
/* The independent oracle reruns the real sampler with the same u/v for the
 * neighboring view; it does not reconstruct the implementation's half vector. */
static void test_runtime_specular_footprint(void) {
    test_runtime_curved_reflection_footprint();
    HitInfo3D h={0};h.sceneObjectIndex=-1;
    h.normal=h.geometricNormal=h.shadingNormal=vec3(0,0,1);
    h.hasIncidentDifferentials=h.hasPixelFootprint=h.constantShadingNormal=true;
    h.pixelDpDx=vec3(.01,0,0);h.pixelDpDy=vec3(0,.01,0);
    RuntimeMaterialPayload3D p={0};p.valid=true;
    RuntimePrincipledBSDF3D b={0};b.valid=true;b.roughness=.02;
    b.specularF0R=.7;b.specularF0G=.8;b.specularF0B=.9;
    for (int k=0;k<32;++k) {
        b.roughness=k<16?.02:.08;
        Vec3 view=vec3_normalize(vec3(.3,0,1));
        h.incidentDirection=vec3_scale(view,-1);
        h.incidentDirectionDx=vec3_normalize(vec3_add(h.incidentDirection,vec3(.001,0,0)));
        h.incidentDirectionDy=vec3_normalize(vec3_add(h.incidentDirection,vec3(0,.001,0)));
        double u=(k+.5)/32,v=(k+.25)/32;
        RuntimeSpecularBSDF3DSample a=RuntimeSpecularBSDF3D_Sample(&b,&h,view,.25,u,v),saved=a;
        RuntimeSpecularBSDF3DSample x=RuntimeSpecularBSDF3D_Sample(&b,&h,vec3_scale(h.incidentDirectionDx,-1),.25,u,v);
        RuntimeSpecularBSDF3DSample y=RuntimeSpecularBSDF3D_Sample(&b,&h,vec3_scale(h.incidentDirectionDy,-1),.25,u,v);
        Ray3D ray=RuntimeRay3D_MakeOffset(vec3(1,2,3),h.normal,a.direction,1e-5),before=ray;
        assert_true("ggx_footprint_eligible",a.valid && x.valid && y.valid &&
            RuntimeSpecularBSDF3D_TransportFootprint(&p,&b,&h,view,&ray));
        assert_close("ggx_neighbor_x_sampler_oracle",vec3_length(vec3_sub(ray.directionDx,x.direction)),0,1e-10);
        assert_close("ggx_neighbor_y_sampler_oracle",vec3_length(vec3_sub(ray.directionDy,y.direction)),0,1e-10);
        assert_true("ggx_central_origin_direction_unchanged",!memcmp(&before.origin,&ray.origin,sizeof(Vec3)) &&
            !memcmp(&before.direction,&ray.direction,sizeof(Vec3)));
        RuntimeSpecularBSDF3DSample replay=RuntimeSpecularBSDF3D_Sample(&b,&h,view,.25,u,v);
        assert_true("ggx_pdf_weight_sample_unchanged",!memcmp(&replay,&saved,sizeof(saved)));
        assert_close("ggx_neighbor_origin",vec3_length(vec3_sub(ray.originDx,vec3_add(ray.origin,h.pixelDpDx))),0,1e-12);
        assert_true("ggx_bounded",ray.hasDifferentials && ray.hasDifferentialOrigins && ray.footprintTransported && !ray.footprintUnbounded);
        for(int bad=0;bad<12;++bad) {
            HitInfo3D q=h;RuntimeMaterialPayload3D pp=p;RuntimePrincipledBSDF3D bb=b;
            Vec3 vv=view;
            switch(bad) {
                case 0:q.constantShadingNormal=false;break;
                case 1:q.hasCurveTangent=true;break;
                case 2:q.footprintUnbounded=true;break;
                case 3:q.hasIncidentDifferentials=false;break;
                case 4:pp.hasMicrodetailNormal=true;break;
                case 5:pp.hasGlassTransportOverride=true;break;
                case 6:pp.transparency=.2;break;
                case 7:bb.roughness=.2;break;
                case 8:bb.transmissionWeight=.1;break;
                case 9:q.pixelDpDx.x=NAN;break;
                case 10:q.shadingNormal=vec3(.1,0,1);q.hasUnperturbedShadingNormal=true;q.unperturbedShadingNormal=h.shadingNormal;break;
                case 11:vv=vec3(0,0,1);break;
            }
            Ray3D rejected=ray;
            assert_true("ggx_unsupported_refused",!RuntimeSpecularBSDF3D_TransportFootprint(&pp,&bb,&q,vv,&rejected));
            assert_true("ggx_refusal_broad_and_central_preserved",rejected.footprintUnbounded && !rejected.hasDifferentials &&
                !rejected.footprintTransported && !memcmp(&before.origin,&rejected.origin,sizeof(Vec3)) &&
                !memcmp(&before.direction,&rejected.direction,sizeof(Vec3)));
        }
    }
}

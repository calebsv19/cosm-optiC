#include "render/runtime_camera_dielectric_3d.h"
#include "render/runtime_specular_bsdf_3d.h"
#include "render/runtime_dielectric_transport_3d.h"
#include "render/runtime_disney_v2_transport_internal_3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static const double pi=3.14159265358979323846;
static HitInfo3D flat_hit(void) {
    HitInfo3D h={0};h.normal=h.geometricNormal=h.shadingNormal=vec3(0,0,1);h.sceneObjectIndex=4;return h;
}
static double exact_fresnel(double c,double ni,double nt) {
    double st2=(ni/nt)*(ni/nt)*(1-c*c);if(st2>=1)return 1;
    double ct=sqrt(1-st2),rs=(ni*c-nt*ct)/(ni*c+nt*ct),rp=(nt*c-ni*ct)/(nt*c+ni*ct);
    return .5*(rs*rs+rp*rp);
}
static unsigned rng=456789;
static double uniform(void) { rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return (rng+.5)/4294967296.; }
int main(int argc,char**argv) {
    double ior=argc>1?atof(argv[1]):1.52;
    RuntimeMaterialPayload3D p={0};p.valid=true;p.opticalIor=ior;p.transparency=.97;
    p.baseColorR=p.baseColorG=p.baseColorB=1;p.hasGlassInterfaceTintOverride=true;
    p.glassInterfaceTintR=p.glassInterfaceTintG=p.glassInterfaceTintB=1;
    p.bsdf.baseColorR=p.bsdf.baseColorG=p.bsdf.baseColorB=1;p.bsdf.ior=ior;p.bsdf.roughness=.02;p.bsdf.specWeight=.04;p.bsdf.diffuseWeight=.05;p.bsdf.reflectivity=.04;
    RuntimePrincipledBSDF3D b=RuntimePrincipledBSDF3D_FromMaterialPayload(&p);HitInfo3D h=flat_hit();
    double angles[]={0,30,60,80,89};
    for(int a=0;a<5;a++) {
        double angle=angles[a]*pi/180,c=cos(angle);Vec3 incident=vec3(sin(angle),0,-c),view=vec3_scale(incident,-1);
        RuntimeDielectricTransport3D d={0};RuntimeDielectricTransport3D_Resolve(&p,h.normal,incident,&d);
        double pd=b.diffuseWeight*(1-b.transmissionWeight),ps=b.specularWeight,pt=b.transmissionWeight,total=pd+ps+pt;pd/=total;ps/=total;pt/=total;
        RuntimeDisneyV2_3DTransmissionSample t={0};RuntimeDisneyV2_3D_SampleTransmission(&p,&b,&h,view,pt,&t);
        double ref_sum=0,ref_max=0,pdf_error=0,identity_error=0,mis_pdf_error=0;int accepted=0;
        const int n=200000;
        for(int i=0;i<n;i++) {
            RuntimeSpecularBSDF3DSample s=RuntimeSpecularBSDF3D_Sample(&b,&h,view,ps,uniform(),uniform());
            if(!s.valid)continue;accepted++;ref_sum+=ps*s.throughputR;ref_max=fmax(ref_max,s.throughputR);
            pdf_error=fmax(pdf_error,fabs(s.pdf-ps*RuntimeSpecularBSDF3D_Pdf(&b,&h,view,s.direction))/fmax(s.pdf,1e-12));
            double mixture=s.pdf+pd*s.cosTheta/pi;
            double estimated=RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(&b,&h,incident,s.direction,true);
            mis_pdf_error=fmax(mis_pdf_error,fabs(estimated-mixture)/fmax(mixture,1e-12));
            Vec3 half=vec3_normalize(vec3_add(view,s.direction));double vh=vec3_dot(view,half),nh=vec3_dot(h.normal,half),nl=vec3_dot(h.normal,s.direction),alpha=fmax(b.roughness*b.roughness,.001),a2=alpha*alpha,den=nh*nh*(a2-1)+1;
            double D=a2/(pi*den*den),Gv=2/(1+sqrt(1+a2*(1-c*c)/(c*c))),Gl=2/(1+sqrt(1+a2*(1-nl*nl)/(nl*nl)));
            double f=D*Gv*Gl*RuntimePrincipledBSDF3D_FresnelSchlick(vh,b.specularF0R)/(4*c*nl);
            identity_error=fmax(identity_error,fabs(s.throughputR*s.pdf-f*nl)/fmax(fabs(f*nl),1e-12));
        }
        double expected_t=b.transmissionWeight*(1-exact_fresnel(c,1,ior))/(ior*ior);
        printf("{\"case\":\"interface\",\"ior\":%.9g,\"angle\":%.9g,\"ps\":%.9g,\"pt\":%.9g,\"f0\":%.9g,\"schlick\":%.9g,\"exact_fresnel\":%.9g,\"snell_error\":%.9g,\"transmission_weight\":%.9g,\"expected_clear_radiance_transmission\":%.9g,\"reflection_mean\":%.9g,\"reflection_max_weight\":%.9g,\"accepted\":%d,\"pdf_error\":%.9g,\"weight_identity_error\":%.9g,\"mis_pdf_relative_error\":%.9g}\n",ior,angles[a],ps,pt,b.specularF0R,d.fresnel,exact_fresnel(c,1,ior),fabs(ior*fabs(d.refractionDir.x)-sin(angle)),pt*t.throughputR,expected_t,ref_sum/n,ref_max,accepted,pdf_error,identity_error,mis_pdf_error);
    }
    /* A parallel slab must recover the original ray direction after exit. */
    RuntimeDielectricTransport3D entry={0},exit={0};Vec3 incoming=vec3(.5,0,-sqrt(.75));
    RuntimeDielectricTransport3D_ResolveInterface(&p,vec3(0,0,1),incoming,1,ior,&entry);
    RuntimeDielectricTransport3D_ResolveInterface(&p,vec3(0,0,-1),entry.refractionDir,ior,1,&exit);
    printf("{\"case\":\"parallel_slab\",\"ior\":%.9g,\"direction_error\":%.9g}\n",ior,vec3_length(vec3_sub(incoming,exit.refractionDir)));
    RuntimeTriangle3D tri={0};tri.normal=vec3(0,0,-1);Ray3D ray={0};ray.direction=entry.refractionDir;Vec3 geom,shading;
    RuntimeRay3D_ResolveTriangleNormals(&tri,&ray,0,0,1,&geom,&shading);
    HitInfo3D exit_hit=flat_hit();exit_hit.normal=exit_hit.geometricNormal=exit_hit.shadingNormal=shading;
    exit_hit.hasBoundaryOrientation=true;exit_hit.boundaryEntering=false;
    RuntimeDielectricTransport3D actual_exit={0};RuntimeCameraDielectric3D_Resolve(NULL,&p,&exit_hit,ray.direction,&actual_exit);
    printf("{\"case\":\"camera_slab_exit\",\"ior\":%.9g,\"eta_from\":%.9g,\"eta_to\":%.9g,\"direction_error\":%.9g}\n",ior,actual_exit.etaFrom,actual_exit.etaTo,vec3_length(vec3_sub(incoming,actual_exit.refractionDir)));
    /* Explicit glass-to-water differs from convenience resolver's air assumption. */
    RuntimeDielectricTransport3D gw={0},automatic={0};Vec3 inside=vec3(.5,0,sqrt(.75));
    RuntimeDielectricTransport3D_ResolveInterface(&p,h.normal,inside,ior,1.333,&gw);
    RuntimeDielectricTransport3D_Resolve(&p,h.normal,inside,&automatic);
    printf("{\"case\":\"nested_interface\",\"ior\":%.9g,\"explicit_eta_to\":%.9g,\"automatic_eta_to\":%.9g,\"direction_difference\":%.9g}\n",ior,gw.etaTo,automatic.etaTo,vec3_length(vec3_sub(gw.refractionDir,automatic.refractionDir)));
    Vec3 tir_incident=vec3(sin(70*pi/180),0,cos(70*pi/180));RuntimeDielectricTransport3D tir={0};
    RuntimeDielectricTransport3D_Resolve(&p,h.normal,tir_incident,&tir);
    int t_selected=0,t_bad_direction=0,s_selected=0,s_invalid=0,tir_budget_errors=0;
    for(unsigned i=0;i<10000;i++) {
        RuntimeNative3DSamplingContext sc={.sampleSequence=i,.temporalSubpassCount=1};RuntimeDisneyV2Transport3DVertexSample s={0};
        bool valid=runtime_disney_v2_transport_3d_sample_vertex(NULL,&h,&p,&b,&sc,tir_incident,3,false,NULL,&s);
        if(s.lobe==RUNTIME_DISNEY_V2_3D_LOBE_TRANSMISSION){t_selected++;if(vec3_length(vec3_sub(s.direction,tir.reflectionDir))>1e-5)t_bad_direction++;}
        if(valid && s.dielectricResolved && s.dielectric.totalInternalReflection && s.policyLobe!=RUNTIME_PATH_DEPTH_POLICY_3D_LOBE_TRANSMISSION)tir_budget_errors++;
        if(s.lobe==RUNTIME_DISNEY_V2_3D_LOBE_SPECULAR){s_selected++;if(!valid)s_invalid++;if(vec3_length(vec3_sub(s.direction,tir.reflectionDir))>1e-5)t_bad_direction++;}
    }
    printf("{\"case\":\"tir\",\"ior\":%.9g,\"tir\":%s,\"reported_fresnel\":%.9g,\"expected_fresnel\":1,\"transmission_selected\":%d,\"wrong_reflection_direction\":%d,\"specular_selected\":%d,\"specular_invalid\":%d}\n",ior,tir.totalInternalReflection?"true":"false",tir.fresnel,t_selected,t_bad_direction,s_selected,s_invalid);
    printf("{\"case\":\"tir_depth_budget\",\"ior\":%.9g,\"wrong_budget\":%d,\"reflections\":%d}\n",ior,tir_budget_errors,s_selected);
    /* Remove authored tint/reflectivity weights for a pure clear-dielectric reference. */
    RuntimeMaterialPayload3D clear=p;clear.transparency=1;clear.bsdf.specWeight=1;clear.bsdf.diffuseWeight=0;clear.bsdf.reflectivity=0;
    RuntimePrincipledBSDF3D cb=RuntimePrincipledBSDF3D_FromMaterialPayload(&clear);
    RuntimeDisneyV2_3DTransmissionSample ct={0};RuntimeDisneyV2_3D_SampleTransmission(&clear,&cb,&h,vec3(0,0,1),.5,&ct);
    double R=exact_fresnel(1,1,ior);
    printf("{\"case\":\"clear_radiance_reference\",\"ior\":%.9g,\"weighted_transmission\":%.9g,\"expected_radiance_transmission\":%.9g,\"expected_flux_transmission\":%.9g}\n",ior,.5*ct.throughputR,(1-R)/(ior*ior),1-R);
    double opacities[]={0,.5,.97};
    for(int i=0;i<3;i++) {
        RuntimeMaterialPayload3D mixed=p;mixed.transparency=opacities[i];
        RuntimePrincipledBSDF3D mb=RuntimePrincipledBSDF3D_FromMaterialPayload(&mixed);
        double pd=mb.diffuseWeight*(1-mb.transmissionWeight),ps=mb.specularWeight,pt=mb.transmissionWeight,total=pd+ps+pt;pd/=total;ps/=total;
        Vec3 view=vec3(.5,0,sqrt(.75));RuntimeSpecularBSDF3DSample ss=RuntimeSpecularBSDF3D_Sample(&mb,&h,view,ps,.25,.5);
        double actual=ss.pdf+pd*ss.cosTheta/pi;
        double evaluated=RuntimeDisneyV2_3D_EstimateDirectBsdfPdf(&mb,&h,vec3_scale(view,-1),ss.direction,true);
        printf("{\"case\":\"mixture_pdf\",\"transparency\":%.9g,\"sampled_mixture_density\":%.9g,\"evaluated_density\":%.9g,\"relative_error\":%.9g}\n",opacities[i],actual,evaluated,fabs(actual-evaluated)/fmax(actual,1e-12));
    }
    /* Independent branch batches reproduce rare grazing-reflection tails with white incident light. */
    RuntimePrincipledBSDF3D grazing=b;grazing.specularF0R=grazing.specularF0G=grazing.specularF0B=.0016;
    double ps=grazing.specularWeight/(grazing.diffuseWeight*(1-grazing.transmissionWeight)+grazing.specularWeight+grazing.transmissionWeight);
    Vec3 gv=vec3(sqrt(1-.031167118069307827*.031167118069307827),0,.031167118069307827);
    for(unsigned batch=0;batch<8;batch++) {
        rng=0x41a7593u+batch*0x9e3779b9u;double mean=0,m2=0,max=0;int count=0;const int n=65536;
        for(int i=0;i<n;i++) {
            double value=0;if(uniform()<ps) {
                RuntimeSpecularBSDF3DSample ss=RuntimeSpecularBSDF3D_Sample(&grazing,&h,gv,ps,uniform(),uniform());
                if(ss.valid)value=ss.throughputR;
            }
            if(value>0)count++;max=fmax(max,value);double delta=value-mean;mean+=delta/(i+1);m2+=delta*(value-mean);
        }
        printf("{\"case\":\"grazing_branch_batch\",\"batch\":%u,\"samples\":%d,\"reflection_count\":%d,\"mean\":%.9g,\"variance\":%.9g,\"maximum\":%.9g}\n",batch,n,count,mean,m2/(n-1),max);
    }
    RuntimeCameraMedium3D nested;RuntimeCameraMedium3D_Init(&nested);
    RuntimeMaterialPayload3D water=p;water.opticalIor=water.bsdf.ior=1.333;
    HitInfo3D boundary=flat_hit();boundary.hasBoundaryOrientation=true;boundary.boundaryEntering=true;
    RuntimeDielectricTransport3D nd={0};bool nested_ok=true;double eta_product=1;
    const RuntimeMaterialPayload3D* media[]={&p,&water,&water,&p};
    int objects[]={4,5,5,4};double expected_from[]={1,ior,1.333,ior},expected_to[]={ior,1.333,ior,1};
    for(int k=0;k<4;k++) {
        boundary.sceneObjectIndex=objects[k];boundary.boundaryEntering=k<2;
        bool resolved=RuntimeCameraDielectric3D_Resolve(&nested,media[k],&boundary,vec3(0,0,-1),&nd);
        nested_ok=nested_ok && resolved && fabs(nd.etaFrom-expected_from[k])<1e-12 && fabs(nd.etaTo-expected_to[k])<1e-12;
        eta_product*=RuntimeCameraDielectric3D_RadianceScale(media[k],&nd);
        nested_ok=nested_ok && RuntimeCameraMedium3D_Transmit(&nested,media[k],&boundary,&nd);
    }
    printf("{\"case\":\"nested_camera_stack\",\"passed\":%s,\"final_depth\":%u,\"radiance_scale_product\":%.9g}\n",nested_ok?"true":"false",RuntimeCausticPhotonMediumStack3D_Depth(&nested),eta_product);
    RuntimeCameraMedium3D_Init(&nested);boundary.sceneObjectIndex=4;boundary.boundaryEntering=true;
    RuntimeCameraDielectric3D_Resolve(&nested,&p,&boundary,vec3(0,0,-1),&nd);RuntimeCameraMedium3D_Transmit(&nested,&p,&boundary,&nd);
    boundary.normal=boundary.geometricNormal=boundary.shadingNormal=vec3(0,0,-1);boundary.boundaryEntering=false;
    bool tir_ok=RuntimeCameraDielectric3D_Resolve(&nested,&p,&boundary,tir_incident,&nd) && nd.totalInternalReflection;
    unsigned before=RuntimeCausticPhotonMediumStack3D_Depth(&nested);
    bool unchanged=RuntimeCameraMedium3D_Transmit(&nested,&p,&boundary,&nd) && before==RuntimeCausticPhotonMediumStack3D_Depth(&nested);
    printf("{\"case\":\"camera_tir_stack\",\"passed\":%s,\"depth_unchanged\":%s}\n",tir_ok?"true":"false",unchanged?"true":"false");
    boundary=flat_hit();boundary.hasBoundaryOrientation=true;boundary.boundaryEntering=true;boundary.sceneObjectIndex=5;
    RuntimeCameraDielectric3D_Resolve(&nested,&water,&boundary,vec3(0,0,-1),&nd);RuntimeCameraMedium3D_Transmit(&nested,&water,&boundary,&nd);
    boundary.sceneObjectIndex=4;boundary.boundaryEntering=false;before=RuntimeCausticPhotonMediumStack3D_Depth(&nested);
    bool mismatch_rejected=!RuntimeCameraDielectric3D_Resolve(&nested,&p,&boundary,vec3(0,0,-1),&nd);
    printf("{\"case\":\"camera_medium_mismatch\",\"rejected\":%s,\"depth_unchanged\":%s}\n",mismatch_rejected?"true":"false",before==RuntimeCausticPhotonMediumStack3D_Depth(&nested)?"true":"false");
    /* Intersection, not the test fixture, must retain raw boundary winding. */
    RuntimeTriangle3D plane={0};plane.p0=vec3(-1,-1,0);plane.p1=vec3(1,-1,0);plane.p2=vec3(0,1,0);plane.normal=vec3(0,0,1);
    Ray3D inside_ray={0};inside_ray.origin=vec3(0,0,-1);inside_ray.direction=vec3(0,0,1);HitInfo3D actual_hit;
    bool intersection=RuntimeRay3D_IntersectTriangle(&inside_ray,&plane,0,0,10,&actual_hit);
    printf("{\"case\":\"intersection_orientation\",\"passed\":%s}\n",intersection && actual_hit.hasBoundaryOrientation && !actual_hit.boundaryEntering && vec3_dot(HitInfo3D_BoundaryNormal(&actual_hit),plane.normal)>.999?"true":"false");
    RuntimePathDepthPolicy3D policy={.minDepthBeforeRoulette=2,.rouletteThreshold=.5};double q=RuntimePathDepthPolicy3D_SurvivalProbability(&policy,3,.1);double sum=0;const int n=200000;
    for(int i=0;i<n;i++) {double survival;if(!RuntimePathDepthPolicy3D_ShouldTerminate(&policy,3,.1,uniform(),&survival))sum+=.1/survival;}
    printf("{\"case\":\"roulette\",\"survival\":%.9g,\"uncompensated_mean\":0.1,\"compensated_mean\":%.9g}\n",q,sum/n);
    return 0;
}

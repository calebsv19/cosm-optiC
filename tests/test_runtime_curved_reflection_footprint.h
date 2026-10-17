/* Independent neighboring intersections and actual sampler replay. The oracle
 * does not use the stored normal field or the footprint implementation. */
static void test_runtime_curved_reflection_footprint(void) {
    RuntimeTriangle3D t={0};t.p0=vec3(-2,-2,0);t.p1=vec3(2,-2,0);t.p2=vec3(0,2,0);
    t.normal=vec3(0,0,1);t.hasVertexNormals=true;
    t.vertexNormal0=vec3_normalize(vec3(-.2,.05,1));
    t.vertexNormal1=vec3_normalize(vec3(.2,.05,1));
    t.vertexNormal2=vec3_normalize(vec3(.05,.3,1));
    Ray3D rays[3]={RuntimeRay3D_Make(vec3(0,0,2),vec3(.03,0,-1)),
        RuntimeRay3D_Make(vec3(.01,0,2),vec3(.031,0,-1)),
        RuntimeRay3D_Make(vec3(0,.01,2),vec3(.03,.001,-1))};
    HitInfo3D hits[3];
    for(int i=0;i<3;++i)assert_true("curved_oracle_intersection",RuntimeRay3D_IntersectTriangle(&rays[i],&t,0,1e-5,10,&hits[i]));
    HitInfo3D h=hits[0];h.sceneObjectIndex=-1;h.hasPixelFootprint=h.hasIncidentDifferentials=true;
    h.pixelDpDx=vec3_sub(hits[1].position,h.position);h.pixelDpDy=vec3_sub(hits[2].position,h.position);
    h.incidentDirection=rays[0].direction;h.incidentDirectionDx=rays[1].direction;h.incidentDirectionDy=rays[2].direction;
    RuntimeMaterialPayload3D p={0};p.valid=true;RuntimePrincipledBSDF3D b={0};b.valid=true;b.roughness=.02;
    b.specularF0R=.7;b.specularF0G=.8;b.specularF0B=.9;
    for(int k=0;k<32;++k) {
        double u=(k+.5)/32,v=(k+.25)/32;Vec3 view=vec3_scale(rays[0].direction,-1);
        RuntimeSpecularBSDF3DSample s[3];
        for(int i=0;i<3;++i)s[i]=RuntimeSpecularBSDF3D_Sample(&b,&hits[i],vec3_scale(rays[i].direction,-1),.25,u,v);
        Ray3D q=RuntimeRay3D_MakeOffset(h.position,h.geometricNormal,s[0].direction,1e-5),before=q;
        assert_true("curved_ggx_eligible",s[0].valid&&s[1].valid&&s[2].valid&&RuntimeSpecularBSDF3D_TransportFootprint(&p,&b,&h,view,&q));
        assert_close("curved_neighbor_x_real_sampler",vec3_length(vec3_sub(q.directionDx,s[1].direction)),0,1e-10);
        assert_close("curved_neighbor_y_real_sampler",vec3_length(vec3_sub(q.directionDy,s[2].direction)),0,1e-10);
        assert_true("curved_central_ray_exact",!memcmp(&q.origin,&before.origin,sizeof(Vec3))&&!memcmp(&q.direction,&before.direction,sizeof(Vec3)));
        RuntimeSpecularBSDF3DSample replay=RuntimeSpecularBSDF3D_Sample(&b,&hits[0],view,.25,u,v);
        assert_true("curved_pdf_throughput_exact",!memcmp(&replay,&s[0],sizeof(replay)));
    }
    Vec3 n=h.shadingNormal;Vec3 d=vec3_normalize(vec3_sub(rays[0].direction,vec3_scale(n,2*vec3_dot(rays[0].direction,n))));
    Ray3D q=RuntimeRay3D_MakeOffset(h.position,h.geometricNormal,d,1e-5),before=q;
    assert_true("curved_ideal_eligible",RuntimeRay3D_TransportReflectionFootprint(&h,n,&q));
    for(int i=1;i<3;++i) {
        Vec3 expected=vec3_normalize(vec3_sub(rays[i].direction,vec3_scale(hits[i].shadingNormal,2*vec3_dot(rays[i].direction,hits[i].shadingNormal))));
        assert_close("curved_ideal_neighbor_oracle",vec3_length(vec3_sub(i==1?q.directionDx:q.directionDy,expected)),0,1e-10);
    }
    for(int bad=0;bad<5;++bad) {
        HitInfo3D hh=h;
        switch(bad) {
            case 0:hh.hasReflectionNormalField=false;break;
            case 1:hh.reflectionNormalGradient[0].x=NAN;break;
            case 2:hh.pixelDpDx=vec3(100,0,0);break;
            case 3:hh.incidentDirectionDx=vec3(1,0,0);break;
            case 4:hh.hasUnperturbedShadingNormal=true;hh.unperturbedShadingNormal=vec3(0,0,1);break;
        }
        Ray3D r=before;assert_true("curved_invalid_broad",!RuntimeRay3D_TransportReflectionFootprint(&hh,n,&r)&&r.footprintUnbounded&&!r.hasDifferentials);
        assert_true("curved_invalid_central_exact",!memcmp(&r.direction,&before.direction,sizeof(Vec3))&&!memcmp(&r.origin,&before.origin,sizeof(Vec3)));
    }
    q=before;assert_true("glass_varying_normal_still_refused",!RuntimeRay3D_TransportIdealFootprint(&h,n,RUNTIME_RAY_IDEAL_REFRACTION,1,1.5,&q));
}

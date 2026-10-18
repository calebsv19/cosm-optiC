#pragma once
#include "render/runtime_physical_glass_3d.h"

static void test_runtime_glass_local_reflection_footprint(void) {
    RuntimeMaterialPayload3D p={0};p.valid=true;p.hasGlassTransportOverride=true;p.bsdf.roughness=0;p.transparency=1;
    HitInfo3D h={0};h.hasPixelFootprint=h.hasIncidentDifferentials=true;
    h.hasReflectionNormalField=h.hasUnperturbedShadingNormal=true;
    h.normal=h.geometricNormal=h.shadingNormal=h.reflectionNormalValue=h.unperturbedShadingNormal=vec3(0,0,1);
    h.reflectionNormalGradient[0]=vec3(-1,0,0);
    h.pixelDpDx=vec3(.08,0,0);h.pixelDpDy=vec3(0,.03,0);
    h.incidentDirection=h.incidentDirectionDx=h.incidentDirectionDy=vec3(.8,0,-.6);
    Vec3 normals[2];
    assert_true("local_glass_normal_neighbors",RuntimeRay3D_NormalFootprintNeighbors(&h,h.normal,normals));
    double choice=.99*RuntimePhysicalGlass3D_Fresnel(.6,1,1.333);
    RuntimePhysicalGlass3DSample central,neighbor;
    assert_true("local_glass_sample",RuntimePhysicalGlass3D_Sample(&p,h.normal,h.incidentDirection,1,1.333,.3,.7,choice,&central));
    assert_true("local_glass_reflected",central.reflected);
    assert_true("local_glass_neighbor_sample",RuntimePhysicalGlass3D_Sample(&p,normals[0],h.incidentDirectionDx,1,1.333,.3,.7,choice,&neighbor));
    assert_true("local_glass_finite_neighbor_crosses_branch",!neighbor.reflected);
    Ray3D next=RuntimeRay3D_Make(vec3(1,2,3),central.direction);
    next.footprintUnbounded=true;
    assert_true("local_glass_recovers_derivative",RuntimePhysicalGlass3D_TransportLocalReflectionFootprint(&p,&h,h.normal,&central,1,1.333,.3,.7,choice,&next));
    /* Analytic smooth reflection derivative: dr=di-2[(di.n+i.dn)n+(i.n)dn]. */
    Vec3 expected=vec3_normalize(vec3_add(central.direction,vec3(-.096,0,.128)));
    assert_close("local_glass_analytic_dx",vec3_length(vec3_sub(next.directionDx,expected)),0,1e-8);
    assert_close("local_glass_analytic_dy",vec3_length(vec3_sub(next.directionDy,central.direction)),0,1e-8);
    assert_close("local_glass_actual_ray_unchanged",vec3_length(vec3_sub(next.direction,central.direction)),0,0);
    assert_true("local_glass_bounded",!next.footprintUnbounded && next.hasDifferentials && next.hasDifferentialOrigins);
    assert_close("local_glass_origin_dx",next.originDx.x,1.08,1e-12);
    /* The microsurface estimator must have the same single-scatter local limit. */
    p.bsdf.roughness=.008;
    assert_true("local_glass_rough_sample",RuntimePhysicalGlass3D_Sample(&p,h.normal,h.incidentDirection,1,1.333,.3,.7,choice,&central));
    assert_true("local_glass_rough_first_reflection",central.reflected && central.scatteringOrder==1);
    next.direction=central.direction;
    double weight=central.weight,pdf=central.pdf;
    assert_true("local_glass_rough_recovers",RuntimePhysicalGlass3D_TransportLocalReflectionFootprint(&p,&h,h.normal,&central,1,1.333,.3,.7,choice,&next));
    assert_close("local_glass_rough_direction_unchanged",vec3_length(vec3_sub(next.direction,central.direction)),0,0);
    assert_close("local_glass_weight_unchanged",central.weight,weight,0);
    assert_close("local_glass_pdf_unchanged",central.pdf,pdf,0);
    p.hasMicrodetailNormal=true;
    assert_true("local_glass_microdetail_declined",!RuntimePhysicalGlass3D_TransportLocalReflectionFootprint(&p,&h,h.normal,&central,1,1.333,.3,.7,choice,&next));
    p.hasMicrodetailNormal=false;h.footprintUnbounded=true;
    assert_true("local_glass_unknown_footprint_declined",!RuntimePhysicalGlass3D_TransportLocalReflectionFootprint(&p,&h,h.normal,&central,1,1.333,.3,.7,choice,&next));
    h.footprintUnbounded=false;central.scatteringOrder=2;
    assert_true("local_glass_multiscatter_declined",!RuntimePhysicalGlass3D_TransportLocalReflectionFootprint(&p,&h,h.normal,&central,1,1.333,.3,.7,choice,&next));
}

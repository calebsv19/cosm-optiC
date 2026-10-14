#include "render/runtime_ray_3d.h"
/* Independent quaternion oracle for a coherent cone sample rotation. */
static Vec3 sampled_test_rotate(Vec3 point,Vec3 from,Vec3 to) {
    double scalar=sqrt((1+vec3_dot(from,to))/2);
    Vec3 q=vec3_scale(vec3_cross(from,to),1/(2*scalar));
    return vec3_normalize(vec3_add(vec3_scale(point,scalar*scalar-vec3_dot(q,q)),
        vec3_add(vec3_scale(q,2*vec3_dot(q,point)),vec3_scale(vec3_cross(q,point),2*scalar))));
}
static void test_runtime_sampled_footprint(void) {
    HitInfo3D hit={0};hit.position=vec3(0,0,0);
    hit.geometricNormal=hit.shadingNormal=vec3(0,0,1);
    hit.hasIncidentDifferentials=hit.hasPixelFootprint=hit.constantShadingNormal=true;
    hit.pixelDpDx=vec3(.01,0,0);hit.pixelDpDy=vec3(0,.01,0);
    hit.incidentDirection=vec3(0,0,-1);
    hit.incidentDirectionDx=vec3_normalize(vec3(.01,0,-1));
    hit.incidentDirectionDy=vec3_normalize(vec3(0,.01,-1));
    Vec3 actual=vec3_normalize(vec3(.1,.05,-1));
    Ray3D ray=RuntimeRay3D_Make(vec3(0,0,-1e-5),actual),before=ray;
    assert_true("rough_sample_footprint_transport",RuntimeRay3D_TransportSampledFootprint(
        &hit,hit.shadingNormal,RUNTIME_RAY_IDEAL_REFRACTION,1,1.5,hit.incidentDirection,&ray));
    double tx=hit.incidentDirectionDx.x/1.5;
    Vec3 snell=vec3(tx,0,-sqrt(1-tx*tx));
    Vec3 oracle=sampled_test_rotate(snell,hit.incidentDirection,actual);
    assert_close("rough_sample_quaternion_oracle",vec3_length(vec3_sub(ray.directionDx,oracle)),0,1e-10);
    assert_true("rough_sample_central_ray_unchanged",!memcmp(&before.origin,&ray.origin,sizeof(Vec3)) && !memcmp(&before.direction,&ray.direction,sizeof(Vec3)));
    assert_true("rough_sample_bounded_transported",ray.hasDifferentials && ray.hasDifferentialOrigins && ray.footprintTransported && !ray.footprintUnbounded);
    assert_close("rough_sample_neighbor_origin",vec3_length(vec3_sub(ray.originDx,vec3_add(before.origin,hit.pixelDpDx))),0,1e-12);
    hit.constantShadingNormal=false;
    assert_true("rough_sample_unknown_normal_refused",!RuntimeRay3D_TransportSampledFootprint(
        &hit,hit.shadingNormal,RUNTIME_RAY_IDEAL_REFRACTION,1,1.5,hit.incidentDirection,&ray));
    assert_true("rough_sample_unknown_normal_broad",ray.footprintUnbounded && !ray.hasDifferentials);
    hit.constantShadingNormal=true;ray=RuntimeRay3D_Make(before.origin,vec3(.1,0,1));
    assert_true("rough_sample_wrong_hemisphere_refused",!RuntimeRay3D_TransportSampledFootprint(
        &hit,hit.shadingNormal,RUNTIME_RAY_IDEAL_REFRACTION,1,1.5,hit.incidentDirection,&ray));
    ray=before;ray.direction.x=NAN;
    assert_true("rough_sample_nonfinite_refused",!RuntimeRay3D_TransportSampledFootprint(
        &hit,hit.shadingNormal,RUNTIME_RAY_IDEAL_REFRACTION,1,1.5,hit.incidentDirection,&ray));
    ray=RuntimeRay3D_Make(before.origin,vec3_normalize(vec3(.1,.05,1)));
    Vec3 reflected=vec3(0,0,1),reflected_dx=vec3_normalize(vec3(.01,0,1));
    assert_true("rough_reflection_sample_bounded",RuntimeRay3D_TransportSampledFootprint(
        &hit,hit.shadingNormal,RUNTIME_RAY_IDEAL_REFLECTION,1,1,reflected,&ray));
    assert_close("rough_reflection_sample_oracle",vec3_length(vec3_sub(ray.directionDx,
        sampled_test_rotate(reflected_dx,reflected,ray.direction))),0,1e-10);
}

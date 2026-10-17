#include "render/runtime_specular_bsdf_3d.h"
#include <math.h>
#include "render/runtime_surface_sampling.h"

static const double pi = 3.14159265358979323846;
static double clamp01(double x) { return fmax(0.0, fmin(1.0, x)); }
static double alpha_for(const RuntimePrincipledBSDF3D *bsdf) {
    return fmax(bsdf->roughness * bsdf->roughness, 1e-3);
}
static double distribution(double alpha, double nh) {
    const double a2 = alpha * alpha;
    const double denominator = nh * nh * (a2 - 1.0) + 1.0;
    return a2 / (pi * denominator * denominator);
}
static double masking(double alpha, double cosine) {
    if (!(cosine > 0.0)) return 0.0;
    return 2.0 / (1.0 + sqrt(1.0 + alpha * alpha *
        fmax(0.0, 1.0 - cosine * cosine) / (cosine * cosine)));
}
static bool valid_sides(const HitInfo3D *hit, Vec3 view, Vec3 direction, Vec3 normal) {
    Vec3 geometric = vec3_normalize(HitInfo3D_OffsetNormal(hit));
    return vec3_dot(view, normal) > 1e-9 && vec3_dot(direction, normal) > 1e-9 &&
           vec3_dot(view, geometric) > 1e-9 && vec3_dot(direction, geometric) > 1e-9;
}
static HitInfo3D reflection_hit(const HitInfo3D* hit,Vec3 view) {
    HitInfo3D oriented=*hit;
    if(vec3_dot(HitInfo3D_OffsetNormal(hit),view)<0) {
        oriented.normal=vec3_scale(oriented.normal,-1);
        oriented.geometricNormal=vec3_scale(oriented.geometricNormal,-1);
        oriented.shadingNormal=vec3_scale(oriented.shadingNormal,-1);
    }
    return oriented;
}
double RuntimeSpecularBSDF3D_Pdf(const RuntimePrincipledBSDF3D *bsdf,
    const HitInfo3D *hit, Vec3 view, Vec3 direction) {
    if (!bsdf || !bsdf->valid || !hit) return 0.0;
    view = vec3_normalize(view);
    HitInfo3D oriented=reflection_hit(hit,view);hit=&oriented;
    direction = vec3_normalize(direction);
    Vec3 normal = HitInfo3D_ShadingNormalForReflection(hit, view);
    if (!valid_sides(hit, view, direction, normal)) return 0.0;
    Vec3 half = vec3_normalize(vec3_add(view, direction));
    double nh = vec3_dot(normal, half), vh = vec3_dot(view, half);
    if (!(nh > 0.0) || !(vh > 1e-9)) return 0.0;
    return distribution(alpha_for(bsdf), nh) * nh / (4.0 * vh);
}
RuntimeSpecularBSDF3DSample RuntimeSpecularBSDF3D_Sample(
    const RuntimePrincipledBSDF3D *bsdf, const HitInfo3D *hit, Vec3 view,
    double lobe_probability, double u, double v) {
    RuntimeSpecularBSDF3DSample sample = {0};
    if (!bsdf || !bsdf->valid || !hit || !(lobe_probability > 0.0)) return sample;
    view = vec3_normalize(view);
    HitInfo3D oriented=reflection_hit(hit,view);hit=&oriented;
    Vec3 normal = HitInfo3D_ShadingNormalForReflection(hit, view);
    Vec3 axis = fabs(normal.z) < 0.999 ? vec3(0,0,1) : vec3(0,1,0);
    Vec3 tangent = vec3_normalize(vec3_cross(axis, normal));
    Vec3 bitangent = vec3_cross(normal, tangent);
    double alpha = alpha_for(bsdf);
    double phi = 2.0 * pi * clamp01(u);
    v = fmin(clamp01(v), 1.0 - 1e-12);
    double nh = sqrt((1.0 - v) / (1.0 + (alpha * alpha - 1.0) * v));
    double sine = sqrt(fmax(0.0, 1.0 - nh * nh));
    Vec3 half = vec3_normalize(vec3_add(vec3_scale(normal, nh),
        vec3_add(vec3_scale(tangent, sine * cos(phi)), vec3_scale(bitangent, sine * sin(phi)))));
    double vh = vec3_dot(view, half);
    if (!(vh > 1e-9)) return sample;
    sample.direction = vec3_normalize(vec3_sub(vec3_scale(half, 2.0 * vh), view));
    double conditional_pdf = RuntimeSpecularBSDF3D_Pdf(bsdf, hit, view, sample.direction);
    if (!(conditional_pdf > 0.0)) return sample;
    sample.pdf = lobe_probability * conditional_pdf;
    sample.cosTheta = vec3_dot(normal, sample.direction);
    double nv = vec3_dot(normal, view);
    double scale = distribution(alpha, nh) * masking(alpha, nv) * masking(alpha, sample.cosTheta) /
                   (4.0 * nv * sample.pdf);
    sample.throughputR = RuntimePrincipledBSDF3D_FresnelSchlick(vh, bsdf->specularF0R) * scale;
    sample.throughputG = RuntimePrincipledBSDF3D_FresnelSchlick(vh, bsdf->specularF0G) * scale;
    sample.throughputB = RuntimePrincipledBSDF3D_FresnelSchlick(vh, bsdf->specularF0B) * scale;
    sample.valid = isfinite(sample.throughputR) && isfinite(sample.throughputG) && isfinite(sample.throughputB);
    if (!sample.valid) return (RuntimeSpecularBSDF3DSample){0};
    return sample;
}

static bool footprint_finite(Vec3 v) {
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z) && vec3_length(v)>1e-12;
}
bool RuntimeSpecularBSDF3D_TransportFootprint(const RuntimeMaterialPayload3D *payload,
    const RuntimePrincipledBSDF3D *bsdf, const HitInfo3D *hit, Vec3 view, Ray3D *ray) {
    if (!ray) return false;
    /* Reset stale differential state on every refusal, leaving the transport ray intact. */
    (void)RuntimeRay3D_TransportIdealFootprint(NULL,vec3(0,0,1),
        RUNTIME_RAY_IDEAL_REFLECTION,1,1,ray);
    if (!payload || !payload->valid || !bsdf || !bsdf->valid || !hit ||
        payload->hasGlassTransportOverride || payload->hasMicrodetailNormal ||
        !isfinite(payload->transparency) || payload->transparency>1e-6 ||
        !isfinite(bsdf->transmissionWeight) || bsdf->transmissionWeight>1e-9 ||
        !isfinite(bsdf->roughness) || bsdf->roughness<0 || bsdf->roughness>0.08 ||
        RuntimeSurfaceSamplingNormalResponseActive(hit->sceneObjectIndex) ||
        !footprint_finite(view) || !footprint_finite(ray->direction) ||
        !footprint_finite(hit->incidentDirection)) return false;
    view=vec3_normalize(view);
    if (vec3_length(vec3_add(view,vec3_normalize(hit->incidentDirection)))>1e-8) return false;
    Vec3 normal=HitInfo3D_ShadingNormalForReflection(hit,view);
    Vec3 ideal_direction=vec3_sub(vec3_scale(normal,2*vec3_dot(view,normal)),view);
    Ray3D ideal=*ray;ideal.direction=ideal_direction;
    Vec3 neighbor_normals[2];
    if (!RuntimeRay3D_ReflectionFootprintNeighbors(hit,normal,&ideal,neighbor_normals)) return false;
    Vec3 half_sum=vec3_add(view,vec3_normalize(ray->direction));
    if (!footprint_finite(half_sum)) return false;
    Vec3 half=vec3_normalize(half_sum),geometric=vec3_normalize(hit->geometricNormal);
    if (vec3_dot(half,normal)<=1e-8 || vec3_dot(view,half)<=1e-8) return false;
    /* Hold the NDF sample's local coordinates fixed, using exactly the same
     * normal-dependent frame as Sample(). Constant normals preserve the old
     * arithmetic; varying normals carry the sample through the changing frame. */
    Vec3 axis=fabs(normal.z)<.999?vec3(0,0,1):vec3(0,1,0);
    Vec3 tangent=vec3_normalize(vec3_cross(axis,normal)),bitangent=vec3_cross(normal,tangent);
    double hn=vec3_dot(half,normal),ht=vec3_dot(half,tangent),hb=vec3_dot(half,bitangent);
    Vec3 incoming[]={hit->incidentDirection,hit->incidentDirectionDx,hit->incidentDirectionDy};
    Vec3 outgoing[3];
    for (int i=0;i<3;++i) {
        Vec3 d=vec3_normalize(incoming[i]);
        Vec3 sample_half=half,sample_normal=normal;
        if(i>0 && !hit->constantShadingNormal) {
            sample_normal=neighbor_normals[i-1];
            /* The sampler basis has a discontinuity at this axis switch. */
            if((fabs(sample_normal.z)<.999)!=(fabs(normal.z)<.999))return false;
            Vec3 t=vec3_normalize(vec3_cross(axis,sample_normal)),b=vec3_cross(sample_normal,t);
            sample_half=vec3_normalize(vec3_add(vec3_scale(sample_normal,hn),
                vec3_add(vec3_scale(t,ht),vec3_scale(b,hb))));
        }
        if (vec3_dot(d,sample_half)>=-1e-8) return false;
        outgoing[i]=vec3_normalize(vec3_sub(d,vec3_scale(sample_half,2*vec3_dot(d,sample_half))));
        if (!footprint_finite(outgoing[i]) || vec3_dot(outgoing[i],geometric)<=1e-8 ||
            vec3_dot(outgoing[i],sample_normal)<=1e-8) return false;
    }
    if (vec3_length(vec3_sub(outgoing[0],vec3_normalize(ray->direction)))>1e-8) return false;
    ray->originDx=ideal.originDx;ray->originDy=ideal.originDy;
    ray->directionDx=outgoing[1];ray->directionDy=outgoing[2];
    ray->hasDifferentials=ray->hasDifferentialOrigins=ray->footprintTransported=true;
    ray->footprintUnbounded=false;
    return true;
}

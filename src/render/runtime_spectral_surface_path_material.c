#include "runtime_spectral_surface_path_internal.h"

bool spectral_path_material(const RuntimeSpectralSurfacePath *context,
    const HitInfo3D *hit, double nm, PathMaterial *out) {
    const RuntimeSpectralSurfacePathConfig *p=&context->profile->surface_path;
    PathMaterial m={0};
    m.ggx=RuntimePrincipledBSDF3D_Default();
    m.ggx.valid=true;
    if (p->native_materials) {
        RuntimeMaterialPayload3D payload;
        if (!RuntimeMaterialPayload3D_ResolveFromHit(hit,&payload) || !payload.valid ||
            payload.transparency>0 || payload.emissive>0 || payload.hasGlassTransportOverride ||
            payload.hasMicrodetailNormal) return false;
        RuntimePrincipledBSDF3D native=RuntimePrincipledBSDF3D_FromMaterialPayload(&payload);
        if (native.transmissionWeight>0) return false;
        m.rho=RuntimeSpectral_RGBBasis(vec3(payload.baseColorR,payload.baseColorG,payload.baseColorB),nm);
        m.diffuse=native.diffuseWeight; m.specular=native.specularWeight;
        m.ggx.roughness=native.roughness;
        double f0=m.specular>0 ? RuntimeSpectral_RGBBasis(vec3(native.specularF0R,native.specularF0G,native.specularF0B),nm)/m.specular : 0;
        m.ggx.specularF0R=m.ggx.specularF0G=m.ggx.specularF0B=fmin(1,fmax(0,f0));
    } else {
        const RuntimeSpectralPathMaterial *material=&p->materials[context->mapping[hit->primitiveIndex]];
        m.rho=RuntimeSpectralCurve_Value(&material->reflectance,nm);
        m.diffuse=material->ggx ? 0 : 1; m.specular=material->ggx ? 1 : 0;
        m.ggx.roughness=material->roughness;
        m.ggx.specularF0R=m.ggx.specularF0G=m.ggx.specularF0B=m.rho;
        m.two_sided=material->two_sided;
    }
    double sum=m.diffuse+m.specular;
    m.probability=sum>0 ? m.specular/sum : 0;
    *out=m;
    return true;
}

double spectral_path_material_eval(const PathMaterial *m,const HitInfo3D *hit,
    Vec3 view,Vec3 direction,double *pdf) {
    double cosine=fmax(0,vec3_dot(hit->normal,direction));
    double f=m->diffuse*m->rho*cosine/M_PI;
    *pdf=(1-m->probability)*cosine/M_PI;
    if (m->specular>0) {
        f+=m->specular*RuntimeSpecularBSDF3D_EvaluateCos(&m->ggx,hit,view,direction).x;
        *pdf+=m->probability*RuntimeSpecularBSDF3D_Pdf(&m->ggx,hit,view,direction);
    }
    return f;
}

bool spectral_path_material_sample(const PathMaterial *m,const HitInfo3D *hit,
    Vec3 view,PathRandom *rng,Vec3 *direction,double *pdf,double *weight) {
    bool specular=m->probability==1 || (m->probability>0 && spectral_path_random(rng)<m->probability);
    double u=spectral_path_random(rng),v=spectral_path_random(rng);
    if (specular) {
        RuntimeSpecularBSDF3DSample sample=RuntimeSpecularBSDF3D_Sample(&m->ggx,hit,view,1,u,v);
        if (!sample.valid) return false;
        *direction=sample.direction;
        if (m->probability==1) { *pdf=sample.pdf; *weight=m->specular*sample.throughputR; return true; }
    } else {
        *direction=spectral_path_direction(hit->normal,sqrt(u),2*M_PI*v);
        if (m->probability==0) { *pdf=fmax(0,vec3_dot(hit->normal,*direction))/M_PI; *weight=m->diffuse*m->rho; return true; }
    }
    double f=spectral_path_material_eval(m,hit,view,*direction,pdf);
    *weight=*pdf>0 ? f/ *pdf : 0;
    return *pdf>0 && isfinite(*weight);
}

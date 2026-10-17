#include "render/runtime_ray_3d.h"
#include <math.h>

static bool finite_vector(Vec3 v) {
    return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
}
static bool direction_valid(Vec3 v) {
    return finite_vector(v) && vec3_length(v)>1e-12;
}
static Vec3 reflect(Vec3 d,Vec3 n) {
    return vec3_normalize(vec3_sub(d,vec3_scale(n,2*vec3_dot(d,n))));
}

void RuntimeRay3D_SetReflectionNormalField(const RuntimeTriangle3D* t,HitInfo3D* h) {
    h->hasReflectionNormalField=false;
    if (!t || !t->hasVertexNormals || h->constantShadingNormal) return;
    Vec3 a=t->vertexNormal0,b=t->vertexNormal1,c=t->vertexNormal2;
    if(!direction_valid(a)||!direction_valid(b)||!direction_valid(c))return;
    Vec3 e1=vec3_sub(t->p1,t->p0),e2=vec3_sub(t->p2,t->p0);
    Vec3 cross=vec3_cross(e1,e2);double area2=vec3_dot(cross,cross);
    if(!isfinite(area2)||area2<=1e-24)return;
    /* Gradients of barycentric coordinates in the world triangle plane. This
     * is independent of UV charts, object type, winding and import provenance. */
    Vec3 gv=vec3_scale(vec3_cross(e2,cross),1/area2);
    Vec3 gw=vec3_scale(vec3_cross(cross,e1),1/area2);
    Vec3 raw=vec3_add(vec3_add(vec3_scale(a,h->baryU),vec3_scale(b,h->baryV)),vec3_scale(c,h->baryW));
    if(!direction_valid(raw))return;
    double sign=vec3_dot(raw,h->shadingNormal)<0?-1:1;
    h->reflectionNormalValue=vec3_scale(raw,sign);
    Vec3 db=vec3_sub(b,a),dc=vec3_sub(c,a);
    double v[]={gv.x,gv.y,gv.z},w[]={gw.x,gw.y,gw.z};
    for(int i=0;i<3;++i) {
        h->reflectionNormalGradient[i]=vec3_scale(vec3_add(vec3_scale(db,v[i]),vec3_scale(dc,w[i])),sign);
        if(!finite_vector(h->reflectionNormalGradient[i]))return;
    }
    h->hasReflectionNormalField=true;
}

bool RuntimeRay3D_ReflectionFootprintNeighbors(const HitInfo3D* h,Vec3 n,Ray3D* ray,Vec3 normals[2]) {
    if(!ray||!normals)return false;
    if(h && h->constantShadingNormal) {
        if(!RuntimeRay3D_TransportIdealFootprint(h,n,RUNTIME_RAY_IDEAL_REFLECTION,1,1,ray))return false;
        normals[0]=normals[1]=vec3_normalize(n);return true;
    }
    (void)RuntimeRay3D_TransportIdealFootprint(NULL,n,RUNTIME_RAY_IDEAL_REFLECTION,1,1,ray);
    if(!h || !h->hasReflectionNormalField || !h->hasPixelFootprint ||
       !h->hasIncidentDifferentials || h->footprintUnbounded || h->hasCurveTangent ||
       !direction_valid(n) || !direction_valid(h->geometricNormal) ||
       !finite_vector(h->pixelDpDx) || !finite_vector(h->pixelDpDy) ||
       !finite_vector(ray->origin) || !direction_valid(ray->direction))return false;
    n=vec3_normalize(n);Vec3 ng=vec3_normalize(h->geometricNormal);
    /* Do not differentiate a clamped/perturbed normal or a shading seam. */
    if(!direction_valid(h->reflectionNormalValue) ||
       vec3_length(vec3_sub(n,vec3_normalize(h->reflectionNormalValue)))>1e-8 ||
       (h->hasUnperturbedShadingNormal && vec3_length(vec3_sub(n,vec3_normalize(h->unperturbedShadingNormal)))>1e-8) ||
       !direction_valid(h->incidentDirection) ||
       vec3_dot(vec3_normalize(h->incidentDirection),ng)>=-1e-8 ||
       vec3_length(vec3_sub(reflect(vec3_normalize(h->incidentDirection),n),vec3_normalize(ray->direction)))>1e-8)return false;
    Vec3 dp[]={h->pixelDpDx,h->pixelDpDy},incoming[]={h->incidentDirectionDx,h->incidentDirectionDy},out[2],origins[2];
    for(int j=0;j<2;++j) {
        Vec3 raw=h->reflectionNormalValue;double d[]={dp[j].x,dp[j].y,dp[j].z};
        for(int i=0;i<3;++i) {
            if(!finite_vector(h->reflectionNormalGradient[i]))return false;
            raw=vec3_add(raw,vec3_scale(h->reflectionNormalGradient[i],d[i]));
        }
        if(!direction_valid(raw)||!direction_valid(incoming[j]))return false;
        normals[j]=vec3_normalize(raw);Vec3 incident=vec3_normalize(incoming[j]);
        if(vec3_dot(normals[j],n)<.9 || vec3_dot(normals[j],ng)<=1e-8 ||
           vec3_dot(incident,ng)>=-1e-8 || vec3_dot(incident,normals[j])>=-1e-8)return false;
        out[j]=reflect(incident,normals[j]);origins[j]=vec3_add(ray->origin,dp[j]);
        if(!direction_valid(out[j])||vec3_dot(out[j],ng)<=1e-8||!finite_vector(origins[j]))return false;
    }
    ray->originDx=origins[0];ray->originDy=origins[1];ray->directionDx=out[0];ray->directionDy=out[1];
    ray->hasDifferentials=ray->hasDifferentialOrigins=ray->footprintTransported=true;
    ray->footprintUnbounded=false;return true;
}

bool RuntimeRay3D_TransportReflectionFootprint(const HitInfo3D* h,Vec3 n,Ray3D* ray) {
    Vec3 normals[2];return RuntimeRay3D_ReflectionFootprintNeighbors(h,n,ray,normals);
}

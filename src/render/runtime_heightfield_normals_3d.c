#include "render/runtime_heightfield_normals_3d.h"
#include <math.h>
#include <stddef.h>

static Vec3 vertex_normal(const float* heights, unsigned w, unsigned d,
    unsigned x, unsigned z, double sx, double sz, bool z_up) {
    unsigned left=x?x-1:x, right=x+1<w?x+1:x;
    unsigned back=z?z-1:z, front=z+1<d?z+1:z;
    double dx=((double)heights[(size_t)z*w+right]-heights[(size_t)z*w+left])/((right-left)*sx);
    double dz=((double)heights[(size_t)front*w+x]-heights[(size_t)back*w+x])/((front-back)*sz);
    double length=hypot(hypot(dx,dz),1);
    return vec3_scale(z_up?vec3(-dx,-dz,1):vec3(-dx,1,-dz),1/length);
}

bool RuntimeHeightfieldNormals3D_ApplyTopQuads(RuntimeTriangle3D* triangles,
    const float* heights, unsigned w, unsigned d, double sx, double sz, bool z_up) {
    if(!triangles || !heights || w<2 || d<2 || !(sx>0) || !(sz>0) ||
       !isfinite(sx) || !isfinite(sz)) return false;
    for(size_t i=0;i<(size_t)w*d;++i)if(!isfinite(heights[i]))return false;
    for(unsigned z=0;z+1<d;++z)for(unsigned x=0;x+1<w;++x) {
        Vec3 a=vertex_normal(heights,w,d,x,z,sx,sz,z_up);
        Vec3 b=vertex_normal(heights,w,d,x,z+1,sx,sz,z_up);
        Vec3 c=vertex_normal(heights,w,d,x+1,z+1,sx,sz,z_up);
        Vec3 e=vertex_normal(heights,w,d,x+1,z,sx,sz,z_up);
        if (!isfinite(a.x+a.y+a.z+b.x+b.y+b.z+c.x+c.y+c.z+e.x+e.y+e.z)) return false;
        size_t i=2*((size_t)z*(w-1)+x);
        triangles[i].hasVertexNormals=triangles[i+1].hasVertexNormals=true;
        triangles[i].vertexNormal0=a;triangles[i].vertexNormal1=z_up?c:b;triangles[i].vertexNormal2=z_up?b:c;
        triangles[i+1].vertexNormal0=a;triangles[i+1].vertexNormal1=z_up?e:c;triangles[i+1].vertexNormal2=z_up?c:e;
    }
    return true;
}

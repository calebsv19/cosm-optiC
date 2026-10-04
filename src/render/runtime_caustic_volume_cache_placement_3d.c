#include "runtime_caustic_volume_cache_placement_3d.h"
#include "render/runtime_volume_3d_transform.h"

void RuntimeCausticVolumeCache3D_SetPlacementFromVolume(RuntimeCausticVolumeCache3D* c,
    const RuntimeVolumeAttachment3D* v) {
    if(!c) return;
    RuntimeVolumeAttachment3D_Init(&c->coordinates);
    c->coordinates.grid=c->grid;
    if(v) {
        c->coordinates.hasPlacement=v->hasPlacement;
        c->coordinates.placementTranslation=v->placementTranslation;
        c->coordinates.placementScale=v->placementScale;
        for(int i=0;i<3;++i) c->coordinates.placementBasis[i]=v->placementBasis[i];
    }
}
Vec3 runtime_caustic_cache_to_local(const RuntimeCausticVolumeCache3D* c,Vec3 p) {
    return RuntimeVolume3D_PlacementIsIdentity(&c->coordinates)?p:RuntimeVolume3D_ToCachePoint(&c->coordinates,p);
}
Vec3 runtime_caustic_cache_to_world(const RuntimeCausticVolumeCache3D* c,Vec3 p) {
    return RuntimeVolume3D_ToWorldPoint(&c->coordinates,p);
}
double runtime_caustic_cache_world_voxel(const RuntimeCausticVolumeCache3D* c) {
    return c->coordinates.hasPlacement?RuntimeVolume3D_WorldVoxelSize(&c->coordinates):c->grid.voxelSize;
}
/* Kernel radii are world lengths. Express cell displacement in units of the
 * minimum world voxel, preserving spherical/axial kernels under XYZ scaling. */
Vec3 runtime_caustic_cache_voxel_displacement(const RuntimeCausticVolumeCache3D* c,Vec3 p) {
    if(RuntimeVolume3D_PlacementIsIdentity(&c->coordinates)) return p;
    return vec3_scale(RuntimeVolume3D_ToWorldDirection(&c->coordinates,vec3_scale(p,c->grid.voxelSize)),
        1.0/runtime_caustic_cache_world_voxel(c));
}

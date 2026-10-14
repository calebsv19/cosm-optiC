#include "render/runtime_volume_3d_integrate.h"
#include "render/runtime_volume_3d_scatter.h"

void RuntimeVolume3D_CaptureOptics(RuntimeVolumeAttachment3D* attachment) {
    if(!attachment) return;
    attachment->optics.material=RuntimeVolume3DMaterial_GetTuning();
    attachment->optics.scatter=RuntimeVolume3DScatter_GetTuning();
    attachment->opticsCaptured=true;
    attachment->grid.cellCentered=attachment->optics.material.cell_centered;
}
RuntimeVolume3DMaterialTuning RuntimeVolume3DMaterial_ForAttachment(const RuntimeVolumeAttachment3D* attachment) {
    return attachment && attachment->opticsCaptured ? attachment->optics.material : RuntimeVolume3DMaterial_GetTuning();
}
RuntimeVolume3DScatterTuning RuntimeVolume3DScatter_ForAttachment(const RuntimeVolumeAttachment3D* attachment) {
    return attachment && attachment->opticsCaptured ? attachment->optics.scatter : RuntimeVolume3DScatter_GetTuning();
}

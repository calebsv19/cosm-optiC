#include "editor/scene_editor_object_transform_preview.h"
#include <math.h>
#include "editor/scene_editor_mesh_preview_store.h"

static void rotate_axis(double v[3], int axis, double radians) {
    int a=(axis+1)%3, b=(axis+2)%3;
    double va=v[a], vb=v[b], c=cos(radians), s=sin(radians);
    v[a]=va*c-vb*s; v[b]=va*s+vb*c;
}
static void rotate_basis(double* x, double* y, double* z,
                          const SceneEditorDocumentTransform* original,
                          const SceneEditorDocumentTransform* preview) {
    const double radians=0.017453292519943295769;
    double v[3]={*x,*y,*z};
    for (int i=2;i>=0;--i) rotate_axis(v,i,-original->rotation_degrees[i]*radians);
    for (int i=0;i<3;++i) rotate_axis(v,i,preview->rotation_degrees[i]*radians);
    *x=v[0]; *y=v[1]; *z=v[2];
}
bool SceneEditorObjectTransformPreviewMesh(const RayTracingRuntimeMeshAssetInstance* source,
                                          RayTracingRuntimeMeshAssetInstance* display) {
    SceneEditorDocumentTransform original,preview;
    if (!source || !display) return false;
    *display=*source;
    if (SceneEditorObjectTransformModeGet()==SCENE_EDITOR_OBJECT_TRANSFORM_MOVE ||
        !SceneEditorObjectTransformPreview(source->scene_object_index,&original,&preview)) return false;
    display->rotation_x=preview.rotation_degrees[0]*0.017453292519943295769;
    display->rotation_y=preview.rotation_degrees[1]*0.017453292519943295769;
    display->rotation_z=preview.rotation_degrees[2]*0.017453292519943295769;
    display->scale_x*=preview.scale[0]/original.scale[0];
    display->scale_y*=preview.scale[1]/original.scale[1];
    display->scale_z*=preview.scale[2]/original.scale[2];
    return true;
}
bool SceneEditorObjectTransformPreviewPrimitive(const RuntimeSceneBridgePrimitiveSeed* source,
                                               RuntimeSceneBridgePrimitiveSeed* display) {
    SceneEditorDocumentTransform original,preview;
    if (!source || !display) return false;
    *display=*source;
    if (SceneEditorObjectTransformModeGet()==SCENE_EDITOR_OBJECT_TRANSFORM_MOVE ||
        !SceneEditorObjectTransformPreview(source->scene_object_index,&original,&preview)) return false;
    rotate_basis(&display->axis_u_x,&display->axis_u_y,&display->axis_u_z,&original,&preview);
    rotate_basis(&display->axis_v_x,&display->axis_v_y,&display->axis_v_z,&original,&preview);
    rotate_basis(&display->normal_x,&display->normal_y,&display->normal_z,&original,&preview);
    display->width*=preview.scale[0]/original.scale[0];
    display->height*=preview.scale[1]/original.scale[1];
    display->depth*=preview.scale[2]/original.scale[2];
    return true;
}

void SceneEditorObjectTransformHandleOrigin(int object_index,SceneEditorObjectTransformMode mode,
    const double fallback[3],double position[3]) {
    for (int i=0;i<3;++i) position[i]=fallback[i]*SceneEditorDocumentWorldScale();
    if (mode==SCENE_EDITOR_OBJECT_TRANSFORM_MOVE) return;
    for (int i=0;i<SceneEditorMeshPreviewStoreInstanceCount();++i) {
        const RayTracingRuntimeMeshAssetInstance* mesh=SceneEditorMeshPreviewStoreGetInstance(i);
        if (!mesh || mesh->scene_object_index!=object_index) continue;
        position[0]=mesh->position_x;position[1]=mesh->position_y;position[2]=mesh->position_z;
        if (mode!=SCENE_EDITOR_OBJECT_TRANSFORM_ROTATE) return;
        double pivot[3]={0};
        if (mesh->rotation_pivot_policy==RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_CUSTOM) {
            pivot[0]=mesh->rotation_pivot_x;pivot[1]=mesh->rotation_pivot_y;pivot[2]=mesh->rotation_pivot_z;
        } else if (mesh->rotation_pivot_policy==RAY_TRACING_RUNTIME_MESH_ROTATION_PIVOT_BOUNDS_CENTER) {
            const CoreMeshAssetRuntimeContract* contract=SceneEditorMeshPreviewStoreGetContract(mesh->asset_index);
            if (contract) {
                pivot[0]=(contract->local_bounds.min.x+contract->local_bounds.max.x)*0.5;
                pivot[1]=(contract->local_bounds.min.y+contract->local_bounds.max.y)*0.5;
                pivot[2]=(contract->local_bounds.min.z+contract->local_bounds.max.z)*0.5;
            }
        }
        position[0]+=pivot[0]*mesh->scale_x;position[1]+=pivot[1]*mesh->scale_y;position[2]+=pivot[2]*mesh->scale_z;
        return;
    }
    RuntimeSceneBridge3DPrimitiveSeedState seeds={0};
    runtime_scene_bridge_get_last_3d_primitive_seed_state(&seeds);
    for (int i=0;seeds.valid && i<seeds.primitive_count;++i) if (seeds.primitives[i].scene_object_index==object_index) {
        position[0]=seeds.primitives[i].origin_x;position[1]=seeds.primitives[i].origin_y;position[2]=seeds.primitives[i].origin_z;return;
    }
}

/* Fixture-sized pixel/geometry proof through the ordinary request and evaluated
 * scene paths. No solver, pose edits, display capture or material substitution. */
#include "app/evaluated_scene_service.h"
#include "config/config_manager.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_native_3d_render.h"
#include "tools/ray_tracing_render_headless_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 3) return 2; /* request.json, fresh output prefix */
    LoadAllSettings();
    RayTracingAgentRenderRequest request = {0};
    char diagnostic[1024] = {0};
    if (!ray_tracing_agent_render_request_load_file(argv[1], &request,
            diagnostic, sizeof(diagnostic))) return 3;
    if (request.width <= 0 || request.height <= 0 || request.width > 640 ||
        request.height > 480 || request.frame_count != 1) return 4;
    RuntimeSceneBridgePreflight admitted = {0};
    if (!runtime_scene_bridge_apply_file(request.runtime_scene_path, &admitted)) return 5;
    ray_tracing_headless_apply_inspection_overrides(&request);
    /* Both routes must have their acceleration ready before the comparison. */
    RuntimeRay3D_SetTraceRouteForTests(RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS_PARITY);
    RayEvaluatedSceneServiceResult sample = {0};
    if (!RayEvaluatedSceneCaptureSample((TimelineSample){request.start_frame,0,1}, &sample) ||
        !ray_tracing_headless_apply_inspection_evaluated_camera(&request,&sample)) return 6;
    RuntimeNative3DPreparedFrame prepared = {0};
    if (!RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(&prepared,
            request.width,request.height,&sample.snapshot,NULL)) return 7;
    const RuntimeScene3D *scene = prepared.traceScene ? prepared.traceScene : &prepared.scene;
    const size_t count = (size_t)request.width * request.height;
    int *ids = malloc(count*sizeof(*ids));
    double *depths = malloc(count*sizeof(*depths));
    if (!ids || !depths) return 8;
    size_t mismatches = 0;
    for (int route=0;route<2;++route) {
        RuntimeRay3D_SetTraceRouteForTests(route ? RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS
                                               : RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH);
        for (int y=0;y<request.height;++y) for (int x=0;x<request.width;++x) {
            const size_t i=(size_t)y*request.width+x;
            Ray3D ray=RuntimeCameraProjector3D_MakePrimaryRay(&prepared.projector,x,y);
            HitInfo3D hit={0};
            bool found=RuntimeRay3D_TraceSceneFirstHit(scene,&ray,prepared.projector.nearPlane,HUGE_VAL,&hit);
            int id=found ? hit.sceneObjectIndex : -1;
            double depth=found ? hit.t : -1;
            if (!route) { ids[i]=id;depths[i]=depth; }
            else if (ids[i]!=id || (found && fabs(depths[i]-depth)>2e-9)) ++mismatches;
        }
    }
    char path[4096];
    if (snprintf(path,sizeof(path),"%s.ids",argv[2]) >= (int)sizeof(path)) return 9;
    FILE *file=fopen(path,"wb");
    if (!file || fwrite(ids,sizeof(*ids),count,file)!=count || fclose(file)) return 10;
    snprintf(path,sizeof(path),"%s.depths",argv[2]);file=fopen(path,"wb");
    if (!file || fwrite(depths,sizeof(*depths),count,file)!=count || fclose(file)) return 11;
    printf("CONTACT {\"width\":%d,\"height\":%d,\"frame\":%d,\"route_comparisons\":%zu,\"route_mismatches\":%zu,\"native_int_bytes\":%zu,\"native_double_bytes\":%zu}\n",
           request.width,request.height,request.start_frame,count,mismatches,sizeof(int),sizeof(double));
    free(ids);free(depths);RuntimeNative3DPreparedFrame_Free(&prepared);
    return mismatches ? 12 : 0;
}

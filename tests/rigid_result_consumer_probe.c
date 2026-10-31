/* BR4 uses the same evaluated service and prepared-frame function as Preview
 * and the ordinary headless CLI. No Ball code or numerical solver is linked. */
#include "app/evaluated_scene_service.h"
#include "config/config_manager.h"
#include "import/runtime_rigid_result.h"
#include "import/runtime_scene_bridge.h"
#include "render/runtime_native_3d_render.h"
#include "render/runtime_ray_3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static json_object *vector(double x, double y, double z) {
  json_object *a = json_object_new_array();
  json_object_array_add(a, json_object_new_double(x));
  json_object_array_add(a, json_object_new_double(y));
  json_object_array_add(a, json_object_new_double(z));
  return a;
}
int main(int argc, char **argv) {
  if (argc < 2)
    return 2;
  LoadAllSettings();
  RuntimeSceneBridgePreflight summary = {0};
  if (!runtime_scene_bridge_apply_file(argv[1], &summary)) {
    fprintf(stderr, "admission: %s\n", summary.diagnostics);
    return 3;
  }
  if (!RuntimeRigidResultActive())
    return 4;
  if(argc==3&&!strcmp(argv[2],"--inspection")) {
    json_object *j=RuntimeRigidResultInspection();if(!j)return 15;
    printf("BR4 %s\n",json_object_to_json_string_ext(j,JSON_C_TO_STRING_PLAIN));
    json_object_put(j);RuntimeRigidResultCommit(NULL);return 0;
  }
  if(argc==4&&!strcmp(argv[2],"--sequence-save")) {
    json_object *original=json_object_from_file(argv[1]);if(!original)return 30;
    const char *text=json_object_to_json_string_ext(original,JSON_C_TO_STRING_PLAIN);
    json_object *ext=NULL,*source_ray=NULL,*author=NULL;
    json_object_object_get_ex(original,"extensions",&ext);json_object_object_get_ex(ext,"ray_tracing",&source_ray);
    json_object *ray=json_tokener_parse(json_object_to_json_string_ext(source_ray,JSON_C_TO_STRING_PLAIN));
    json_object *overlay=json_object_new_object(),*overlay_ext=json_object_new_object(),*meta=json_object_new_object();
    json_object_object_add(meta,"producer",json_object_new_string("ray_tracing"));
    json_object_object_add(meta,"logical_clock",json_object_new_int(1));
    json_object_object_add(overlay,"overlay_meta",meta);
    json_object_object_add(overlay_ext,"ray_tracing",ray);json_object_object_add(overlay,"extensions",overlay_ext);
    json_object_object_get_ex(ray,"authoring",&author);json_object_object_del(author,"sequence");json_object_object_del(author,"camera_rig");
    char *saved=NULL,diagnostic[512];
    bool ok=runtime_scene_bridge_writeback_ray_overlay_json(text,json_object_to_json_string_ext(overlay,JSON_C_TO_STRING_PLAIN),&saved,diagnostic,sizeof(diagnostic));
    if(!ok){fprintf(stderr,"save overlay: %s\n",diagnostic);return 31;}
    FILE *file=fopen(argv[3],"wx");if(!file)return 32;
    fputs(saved,file);fclose(file);free(saved);json_object_put(overlay);json_object_put(original);
    RuntimeRigidResultCommit(NULL);puts("BR4 {\"saved\":true}");return 0;
  }
  TimelineRate rate;
  TimelineRange range;
  if (RayEvaluatedSceneResolveTimelineClock(&rate, &range) !=
      TIMELINE_STATUS_OK)
    return 5;
  RuntimeRay3D_SetTraceRouteForTests(
      RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS_PARITY);
  animSettings.spaceMode = SPACE_MODE_3D;
  animSettings.causticMode3D = 0;
  if (argc > 3 && !strcmp(argv[2], "--replacement")) {
    RayEvaluatedSceneServiceResult before = {0}, after = {0};
    if (!RayEvaluatedSceneCaptureSample(
            (TimelineSample){range.start_frame + 1, 0, 1}, &before))
      return 6;
    bool ok = runtime_scene_bridge_apply_file(argv[3], &summary);
    if (ok || !RuntimeRigidResultActive() ||
        !RayEvaluatedSceneCaptureSample(
            (TimelineSample){range.start_frame + 1, 0, 1}, &after) ||
        memcmp(&before.snapshot.camera,&after.snapshot.camera,sizeof(RayEvaluatedCamera)) ||
        memcmp(before.snapshot.object_transforms,
               after.snapshot.object_transforms,
               before.snapshot.object_transform_count *
                   sizeof(RayEvaluatedObjectTransform)))
      return 7;
    puts("BR4 {\"rollback\":true}");
    RuntimeRigidResultCommit(NULL);
    return 0;
  }
  for (int arg = 2; arg < argc; arg++) {
    int frame = atoi(argv[arg]);
    RayEvaluatedSceneServiceResult sample = {0}, preview = {0};
    if (!RayEvaluatedSceneCaptureSample(
            (TimelineSample){range.start_frame + frame, 0, 1}, &sample)) {
      fprintf(stderr, "sample: %s\n", sample.status_line);
      return 8;
    }
    if (!RayEvaluatedSceneCaptureForElapsed(
            (double)frame * rate.frames_per_second_denominator /
                rate.frames_per_second_numerator,
            &preview) ||
        sample.snapshot.object_transform_count !=
            preview.snapshot.object_transform_count)
      return 9;
    for (size_t i = 0; i < sample.snapshot.object_transform_count; i++) {
      const RayEvaluatedObjectTransform
          *a = &sample.snapshot.object_transforms[i],
          *b = &preview.snapshot.object_transforms[i];
      if (a->source_tick != b->source_tick ||
          memcmp(&a->position, &b->position, sizeof(a->position)) ||
          memcmp(&a->orientation_quaternion, &b->orientation_quaternion,
                 sizeof(a->orientation_quaternion)))
        return 10;
    }
    RuntimeNative3DPreparedFrame prepared = {0};
    if (!RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(
            &prepared, 16, 16, &sample.snapshot, NULL)) {
      fprintf(stderr, "prepare: %s\n",
              RuntimeNative3DPrepareFrameLastDiagnostics());
      return 11;
    }
    if (arg == 2) {
      RayEvaluatedSceneSnapshot forged = sample.snapshot;
      forged.object_transforms[0].position.x += 0.01;
      RuntimeNative3DPreparedFrame refused = {0};
      if (RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(
              &refused, 16, 16, &forged, NULL))
        return 13;
      RuntimeNative3DPreparedFrame_Free(&refused);
      if(sample.snapshot.camera.inspection_id[0]){
        forged=sample.snapshot;forged.camera.inspection_time.numerator++;
        if(RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(&refused,16,16,&forged,NULL))return 35;
        RuntimeNative3DPreparedFrame_Free(&refused);
      }
      if(sample.snapshot.has_sequence) {
        forged=sample.snapshot;forged.sequence.simulation_time.numerator++;
        if(RuntimeNative3DPrepareFrameWithSamplingForEvaluatedScene(&refused,16,16,&forged,NULL))return 34;
        RuntimeNative3DPreparedFrame_Free(&refused);
      }

    }
    json_object *out = json_object_new_object(),
                *poses = json_object_new_array(),
                *triangles = json_object_new_array();
    json_object_object_add(out, "frame", json_object_new_int(frame));
    json_object_object_add(out, "poses", poses);
    json_object_object_add(out,"camera",vector(sample.snapshot.camera.position.x,sample.snapshot.camera.position.y,sample.snapshot.camera.position.z));
    json_object_object_add(out,"camera_id",json_object_new_string(sample.snapshot.camera.target_id));
    json_object_object_add(out,"inspection_id",json_object_new_string(sample.snapshot.camera.inspection_id));
    json_object *it=json_object_new_array();json_object_array_add(it,json_object_new_uint64(sample.snapshot.camera.inspection_time.numerator));json_object_array_add(it,json_object_new_uint64(sample.snapshot.camera.inspection_time.denominator));json_object_object_add(out,"inspection_time",it);
    if(sample.snapshot.has_sequence) {
      const SequenceEvaluation *v=&sample.snapshot.sequence;
      json_object_object_add(out,"source_tick",json_object_new_int64(v->source_tick));
      json_object_object_add(out,"presentation_seconds",json_object_new_double((double)v->presentation_time.numerator/v->presentation_time.denominator));
      if(!preview.snapshot.has_sequence || !SequenceClockEvaluationsEqual(&sample.snapshot.sequence,&preview.snapshot.sequence))return 33;
    }

    json_object_object_add(out, "triangles", triangles);
    for (size_t i = 0; i < sample.snapshot.object_transform_count; i++) {
      const RayEvaluatedObjectTransform *t =
          &sample.snapshot.object_transforms[i];
      json_object *o = json_object_new_object(), *q = json_object_new_array();
      json_object_object_add(o, "id", json_object_new_string(t->target_id));
      json_object_object_add(o, "tick",
                             json_object_new_int64((int64_t)t->source_tick));
      json_object_object_add(o, "body_id",
                             json_object_new_int64(t->rigid_body_id));
      json_object_object_add(o, "revision",
                             json_object_new_string(t->rigid_result_revision));
      json_object_object_add(
          o, "position", vector(t->position.x, t->position.y, t->position.z));
      json_object_array_add(
          q, json_object_new_double(t->orientation_quaternion.w));
      json_object_array_add(
          q, json_object_new_double(t->orientation_quaternion.x));
      json_object_array_add(
          q, json_object_new_double(t->orientation_quaternion.y));
      json_object_array_add(
          q, json_object_new_double(t->orientation_quaternion.z));
      json_object_object_add(o, "q", q);
      json_object_array_add(poses, o);
    }
    const RuntimeScene3D *scene =
        prepared.traceScene ? prepared.traceScene : &prepared.scene;
    for (int i = 0; i < scene->triangleMesh.triangleCount; i++) {
      const RuntimeTriangle3D *t = &scene->triangleMesh.triangles[i];
      if (t->primitiveIndex < 0 || t->primitiveIndex >= scene->primitiveCount)
        return 12;
      json_object *o = json_object_new_object(),
                  *vertices = json_object_new_array();
      json_object_object_add(
          o, "id",
          json_object_new_string(
              scene->primitives[t->primitiveIndex].source.objectId));
      json_object_object_add(o, "local",
                             json_object_new_int(t->localTriangleIndex));
      json_object_array_add(vertices, vector(t->p0.x, t->p0.y, t->p0.z));
      json_object_array_add(vertices, vector(t->p1.x, t->p1.y, t->p1.z));
      json_object_array_add(vertices, vector(t->p2.x, t->p2.y, t->p2.z));
      json_object_object_add(o, "vertices", vertices);
      json_object_array_add(triangles, o);
    }
    /* Concave bodies can have COM in empty space. Probe every visible physical
     * body from just inside one of its actual prepared triangle faces. */
    for(size_t body=0;body<sample.snapshot.object_transform_count;++body) {
      const RayEvaluatedObjectTransform *target=&sample.snapshot.object_transforms[body];
      bool visible=false,verified=false;
      for(int k=0;k<scene->primitiveCount;++k)
        if(!strcmp(scene->primitives[k].source.objectId,target->target_id))visible=true;
      if(!visible)continue;
      for(int k=0;k<scene->triangleMesh.triangleCount&&!verified;++k) {
        const RuntimeTriangle3D *t=&scene->triangleMesh.triangles[k];
        if(strcmp(scene->primitives[t->primitiveIndex].source.objectId,target->target_id))continue;
        double ux=t->p1.x-t->p0.x,uy=t->p1.y-t->p0.y,uz=t->p1.z-t->p0.z;
        double vx=t->p2.x-t->p0.x,vy=t->p2.y-t->p0.y,vz=t->p2.z-t->p0.z;
        double nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
        double length=sqrt(nx*nx+ny*ny+nz*nz);if(!(length>1e-12))continue;
        nx/=length;ny/=length;nz/=length;
        Ray3D ray={.origin={(t->p0.x+t->p1.x+t->p2.x)/3-1e-5*nx,
          (t->p0.y+t->p1.y+t->p2.y)/3-1e-5*ny,(t->p0.z+t->p1.z+t->p2.z)/3-1e-5*nz},.direction={nx,ny,nz}};
        HitInfo3D hits[2]={0};bool found_both=true;
        for(int route=0;route<2;++route) {
          RuntimeRay3D_SetTraceRouteForTests(route?RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS:RUNTIME_RAY_3D_TRACE_ROUTE_FLATTENED_BVH);
          if(!RuntimeRay3D_TraceSceneFirstHit(scene,&ray,1e-7,100,&hits[route])||strcmp(hits[route].source.objectId,target->target_id))found_both=false;
        }
        if(found_both) {
          if(fabs(hits[0].t-hits[1].t)>2e-10)return 15;
          verified=true;
        }
      }
      if(!verified){fprintf(stderr,"surface probe frame=%d target=%s failed\n",frame,target->target_id);return 14;}
    }
    RuntimeRay3D_SetTraceRouteForTests(RUNTIME_RAY_3D_TRACE_ROUTE_TLAS_BLAS_PARITY);
    printf("BR4 %s\n",
           json_object_to_json_string_ext(out, JSON_C_TO_STRING_PLAIN));
    json_object_put(out);
    RuntimeNative3DPreparedFrame_Free(&prepared);
  }
  RuntimeRigidResultCommit(NULL);
  return 0;
}

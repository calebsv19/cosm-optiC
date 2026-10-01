static void orientation_stress(SceneEditor *editor) {
 const int frames[]={0,40,10,119,53,40};
 RayEvaluatedSceneSnapshot reference;
 for(size_t t=0;t<sizeof(frames)/sizeof(frames[0]);++t){assert(SceneEditorTimelineSeek(frames[t]));assert(SceneEditorTimelineCopyEvaluated(&reference));assert(reference.object_transform_count==3 && reference.camera.has_orientation_frame);
  for(int workspace=0;workspace<SCENE_WORKSPACE_PROFILE_COUNT;++workspace){SceneEditorWorkspaceProfileSelect(editor,(SceneEditorWorkspaceProfile)workspace);SceneEditorSessionRuntimeRender(editor);
   TimelineSample sample;assert(SceneEditorTimelineCurrentSample(&sample) && sample.absolute_frame==frames[t]);
   RayEvaluatedSceneSnapshot current;assert(SceneEditorTimelineCopyEvaluated(&current));assert(!memcmp(&reference.camera.orientation_frame,&current.camera.orientation_frame,sizeof(MotionFrame)));
   for(size_t i=0;i<reference.object_transform_count;++i){const RayEvaluatedObjectTransform *o=&reference.object_transforms[i];double p[3],r[3];assert(o->has_rotation);assert(SceneEditorObjectTimelinePosition(o->target_id,p)&&SceneEditorObjectTimelineRotation(o->target_id,r));assert(fabs(p[0]-o->position.x)<1e-12&&fabs(p[1]-o->position.y)<1e-12&&fabs(p[2]-o->position.z)<1e-12);assert(fabs(r[0]-o->rotation_radians.x)<1e-12&&fabs(r[1]-o->rotation_radians.y)<1e-12&&fabs(r[2]-o->rotation_radians.z)<1e-12);}
  }
 }
 SceneEditorDocumentObjectInfo info;assert(SceneEditorDocumentObjectById("obj_sphere_medium",&info));SceneEditorDocumentTransform transform;char message[256];assert(SceneEditorDocumentGetTransformForSceneIndex(info.runtime_index,&transform,message,sizeof(message)));transform.position[0]+=1;assert(!SceneEditorDocumentSetTransformForSceneIndex(info.runtime_index,&transform,message,sizeof(message)));assert(strstr(message,"Animated transform"));
 capture(editor,"stable_multiple_followers.ppm");
 fprintf(stderr,"Orientation stress PASS: three objects, shared/separate paths, reverse progress, camera/light, six seeks across all five workspaces, animated transform edit refusal.\n");
}

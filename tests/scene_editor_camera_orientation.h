#include "app/preview_camera_projector.h"
#include "render/runtime_camera_3d_rays.h"
static void stable_camera_orientation(SceneEditor *editor,const char *scene) {
 char message[256];MotionPaths paths;
 choose_menu(editor,-1,SCENE_WORKSPACE_RENDER);authoring_control(editor,"paths");
 assert(SceneEditorMotionPathsRead(&paths));
 assert(SceneEditorMotionPathBindCamera(paths.paths[0].id,true,SceneEditorDocumentRevision(),message,sizeof(message)));
 SceneEditorSessionRuntimeRender(editor);authoring_control(editor,"path_follower/camera/main");SceneEditorSessionRuntimeRender(editor);
 dm4_inspector_control(editor,"path_camera_orientation");
 dm4_inspector_control(editor,"path_start_roll");authoring_text(editor,"35");
 dm4_inspector_control(editor,"path_end_roll_enabled");dm4_inspector_control(editor,"path_end_roll");authoring_text(editor,"395");
 RayEvaluatedSceneSnapshot samples[3];int frames[]={0,53,119};
 for(int i=0;i<3;++i){assert(SceneEditorTimelineSeek(frames[i]));assert(SceneEditorTimelineCopyEvaluated(&samples[i]));
  const RayEvaluatedCamera *camera=&samples[i].camera;assert(camera->has_orientation_frame && MotionFrameValid(&camera->orientation_frame));
  PreviewCameraSample sample={.valid=true,.has_orientation_frame=true,.orientation_frame=camera->orientation_frame,.fov_y_degrees=camera->fov_y_degrees,.aspect_ratio=1};
  PreviewCameraProjector preview;assert(PreviewCameraProjectorBuild(&sample,(SDL_Rect){0,0,128,128},&preview));
  RuntimeCamera3D runtime={0};runtime.hasOrientationFrame=true;runtime.zoom=1;runtime.nearPlane=.1;
  runtime.orientationForward=vec3(camera->orientation_frame.forward[0],camera->orientation_frame.forward[1],camera->orientation_frame.forward[2]);
  runtime.orientationUp=vec3(camera->orientation_frame.up[0],camera->orientation_frame.up[1],camera->orientation_frame.up[2]);
  RuntimeCameraProjector3D render;assert(RuntimeCameraProjector3D_Build(&runtime,128,128,&render));
  assert(fabs(preview.forward_x-render.forward.x)<1e-12 && fabs(preview.up_y-render.up.y)<1e-12 && fabs(preview.right_z-render.right.z)<1e-12);
 }
 for(int i=2;i>=0;--i){RayEvaluatedSceneSnapshot sample;SceneEditorTimelineSeek(frames[i]);assert(SceneEditorTimelineCopyEvaluated(&sample));assert(!memcmp(&sample.camera.orientation_frame,&samples[i].camera.orientation_frame,sizeof(MotionFrame)));}
 dm4_inspector_control(editor,"path_camera_orientation");
 RayEvaluatedSceneSnapshot focus;assert(SceneEditorTimelineCopyEvaluated(&focus));assert(focus.camera.has_orientation_frame && MotionFrameValid(&focus.camera.orientation_frame));
 choose_menu(editor,0,0);assert(!SceneEditorDocumentIsDirty());assert(SceneEditorDocumentOpen(scene,message,sizeof(message)));
 SceneEditorTimelineSeek(53);assert(SceneEditorTimelineCopyEvaluated(&focus));assert(focus.camera.has_orientation_frame);
 fprintf(stderr,"Stable camera PASS: route/focus mode, unwrapped roll, native/render projector basis parity, reverse seek and save/reopen.\n");
}

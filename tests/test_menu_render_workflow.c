#include "ui/menu_path_edit.h"
#include "ui/menu_batch_panel.h"
#include "ui/volume_source_ui_labels.h"
#include "app/scene_atmosphere_runtime.h"
#include "config/config_manager.h"
#include "test_support.h"
#include <json-c/json.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int run_test_menu_render_workflow_tests(void) {
    AnimationConfig saved=animSettings;MenuRuntimeState s={0};SDL_Event e={0};
    s.editingFrameDir=true;strcpy(s.pathInputBuffer,"/old/frames");menu_path_edit_begin(&s);
    assert_true("path_initial_selection_replaces",menu_path_edit_insert(&s,"/tmp/new frames"));
    assert_true("path_new_value",!strcmp(s.pathInputBuffer,"/tmp/new frames"));
    e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_a;e.key.keysym.mod=KMOD_GUI;
    assert_true("path_cmd_a",menu_path_edit_key(&s,&e) && s.pathSelectAll);
    assert_true("path_selection_replacement",menu_path_edit_insert(&s,"/tmp/é"));
    e.key.keysym.sym=SDLK_BACKSPACE;e.key.keysym.mod=0;
    assert_true("path_unicode_backspace",menu_path_edit_key(&s,&e) && !strcmp(s.pathInputBuffer,"/tmp/"));
    char before[PATH_MAX];strcpy(before,s.pathInputBuffer);
    assert_true("path_reject_multiline",!menu_path_edit_insert(&s,"bad\nfolder") && !strcmp(before,s.pathInputBuffer));
    char oversized[PATH_MAX+1];memset(oversized,'x',sizeof(oversized)-1);oversized[PATH_MAX]=0;
    assert_true("path_reject_overflow",!menu_path_edit_insert(&s,oversized) && !strcmp(before,s.pathInputBuffer));
    snprintf(animSettings.outputRoot,sizeof(animSettings.outputRoot),"/tmp/old");
    snprintf(animSettings.frameDir,sizeof(animSettings.frameDir),"/tmp/old/frames/evaluation");
    assert_true("output_rebase_applied",menu_path_edit_apply_output(&s,"/tmp/new output/",false));
    assert_true("output_rebase_preserves_frames_suffix",!strcmp(animSettings.frameDir,"/tmp/new output/frames/evaluation"));
    assert_true("output_root_applied",!strcmp(animSettings.outputRoot,"/tmp/new output"));
    assert_true("output_relative_refused",!menu_path_edit_apply_output(&s,"relative",false) && !strcmp(animSettings.outputRoot,"/tmp/new output"));
    assert_true("frames_root_refused",!menu_path_edit_apply_output(&s,"/",true) && !strcmp(animSettings.frameDir,"/tmp/new output/frames/evaluation"));
    assert_true("output_parent_component_refused",!menu_path_edit_apply_output(&s,"/tmp/../bad",false));
    assert_true("frames_explicit_applied",menu_path_edit_apply_output(&s,"/tmp/other/frames",true));
    assert_true("frames_reconcile_root",!strcmp(animSettings.outputRoot,"/tmp/other") && !strcmp(animSettings.frameDir,"/tmp/other/frames"));
    strcpy(s.pathInputBuffer,"/tmp/cancelled");
    assert_true("frames_cancel",menu_batch_panel_finish_edit(&s,false) && !strcmp(animSettings.frameDir,"/tmp/other/frames"));

    animSettings.sceneSource=SCENE_SOURCE_RUNTIME_SCENE;animSettings.runtimeScenePath[0]=0;
    SceneAtmosphereAttachment v;SceneAtmosphereAttachmentDefaults(&v);
    v.present=true;strcpy(v.source_kind,"vf3d");strcpy(v.source_path,"assets/cloud.vf3d");
    json_object* scene=json_object_new_object(),*ext=json_object_new_object(),*ray=json_object_new_object(),*author=json_object_new_object();
    json_object_object_add(scene,"extensions",ext);json_object_object_add(ext,"ray_tracing",ray);json_object_object_add(ray,"authoring",author);
    json_object_object_add(author,"atmosphere_attachment",SceneAtmosphereAttachmentToJson(&v));
    assert_true("scene_static_load",SceneAtmosphereRuntimeLoad(scene,1));
    assert_true("scene_static_overrides_legacy_dynamic",!SceneAtmosphereRuntimeRequiresSynchronousSelection(true));
    char label[160];volume_source_ui_format_active_button_label(label,sizeof(label));
    assert_true("scene_volume_label_on",strstr(label,"Scene atmosphere [On]")!=NULL);
    v.animated=true;json_object_object_add(author,"atmosphere_attachment",SceneAtmosphereAttachmentToJson(&v));
    assert_true("scene_dynamic_load",SceneAtmosphereRuntimeLoad(scene,1));
    assert_true("scene_dynamic_stays_guarded",SceneAtmosphereRuntimeRequiresSynchronousSelection(false));
    v.enabled=false;json_object_object_add(author,"atmosphere_attachment",SceneAtmosphereAttachmentToJson(&v));
    assert_true("scene_disabled_load",SceneAtmosphereRuntimeLoad(scene,1));
    assert_true("scene_disabled_owns_selection",!SceneAtmosphereRuntimeRequiresSynchronousSelection(true));
    volume_source_ui_format_active_button_label(label,sizeof(label));
    assert_true("scene_volume_label_off",strstr(label,"Scene atmosphere [Off]")!=NULL);
    assert_true("saved_scene_status_write",json_object_to_file("menu_atmosphere_status.json",scene)==0);
    snprintf(animSettings.runtimeScenePath,sizeof(animSettings.runtimeScenePath),"menu_atmosphere_status.json");
    SceneAtmosphereRuntimeReset();
    volume_source_ui_format_active_button_label(label,sizeof(label));
    assert_true("saved_scene_status_before_runtime_load",strstr(label,"Scene atmosphere [Off]")!=NULL);
    v.enabled=true;json_object_object_add(author,"atmosphere_attachment",SceneAtmosphereAttachmentToJson(&v));
    assert_true("saved_scene_status_update",json_object_to_file("menu_atmosphere_status.json",scene)==0);
    volume_source_ui_format_active_button_label(label,sizeof(label));
    assert_true("saved_scene_status_refresh",strstr(label,"Scene atmosphere [On]")!=NULL);
    assert_true("saved_status_does_not_load_runtime",!SceneAtmosphereRuntimeRead()->present);
    remove("menu_atmosphere_status.json");json_object_put(scene);
    assert_true("legacy_dynamic_stays_guarded",SceneAtmosphereRuntimeRequiresSynchronousSelection(true));
    animSettings=saved;return 0;
}

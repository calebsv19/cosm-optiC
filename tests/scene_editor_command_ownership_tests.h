/* Exercised inside the real open-document fixture, through the live dispatcher. */
#include "editor/scene_editor_agent_control.h"
#include "editor/scene_editor_rename.h"
static json_object *l2_command(const char *method, unsigned long long revision) {
    json_object *r=json_object_new_object(),*p=json_object_new_object();
    json_object_object_add(r,"method",json_object_new_string(method));
    json_object_object_add(r,"params",p);
    json_object_object_add(p,"expected_revision",json_object_new_int64(revision));
    json_object_object_add(r,"scene_path",json_object_new_string(SceneEditorDocumentPath()));
    return r;
}
static bool l2_dispatch(json_object *r) {
    json_object *out=SceneEditorAgentControlDispatch(r),*ok=NULL;
    json_object_object_get_ex(out,"ok",&ok);bool value=json_object_get_boolean(ok);
    json_object_put(out);json_object_put(r);return value;
}
static void test_l2_command_ownership(void) {
    char message[256];SceneEditorDocumentObjectInfo before,after;
    assert_true("l2_initial_object",SceneEditorDocumentObjectAt(0,&before));
    unsigned long long revision=SceneEditorDocumentRevision();
    assert_true("l2_native_rename",SceneEditorDocumentRenameById(before.id,"L2 native",revision,message,sizeof(message)));
    json_object *r=l2_command("scene.undo",revision);
    assert_true("l2_stale_refuses",!l2_dispatch(r));
    revision=SceneEditorDocumentRevision();
    r=l2_command("scene.undo",revision);
    json_object_object_add(r,"scene_path",json_object_new_string("/foreign/scene.json"));
    assert_true("l2_foreign_refuses",!l2_dispatch(r));
    r=l2_command("scene.undo",revision);
    json_object_object_add(r,"method",json_object_new_string_len("scene.undo\0suffix",17));
    assert_true("l2_embedded_nul_method_refuses",!l2_dispatch(r));
    r=l2_command("scene.undo",revision);
    char path[4096];size_t length=strlen(SceneEditorDocumentPath());
    assert_true("l2_fixture_path_capacity",length+8<sizeof(path));
    memcpy(path,SceneEditorDocumentPath(),length+1);memcpy(path+length+1,"suffix",7);
    json_object_object_add(r,"scene_path",json_object_new_string_len(path,(int)length+7));
    assert_true("l2_embedded_nul_owner_refuses",!l2_dispatch(r));
    assert_true("l2_refusal_preserves_revision",SceneEditorDocumentRevision()==revision);
    r=l2_command("scene.undo",revision);
    assert_true("l2_agent_undo_native_edit",l2_dispatch(r));
    assert_true("l2_native_state_restored",SceneEditorDocumentObjectAt(0,&after)&&!strcmp(before.name,after.name));
    assert_true("l2_redo_survives_refused_command",!l2_dispatch(l2_command("scene.undo",revision))&&SceneEditorDocumentCanRedo());
    assert_true("l2_native_redo_agent_undo",SceneEditorDocumentRedo(message,sizeof(message)));
    assert_true("l2_native_undo_cleanup",SceneEditorDocumentUndo(message,sizeof(message)));
    puts("L2 native/control shared history, malformed/stale/foreign refusal passed");
}

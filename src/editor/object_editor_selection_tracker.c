#include "editor/object_editor_selection_tracker.h"
#include "editor/scene_editor_document.h"
#include "import/runtime_scene_bridge.h"
#include <string.h>
#include <stdio.h>
#include "config/config_manager.h"
static bool retained_selection(void) {
    return SceneEditorDocumentIsOpen() && animSettings.sceneSource==SCENE_SOURCE_RUNTIME_SCENE &&
        strcmp(SceneEditorDocumentPath(),animSettings.runtimeScenePath)==0;
}
static char current_id[128], last_id[128];

const char* ObjectEditorSelectionTrackerId(void) {
    if (current_id[0] && !SceneEditorDocumentObjectById(current_id,NULL)) current_id[0]=0;
    return current_id;
}
bool ObjectEditorSelectionTrackerSelectId(const char* id) {
    SceneEditorDocumentObjectInfo info;
    if (!id || !id[0]) { current_id[0]=0;return true; }
    if (!SceneEditorDocumentObjectById(id,&info)) return false;
    snprintf(current_id,sizeof(current_id),"%s",id);
    snprintf(last_id,sizeof(last_id),"%s",id);
    return true;
}

static int s_current_object_index = -1;
static int s_last_object_index = -1;

static int tracker_valid_index(int index, int object_count) {
    return index >= 0 && index < object_count;
}

void ObjectEditorSelectionTrackerSetCurrent(int index, int object_count) {
    if (retained_selection()) {
        char id[128]={0};
        if (index>=0 && index<object_count) runtime_scene_bridge_get_last_object_id_for_scene_index(index,id,sizeof(id));
        ObjectEditorSelectionTrackerSelectId(id);
        return;
    }
    current_id[0]=last_id[0]=0;
    if (!tracker_valid_index(index, object_count)) {
        s_current_object_index = -1;
        return;
    }
    s_current_object_index = index;
    s_last_object_index = index;
}

void ObjectEditorSelectionTrackerNotifyDelete(int index) {
    if (retained_selection()) { (void)ObjectEditorSelectionTrackerId();return; }
    if (index < 0) return;
    if (s_current_object_index == index) {
        s_current_object_index = -1;
    } else if (s_current_object_index > index) {
        s_current_object_index -= 1;
    }
    if (s_last_object_index == index) {
        s_last_object_index = -1;
    } else if (s_last_object_index > index) {
        s_last_object_index -= 1;
    }
}

int ObjectEditorSelectionTrackerCurrent(int object_count) {
    if (retained_selection()) {
        SceneEditorDocumentObjectInfo info;
        return SceneEditorDocumentObjectById(current_id,&info) ? info.runtime_index : -1;
    }
    if (tracker_valid_index(s_current_object_index, object_count)) {
        return s_current_object_index;
    }
    return -1;
}

int ObjectEditorSelectionTrackerLast(int object_count) {
    if (retained_selection()) {
        SceneEditorDocumentObjectInfo info;
        return SceneEditorDocumentObjectById(last_id,&info) ? info.runtime_index : -1;
    }
    if (tracker_valid_index(s_last_object_index, object_count)) {
        return s_last_object_index;
    }
    return -1;
}

void ObjectEditorSelectionTrackerReset(void) {
    current_id[0]=last_id[0]=0;s_current_object_index=s_last_object_index=-1;
}

#include "editor/object_editor_selection_tracker.h"

static int s_current_object_index = -1;
static int s_last_object_index = -1;

static int tracker_valid_index(int index, int object_count) {
    return index >= 0 && index < object_count;
}

void ObjectEditorSelectionTrackerSetCurrent(int index, int object_count) {
    if (!tracker_valid_index(index, object_count)) {
        s_current_object_index = -1;
        return;
    }
    s_current_object_index = index;
    s_last_object_index = index;
}

void ObjectEditorSelectionTrackerNotifyDelete(int index) {
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
    if (tracker_valid_index(s_current_object_index, object_count)) {
        return s_current_object_index;
    }
    return -1;
}

int ObjectEditorSelectionTrackerLast(int object_count) {
    if (tracker_valid_index(s_last_object_index, object_count)) {
        return s_last_object_index;
    }
    return -1;
}

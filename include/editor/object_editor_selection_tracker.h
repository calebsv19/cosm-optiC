#ifndef OBJECT_EDITOR_SELECTION_TRACKER_H
#define OBJECT_EDITOR_SELECTION_TRACKER_H

void ObjectEditorSelectionTrackerSetCurrent(int index, int object_count);
void ObjectEditorSelectionTrackerNotifyDelete(int index);
int ObjectEditorSelectionTrackerCurrent(int object_count);
int ObjectEditorSelectionTrackerLast(int object_count);

#endif

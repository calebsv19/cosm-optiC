#ifndef OBJECT_EDITOR_SELECTION_TRACKER_H
#define OBJECT_EDITOR_SELECTION_TRACKER_H
#include <stdbool.h>
void ObjectEditorSelectionTrackerReset(void);
const char* ObjectEditorSelectionTrackerId(void);
bool ObjectEditorSelectionTrackerSelectId(const char* id);

void ObjectEditorSelectionTrackerSetCurrent(int index, int object_count);
void ObjectEditorSelectionTrackerNotifyDelete(int index);
int ObjectEditorSelectionTrackerCurrent(int object_count);
int ObjectEditorSelectionTrackerLast(int object_count);

#endif

#ifndef SCENE_SOURCE_UI_LABELS_H
#define SCENE_SOURCE_UI_LABELS_H

#include <stddef.h>

void scene_source_ui_format_catalog_option_label(const char* path,
                                                 int source,
                                                 char* out,
                                                 size_t out_size);
void scene_source_ui_format_active_button_label(char* out, size_t out_size);
void scene_source_ui_format_scene_select_status(int source,
                                                const char* path,
                                                char* out,
                                                size_t out_size);

#endif

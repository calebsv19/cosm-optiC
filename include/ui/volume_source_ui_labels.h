#ifndef VOLUME_SOURCE_UI_LABELS_H
#define VOLUME_SOURCE_UI_LABELS_H

#include <stdbool.h>
#include <stddef.h>

void volume_source_ui_format_catalog_option_label(const char* path,
                                                  int kind,
                                                  char* out,
                                                  size_t out_size);
void volume_source_ui_format_active_button_label(char* out, size_t out_size);
void volume_source_ui_format_attach_status(int kind,
                                           const char* path,
                                           bool enabled,
                                           char* out,
                                           size_t out_size);

#endif

#ifndef UI_MENU_SETTINGS_LIFECYCLE_H
#define UI_MENU_SETTINGS_LIFECYCLE_H

#include <stdbool.h>

#include "ui/sdl_menu_state.h"

void menu_settings_lifecycle_commit(MenuRuntimeState* state,
                                    const char* reason,
                                    bool render_state_changed);
void menu_settings_lifecycle_commit_slider_release(MenuRuntimeState* state,
                                                   int* target);

#endif

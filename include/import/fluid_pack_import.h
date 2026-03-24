#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "import/fluid_import.h"

bool fluid_pack_path_is_pack(const char *path);
bool fluid_pack_derive_legacy_vf2d_path(const char *pack_path, char *out_path, size_t out_path_size);
bool fluid_pack_frame_load(const char *path, FluidFrame *out);

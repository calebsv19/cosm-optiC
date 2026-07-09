#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "import/fluid_import.h"

// Legacy physics pack import helpers for the planar VFHD/DENS/VELX/VELY
// profile. PSBU-11A freezes truthful 3D pack ingest as a separate VF3H-based
// profile instead of extending these helpers in place.

bool fluid_pack_path_is_pack(const char *path);
bool fluid_pack_derive_legacy_vf2d_path(const char *pack_path, char *out_path, size_t out_path_size);
bool fluid_pack_frame_load(const char *path, FluidFrame *out);

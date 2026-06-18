#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "render/runtime_volume_3d.h"

/* Native-3D VF3D import contract.
 * This lane intentionally stays separate from the legacy planar fluid_import.h
 * family, which remains vf2d/VFHD-only. */

bool fluid_volume_import_3d_classify_path(const char* path,
                                          RuntimeVolume3DSourceKind* out_kind);
bool fluid_volume_import_3d_path_is_supported(const char* path);
bool fluid_volume_import_3d_load_raw(const char* path,
                                     RuntimeVolumeAttachment3D* out_attachment,
                                     char* out_diagnostics,
                                     size_t out_diagnostics_size);
bool fluid_volume_import_3d_load_pack(const char* path,
                                      RuntimeVolumeAttachment3D* out_attachment,
                                      char* out_diagnostics,
                                      size_t out_diagnostics_size);
bool fluid_volume_import_3d_load_source(const char* path,
                                        RuntimeVolume3DSourceKind source_kind_hint,
                                        RuntimeVolumeAttachment3D* out_attachment,
                                        char* out_diagnostics,
                                        size_t out_diagnostics_size);
bool fluid_volume_import_3d_load_source_at_frame(const char* path,
                                                 RuntimeVolume3DSourceKind source_kind_hint,
                                                 int requested_frame_index,
                                                 RuntimeVolumeAttachment3D* out_attachment,
                                                 char* out_diagnostics,
                                                 size_t out_diagnostics_size);
bool fluid_volume_import_3d_resolve_source_frame_path(const char* path,
                                                      RuntimeVolume3DSourceKind source_kind_hint,
                                                      int requested_frame_index,
                                                      char* out_frame_path,
                                                      size_t out_frame_path_size,
                                                      char* out_diagnostics,
                                                      size_t out_diagnostics_size);

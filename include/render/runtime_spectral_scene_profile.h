#ifndef RENDER_RUNTIME_SPECTRAL_SCENE_PROFILE_H
#define RENDER_RUNTIME_SPECTRAL_SCENE_PROFILE_H
#include <json-c/json.h>
#include "render/runtime_spectral_pencil.h"
/* Shared serialized optical-profile boundary, independent of CLI request envelopes.
 * Pixel probes/work limits are checked against the effective prepared film. */
bool RuntimeSpectralProfile_FromScene(json_object *,const char *,bool,RuntimeSpectralPencil *);
bool RuntimeSpectralProfile_Parse(json_object *,const char *,RuntimeSpectralPencil *);
bool RuntimeSpectralProfile_ParseFilm(json_object *,const char *,int,int,RuntimeSpectralPencil *);
#endif

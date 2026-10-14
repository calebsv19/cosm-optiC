#ifndef SCENE_ATMOSPHERE_OPTICS_H
#define SCENE_ATMOSPHERE_OPTICS_H
#include <stdbool.h>
#include <stddef.h>
#include <json-c/json.h>
/* Optional retained policy. Absent metadata preserves legacy appearance. */
typedef struct {
    bool present, physical, cell_centered, sky_illumination;
    double reference_concentration_kg_m3, mass_extinction_m2_kg;
    double scattering_albedo, anisotropy, sky_error_budget;
} SceneAtmosphereOptics;
void SceneAtmosphereOpticsDefaults(SceneAtmosphereOptics* value);
bool SceneAtmosphereOpticsValidate(const SceneAtmosphereOptics* value, char* diagnostic, size_t size);
bool SceneAtmosphereOpticsParse(json_object* object, SceneAtmosphereOptics* value, char* diagnostic, size_t size);
json_object* SceneAtmosphereOpticsToJson(const SceneAtmosphereOptics* value);
#endif

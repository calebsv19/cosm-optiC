#include "procedural/procedural_surface_wood_grain.h"
#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool get_double(json_object *object, const char *key, double *out_value) {
    json_object *value = NULL;
    if (!json_object_object_get_ex(object, key, &value) ||
        (!json_object_is_type(value, json_type_double) &&
         !json_object_is_type(value, json_type_int))) return false;
    *out_value = json_object_get_double(value);
    return isfinite(*out_value);
}

static bool get_vec(json_object *object, const char *key, double values[3]) {
    json_object *array = NULL;
    if (!json_object_object_get_ex(object, key, &array) ||
        !json_object_is_type(array, json_type_array) ||
        json_object_array_length(array) != 3) return false;
    for (size_t i = 0u; i < 3u; ++i) {
        json_object *value = json_object_array_get_idx(array, (int)i);
        if (!value || (!json_object_is_type(value, json_type_double) &&
                       !json_object_is_type(value, json_type_int))) return false;
        values[i] = json_object_get_double(value);
        if (!isfinite(values[i])) return false;
    }
    return true;
}

bool ProceduralSurfaceWoodGrainFieldV1_Validate(
    const ProceduralSurfaceWoodGrainFieldV1 *field) {
    return field && strlen(field->preset_digest_sha256) == 64u &&
        strlen(field->source_mesh_digest_sha256) == 64u &&
        isfinite(field->orientation_radians) && field->frequency_per_unit > 0.0 &&
        field->contrast >= 0.0 && field->normal_strength >= 0.0 &&
        field->flow_kind >= 0 && field->flow_kind <= 2;
}

bool ProceduralSurfaceWoodGrainFieldV1_LoadJsonFile(
    const char *path, ProceduralSurfaceWoodGrainFieldV1 *out_field) {
    json_object *root = NULL, *evaluation = NULL, *outputs = NULL;
    json_object *chroma = NULL, *microdetail = NULL, *value = NULL;
    ProceduralSurfaceWoodGrainFieldV1 field = {0};
    const char *flow = NULL;
    bool valid = false;
    if (!path || !out_field || !(root = json_object_from_file(path))) return false;
    if (!json_object_object_get_ex(root, "preset_digest_sha256", &value)) goto done;
    snprintf(field.preset_digest_sha256, sizeof(field.preset_digest_sha256), "%s",
             json_object_get_string(value));
    if (!json_object_object_get_ex(root, "source_mesh_digest_sha256", &value)) goto done;
    snprintf(field.source_mesh_digest_sha256, sizeof(field.source_mesh_digest_sha256), "%s",
             json_object_get_string(value));
    if (!json_object_object_get_ex(root, "evaluation", &evaluation) ||
        !json_object_object_get_ex(root, "outputs", &outputs) ||
        !json_object_object_get_ex(outputs, "chroma_bands", &chroma) ||
        !json_object_object_get_ex(outputs, "microdetail_height", &microdetail) ||
        !get_double(evaluation, "orientation_radians", &field.orientation_radians) ||
        !get_double(evaluation, "frequency_per_unit", &field.frequency_per_unit) ||
        !get_double(evaluation, "width_variation", &field.width_variation) ||
        !get_double(evaluation, "turbulence", &field.turbulence) ||
        !json_object_object_get_ex(evaluation, "flow", &value)) goto done;
    flow = json_object_get_string(value);
    field.flow_kind = !strcmp(flow, "straight") ? 0 : !strcmp(flow, "curved") ? 1 :
        !strcmp(flow, "turbulent") ? 2 : -1;
    if (!get_vec(chroma, "base_color", field.base_color) ||
        !get_vec(chroma, "latewood_color", field.latewood_color) ||
        !get_double(chroma, "contrast", &field.contrast) ||
        !get_double(microdetail, "normal_strength", &field.normal_strength) ||
        !get_double(microdetail, "knot_normal_strength", &field.knot_normal_strength)) goto done;
    valid = ProceduralSurfaceWoodGrainFieldV1_Validate(&field);
done:
    json_object_put(root);
    if (valid) *out_field = field;
    else memset(out_field, 0, sizeof(*out_field));
    return valid;
}
static double height(const ProceduralSurfaceWoodGrainFieldV1*f,double x,double z){double u=x*cos(f->orientation_radians)+z*sin(f->orientation_radians),w=f->turbulence*sin(z*2.3+u*.7);if(f->flow_kind==1)w+=.20*z*z;else if(f->flow_kind==2)w+=.32*sin(x*1.7+z*2.9);return .5+.5*sin((u+w)*f->frequency_per_unit*6.283185307179586);}
bool ProceduralSurfaceWoodGrainFieldV1_Sample(const ProceduralSurfaceWoodGrainFieldV1*f,double x,double z,ProceduralSurfaceWoodGrainSampleV1*out){double h,eps=.002;if(!ProceduralSurfaceWoodGrainFieldV1_Validate(f)||!out)return false;out->height=height(f,x,z);h=fmax(0.,fmin(1.,(out->height-.5)*f->contrast+.5));for(int i=0;i<3;i++)out->color[i]=f->latewood_color[i]*(1-h)+f->base_color[i]*h;out->roughness_delta=(.5-h)*.18;out->slope_x=(height(f,x+eps,z)-height(f,x-eps,z))/(2*eps)*f->normal_strength;out->slope_z=(height(f,x,z+eps)-height(f,x,z-eps))/(2*eps)*f->normal_strength;return true;}

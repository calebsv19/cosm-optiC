#include "procedural/procedural_surface_feature_field.h"
#include "app/ray_tracing_sha256.h"
#include "core_io.h"
#include <json-c/json.h>

#include <math.h>
#include <string.h>
#include <stdio.h>

static double dot(ProceduralSurfaceFeatureVec3 a, ProceduralSurfaceFeatureVec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static double length(ProceduralSurfaceFeatureVec3 a) { return sqrt(dot(a,a)); }
static bool unit(ProceduralSurfaceFeatureVec3 a) { return isfinite(a.x)&&isfinite(a.y)&&isfinite(a.z)&&fabs(length(a)-1.0)<1e-6; }
static double clamp01(double a) { return a < 0.0 ? 0.0 : a > 1.0 ? 1.0 : a; }

static bool sha256(const char *text) {
    size_t i;
    if (!text || strlen(text) != 64u) return false;
    for (i = 0u; i < 64u; ++i)
        if (!((text[i] >= '0' && text[i] <= '9') ||
              (text[i] >= 'a' && text[i] <= 'f')))
            return false;
    return true;
}

bool ProceduralSurfaceFeatureFieldV1_Validate(const ProceduralSurfaceFeatureFieldV1 *f) {
    if (!f || !sha256(f->source_mesh_digest_sha256) ||
        !sha256(f->authoring_digest_sha256) ||
        f->feature_count == 0u || f->feature_count > PROCEDURAL_SURFACE_FEATURE_FIELD_MAX_FEATURES ||
        !isfinite(f->normal_compatibility_cosine) || f->normal_compatibility_cosine < -1.0 || f->normal_compatibility_cosine > 1.0) return false;
    for (size_t i=0; i<f->feature_count; ++i) { const ProceduralSurfaceFeatureRootV1 *r=&f->features[i];
        if (!r->feature_id || !isfinite(r->radius) || r->radius<=0.0 || !isfinite(r->aspect) || r->aspect<=0.0 ||
            !isfinite(r->rotation) || !isfinite(r->edge_softness) || r->edge_softness<0.0 || r->edge_softness>1.0 ||
            !isfinite(r->rim_width) || r->rim_width<=0.0 || r->rim_width>=1.0 || !unit(r->normal) || !unit(r->tangent) || !unit(r->bitangent) ||
            fabs(dot(r->normal,r->tangent))>1e-6 || fabs(dot(r->normal,r->bitangent))>1e-6 || fabs(dot(r->tangent,r->bitangent))>1e-6 ||
            !isfinite(r->position.x)||!isfinite(r->position.y)||!isfinite(r->position.z) ||
            r->barycentric[0]<0.0 || r->barycentric[1]<0.0 || r->barycentric[2]<0.0 || fabs(r->barycentric[0]+r->barycentric[1]+r->barycentric[2]-1.0)>1e-6) return false;
        for (size_t j=0;j<i;++j) if (r->feature_id==f->features[j].feature_id) return false;
    } return true;
}

static size_t cell(const ProceduralSurfaceFeatureFieldV1 *f, double x, double y) {
    double sx=(x-f->grid_min_x)/(f->grid_max_x-f->grid_min_x), sy=(y-f->grid_min_y)/(f->grid_max_y-f->grid_min_y);
    int ix=(int)floor(sx*PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION), iy=(int)floor(sy*PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION);
    if (ix < 0) ix = 0;
    if (ix >= (int)PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION)
        ix = (int)PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION - 1;
    if (iy < 0) iy = 0;
    if (iy >= (int)PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION)
        iy = (int)PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION - 1;
    return (size_t)iy*PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION+(size_t)ix;
}

bool ProceduralSurfaceFeatureFieldV1_BuildIndex(ProceduralSurfaceFeatureFieldV1 *f) {
    if(!ProceduralSurfaceFeatureFieldV1_Validate(f)) return false;
    f->grid_min_x=f->grid_min_y=INFINITY; f->grid_max_x=f->grid_max_y=-INFINITY;
    for(size_t i=0;i<f->feature_count;i++){ const ProceduralSurfaceFeatureRootV1*r=&f->features[i];
        if(r->position.x-r->radius<f->grid_min_x)f->grid_min_x=r->position.x-r->radius; if(r->position.y-r->radius*r->aspect<f->grid_min_y)f->grid_min_y=r->position.y-r->radius*r->aspect;
        if(r->position.x+r->radius>f->grid_max_x)f->grid_max_x=r->position.x+r->radius; if(r->position.y+r->radius*r->aspect>f->grid_max_y)f->grid_max_y=r->position.y+r->radius*r->aspect; }
    if(f->grid_max_x<=f->grid_min_x)f->grid_max_x=f->grid_min_x+1; if(f->grid_max_y<=f->grid_min_y)f->grid_max_y=f->grid_min_y+1;
    memset(f->grid_counts,0,sizeof(f->grid_counts)); f->grid_index_count=0;
    /* Every cell touched by the conservative oriented-ellipse bound holds this
     * root.  Sampling one cell therefore cannot miss a footprint, and the
     * explicit per-cell capacity remains the native hit-time bound. */
    for (size_t i = 0u; i < f->feature_count; ++i) {
        const ProceduralSurfaceFeatureRootV1 *r = &f->features[i];
        double extent = r->radius * fmax(1.0, r->aspect);
        size_t lo = cell(f, r->position.x - extent, r->position.y - extent);
        size_t hi = cell(f, r->position.x + extent, r->position.y + extent);
        size_t min_x = lo % PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION;
        size_t min_y = lo / PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION;
        size_t max_x = hi % PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION;
        size_t max_y = hi / PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION;
        for (size_t y = min_y; y <= max_y; ++y) for (size_t x = min_x; x <= max_x; ++x) {
            size_t c = y * PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_DIMENSION + x;
            if (f->grid_counts[c] >= PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_CELL_CAPACITY ||
                f->grid_index_count >= sizeof(f->grid_indices) / sizeof(f->grid_indices[0]))
                return false;
            f->grid_indices[c * PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_CELL_CAPACITY +
                            f->grid_counts[c]++] = (uint16_t)i;
            ++f->grid_index_count;
        }
    }
    return true;
}

bool ProceduralSurfaceFeatureFieldV1_CanonicalJson(const ProceduralSurfaceFeatureFieldV1 *f,
    char *out, size_t cap) {
    size_t used = 0u;
    int wrote;
    if (!out || cap == 0u || !ProceduralSurfaceFeatureFieldV1_Validate(f)) return false;
#define APPEND(...) do { wrote = snprintf(out + used, cap - used, __VA_ARGS__); \
    if (wrote < 0 || (size_t)wrote >= cap - used) return false; used += (size_t)wrote; } while (0)
    APPEND("{\"schema\":\"surface_feature_field_v1\",\"schema_version\":1,\"source_mesh_digest_sha256\":\"%s\",\"authoring_digest_sha256\":\"%s\",\"seed\":%llu,\"normal_compatibility_cosine\":%.17g,\"features\":[", f->source_mesh_digest_sha256, f->authoring_digest_sha256, (unsigned long long)f->seed, f->normal_compatibility_cosine);
    for (size_t i=0;i<f->feature_count;i++) { const ProceduralSurfaceFeatureRootV1 *r=&f->features[i];
        APPEND("%s{\"feature_id\":%u,\"population\":%u,\"source_triangle\":%u,\"barycentric_root\":[%.17g,%.17g,%.17g],\"position\":[%.17g,%.17g,%.17g],\"normal\":[%.17g,%.17g,%.17g],\"tangent\":[%.17g,%.17g,%.17g],\"bitangent\":[%.17g,%.17g,%.17g],\"radius\":%.17g,\"aspect\":%.17g,\"rotation\":%.17g,\"edge_softness\":%.17g,\"rim_width\":%.17g}", i ? "," : "", r->feature_id,r->population,r->source_triangle,r->barycentric[0],r->barycentric[1],r->barycentric[2],r->position.x,r->position.y,r->position.z,r->normal.x,r->normal.y,r->normal.z,r->tangent.x,r->tangent.y,r->tangent.z,r->bitangent.x,r->bitangent.y,r->bitangent.z,r->radius,r->aspect,r->rotation,r->edge_softness,r->rim_width); }
    APPEND("]}");
#undef APPEND
    return true;
}

bool ProceduralSurfaceFeatureFieldV1_Digest(const ProceduralSurfaceFeatureFieldV1 *f,
    char out_digest[PROCEDURAL_SURFACE_FEATURE_FIELD_DIGEST_CAPACITY]) {
    char canonical[PROCEDURAL_SURFACE_FEATURE_FIELD_CANONICAL_CAPACITY];
    return out_digest && ProceduralSurfaceFeatureFieldV1_CanonicalJson(f, canonical, sizeof(canonical)) &&
        ray_tracing_sha256_bytes(canonical, strlen(canonical), out_digest);
}

bool ProceduralSurfaceFeatureFieldV1_SaveJsonFileAtomic(
    const char *path, const ProceduralSurfaceFeatureFieldV1 *f) {
    char canonical[PROCEDURAL_SURFACE_FEATURE_FIELD_CANONICAL_CAPACITY];
    CoreResult result;
    if (!path || !ProceduralSurfaceFeatureFieldV1_CanonicalJson(
            f, canonical, sizeof(canonical))) return false;
    result = core_io_write_all_atomic(path, canonical, strlen(canonical));
    return result.code == CORE_OK;
}

static bool json_number_array(json_object *root, const char *key,
                              double *out, size_t count) {
    json_object *array = NULL;
    if (!json_object_object_get_ex(root, key, &array) ||
        !json_object_is_type(array, json_type_array) ||
        json_object_array_length(array) != count) return false;
    for (size_t i = 0u; i < count; ++i) {
        out[i] = json_object_get_double(json_object_array_get_idx(array, i));
        if (!isfinite(out[i])) return false;
    }
    return true;
}

bool ProceduralSurfaceFeatureFieldV1_LoadJsonFile(
    const char *path, ProceduralSurfaceFeatureFieldV1 *out_field) {
    json_object *root = NULL, *value = NULL, *features = NULL;
    ProceduralSurfaceFeatureFieldV1 field = {0};
    if (!path || !out_field || !(root = json_object_from_file(path)) ||
        !json_object_object_get_ex(root, "schema", &value) ||
        strcmp(json_object_get_string(value), "surface_feature_field_v1") != 0 ||
        !json_object_object_get_ex(root, "schema_version", &value) ||
        json_object_get_int(value) != 1 ||
        !json_object_object_get_ex(root, "source_mesh_digest_sha256", &value)) goto fail;
    snprintf(field.source_mesh_digest_sha256, sizeof(field.source_mesh_digest_sha256), "%s",
             json_object_get_string(value));
    if (!json_object_object_get_ex(root, "authoring_digest_sha256", &value)) goto fail;
    snprintf(field.authoring_digest_sha256, sizeof(field.authoring_digest_sha256), "%s",
             json_object_get_string(value));
    if (!json_object_object_get_ex(root, "seed", &value)) goto fail;
    field.seed = (uint64_t)json_object_get_int64(value);
    if (!json_object_object_get_ex(root, "normal_compatibility_cosine", &value)) goto fail;
    field.normal_compatibility_cosine = json_object_get_double(value);
    if (!json_object_object_get_ex(root, "features", &features) ||
        !json_object_is_type(features, json_type_array) ||
        (field.feature_count = json_object_array_length(features)) > PROCEDURAL_SURFACE_FEATURE_FIELD_MAX_FEATURES)
        goto fail;
    for (size_t i = 0u; i < field.feature_count; ++i) {
        json_object *entry = json_object_array_get_idx(features, i);
        ProceduralSurfaceFeatureRootV1 *r = &field.features[i];
#define READ_UINT(KEY, TARGET) do { if (!json_object_object_get_ex(entry, KEY, &value)) goto fail; TARGET = (uint32_t)json_object_get_int64(value); } while (0)
#define READ_NUM(KEY, TARGET) do { if (!json_object_object_get_ex(entry, KEY, &value)) goto fail; TARGET = json_object_get_double(value); } while (0)
        READ_UINT("feature_id", r->feature_id); READ_UINT("population", r->population);
        READ_UINT("source_triangle", r->source_triangle);
        if (!json_number_array(entry, "barycentric_root", r->barycentric, 3u) ||
            !json_number_array(entry, "position", &r->position.x, 3u) ||
            !json_number_array(entry, "normal", &r->normal.x, 3u) ||
            !json_number_array(entry, "tangent", &r->tangent.x, 3u) ||
            !json_number_array(entry, "bitangent", &r->bitangent.x, 3u)) goto fail;
        READ_NUM("radius", r->radius); READ_NUM("aspect", r->aspect);
        READ_NUM("rotation", r->rotation); READ_NUM("edge_softness", r->edge_softness);
        READ_NUM("rim_width", r->rim_width);
#undef READ_UINT
#undef READ_NUM
    }
    if (!ProceduralSurfaceFeatureFieldV1_BuildIndex(&field)) goto fail;
    *out_field = field;
    json_object_put(root);
    return true;
fail:
    if (root) json_object_put(root);
    return false;
}

bool ProceduralSurfaceFeatureFieldV1_Sample(const ProceduralSurfaceFeatureFieldV1 *f, ProceduralSurfaceFeatureVec3 p, ProceduralSurfaceFeatureVec3 n, ProceduralSurfaceFeatureSampleV1 *out) {
    if (!out || !unit(n) || !ProceduralSurfaceFeatureFieldV1_Validate(f) || f->grid_index_count==0u) return false;
    memset(out,0,sizeof(*out)); double best=0.0;
    size_t gc = cell(f, p.x, p.y), count = f->grid_counts[gc];
    for (size_t k=0u; k<count; ++k) { const ProceduralSurfaceFeatureRootV1 *r=&f->features[f->grid_indices[gc*PROCEDURAL_SURFACE_FEATURE_FIELD_GRID_CELL_CAPACITY+k]];
        ProceduralSurfaceFeatureVec3 d={p.x-r->position.x,p.y-r->position.y,p.z-r->position.z};
        if (dot(n,r->normal)<f->normal_compatibility_cosine) continue;
        double x=dot(d,r->tangent), y=dot(d,r->bitangent), c=cos(r->rotation), s=sin(r->rotation);
        double u=(x*c+y*s)/r->radius, v=(-x*s+y*c)/(r->radius*r->aspect), q=sqrt(u*u+v*v);
        if (q>1.0) continue; ++out->candidates_considered;
        double edge=clamp01((1.0-q)/fmax(r->edge_softness,1e-9));
        double cov=r->edge_softness>0.0 ? edge*edge*(3.0-2.0*edge) : 1.0;
        if (cov>best) { best=cov; out->coverage=cov; out->interior=clamp01((1.0-q-r->rim_width)/fmax(1.0-r->rim_width,1e-9));
            out->rim=clamp01(cov-out->interior); out->height_or_depth=cov*(1.0-0.35*q); out->feature_id=r->feature_id; out->direction=r->tangent; }
    }
    return true;
}

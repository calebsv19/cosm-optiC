/* App-owned immutable resource cache. Scene updates are serialized at the existing
 * update boundary. Shading sees only borrowed immutable pyramid descriptors. */
#define SAMPLING_CACHE_MAX_ENTRIES 2048u
typedef struct SamplingResource {
    bool used;
    unsigned references;
    unsigned long long touched, validation_epoch;
    char key[96];
    CoreAuthoredSurfacePyramid pyramid;
} SamplingResource;
static SamplingResource sampling_resources[SAMPLING_CACHE_MAX_ENTRIES];
static RuntimeSurfaceSamplingCacheStats sampling_cache_stats;
static size_t sampling_cache_budget = SAMPLING_MAX_BYTES;
static unsigned long long sampling_cache_clock, sampling_validation_epoch;
static bool sampling_validating;
static char sampling_scene_context[PATH_MAX];
static size_t sampling_resource_bytes(const SamplingResource *r) {
    return r->pyramid.value_count * sizeof(float);
}
static void sampling_resource_release(SamplingResource *r) {
    if (r && r->references) {
        --r->references;
        r->touched = ++sampling_cache_clock;
    }
}
static void sampling_resource_evict(SamplingResource *r) {
    if (!r || !r->used || r->references)
        return;
    sampling_cache_stats.bytes -= sampling_resource_bytes(r);
    core_authored_surface_pyramid_free(&r->pyramid);
    memset(r, 0, sizeof(*r));
    ++sampling_cache_stats.evictions;
}
static SamplingResource *sampling_resource_oldest(void) {
    SamplingResource *oldest = NULL;
    for (size_t i = 0; i < SAMPLING_CACHE_MAX_ENTRIES; ++i) {
        SamplingResource *r = &sampling_resources[i];
        if (r->used && !r->references && (!oldest || r->touched < oldest->touched))
            oldest = r;
    }
    return oldest;
}
/* Reserve space before allocating a new pyramid. Only inactive entries may leave. */
static SamplingResource *sampling_resource_slot(size_t bytes) {
    if (bytes > sampling_cache_budget)
        return NULL;
    while (sampling_cache_stats.bytes > sampling_cache_budget - bytes) {
        SamplingResource *oldest = sampling_resource_oldest();
        if (!oldest)
            return NULL;
        sampling_resource_evict(oldest);
    }
    for (size_t i = 0; i < SAMPLING_CACHE_MAX_ENTRIES; ++i)
        if (!sampling_resources[i].used)
            return &sampling_resources[i];
    SamplingResource *oldest = sampling_resource_oldest();
    if (oldest)
        sampling_resource_evict(oldest);
    return oldest;
}
static SamplingResource *sampling_resource_retain(SamplingResource *r) {
    ++r->references;
    r->touched = ++sampling_cache_clock;
    /* Keep every unique validation candidate alive until the complete scene has
     * been checked, then leave it eligible for reuse by immediate adoption. */
    if (sampling_validating && r->validation_epoch != sampling_validation_epoch) {
        r->validation_epoch = sampling_validation_epoch;
        ++r->references;
    }
    return r;
}
static SamplingResource *sampling_resource_find(const char *key) {
    for (size_t i = 0; i < SAMPLING_CACHE_MAX_ENTRIES; ++i) {
        SamplingResource *r = &sampling_resources[i];
        if (r->used && !strcmp(r->key, key)) {
            ++sampling_cache_stats.cache_hits;
            return sampling_resource_retain(r);
        }
    }
    return NULL;
}
static SamplingResource *sampling_resource_publish(SamplingResource *slot, const char *key,
                                                   CoreAuthoredSurfacePyramid *owned) {
    memset(slot, 0, sizeof(*slot));
    slot->used = true;
    snprintf(slot->key, sizeof(slot->key), "%s", key);
    slot->pyramid = *owned;
    memset(owned, 0, sizeof(*owned));
    sampling_cache_stats.bytes += sampling_resource_bytes(slot);
    return sampling_resource_retain(slot);
}
static size_t sampling_pyramid_bytes(unsigned width, unsigned height, unsigned channels) {
    size_t values = 0;
    do {
        values += (size_t)width * height * channels;
        if (width == 1 && height == 1)
            break;
        if (width > 1)
            width /= 2;
        if (height > 1)
            height /= 2;
    } while (true);
    return values * sizeof(float);
}
static void sampling_validation_begin(void) {
    ++sampling_validation_epoch;
    sampling_validating = true;
}
static size_t sampling_validation_bytes(void) {
    size_t bytes = 0;
    for (size_t i = 0; i < SAMPLING_CACHE_MAX_ENTRIES; ++i)
        if (sampling_resources[i].used &&
            sampling_resources[i].validation_epoch == sampling_validation_epoch)
            bytes += sampling_resource_bytes(&sampling_resources[i]);
    return bytes;
}
static void sampling_validation_end(void) {
    sampling_validating = false;
    for (size_t i = 0; i < SAMPLING_CACHE_MAX_ENTRIES; ++i)
        if (sampling_resources[i].used &&
            sampling_resources[i].validation_epoch == sampling_validation_epoch)
            sampling_resource_release(&sampling_resources[i]);
}
void RuntimeSurfaceSamplingGetCacheStats(RuntimeSurfaceSamplingCacheStats *out) {
    if (!out)
        return;
    *out = sampling_cache_stats;
    out->budget_bytes = sampling_cache_budget;
    out->entries = out->live_entries = out->live_bytes = 0;
    for (size_t i = 0; i < SAMPLING_CACHE_MAX_ENTRIES; ++i) {
        SamplingResource *r = &sampling_resources[i];
        if (!r->used)
            continue;
        ++out->entries;
        if (r->references) {
            ++out->live_entries;
            out->live_bytes += sampling_resource_bytes(r);
        }
    }
}
void RuntimeSurfaceSamplingResetCacheStats(void) {
    size_t bytes = sampling_cache_stats.bytes;
    memset(&sampling_cache_stats, 0, sizeof(sampling_cache_stats));
    sampling_cache_stats.bytes = bytes;
}
void RuntimeSurfaceSamplingPurgeUnusedResources(void) {
    for (size_t i = 0; i < SAMPLING_CACHE_MAX_ENTRIES; ++i)
        sampling_resource_evict(&sampling_resources[i]);
}
bool RuntimeSurfaceSamplingSetCacheBudgetForTests(size_t bytes) {
    RuntimeSurfaceSamplingCacheStats stats;
    RuntimeSurfaceSamplingGetCacheStats(&stats);
    if (!bytes || bytes > SAMPLING_MAX_BYTES || bytes < stats.live_bytes)
        return false;
    sampling_cache_budget = bytes;
    while (sampling_cache_stats.bytes > bytes)
        sampling_resource_evict(sampling_resource_oldest());
    return true;
}
bool RuntimeSurfaceSamplingCacheResetForTests(void) {
    RuntimeSurfaceSamplingCacheStats stats;
    RuntimeSurfaceSamplingGetCacheStats(&stats);
    if (stats.live_entries)
        return false;
    RuntimeSurfaceSamplingPurgeUnusedResources();
    RuntimeSurfaceSamplingResetCacheStats();
    sampling_cache_budget = SAMPLING_MAX_BYTES;
    return true;
}
bool RuntimeSurfaceSamplingSetScenePathContext(const char *scene_path) {
    if (!scene_path || !scene_path[0]) {
        sampling_scene_context[0] = 0;
        return true;
    }
    char absolute[PATH_MAX];
    const char *path = scene_path;
    if (path[0] != '/') {
        if (!realpath(path, absolute))
            return false;
        path = absolute;
    }
    if (strlen(path) >= sizeof(sampling_scene_context))
        return false;
    snprintf(sampling_scene_context, sizeof(sampling_scene_context), "%s", path);
    return true;
}
const char *RuntimeSurfaceSamplingScenePathContext(void) { return sampling_scene_context; }
static bool sampling_resource_path(const char *declared, char out[PATH_MAX]) {
    if (!declared || !declared[0])
        return false;
    if (declared[0] == '/') {
        if (strlen(declared) >= PATH_MAX)
            return false;
        snprintf(out, PATH_MAX, "%s", declared);
        return true;
    }
    if (!sampling_scene_context[0])
        return false;
    for (const char *part = declared; *part;) {
        const char *end = strchr(part, '/');
        size_t length = end ? (size_t)(end - part) : strlen(part);
        if (length == 2 && part[0] == '.' && part[1] == '.')
            return false;
        if (!end)
            break;
        part = end + 1;
    }
    char parent[PATH_MAX], root[PATH_MAX], joined[PATH_MAX];
    snprintf(parent, sizeof(parent), "%s", sampling_scene_context);
    char *slash = strrchr(parent, '/');
    if (!slash)
        return false;
    if (slash == parent)
        slash[1] = 0;
    else
        *slash = 0;
    if (!realpath(parent, root) ||
        snprintf(joined, sizeof(joined), "%s/%s", root, declared) >= (int)sizeof(joined) ||
        !realpath(joined, out))
        return false;
    size_t length = strlen(root);
    return !strncmp(out, root, length) && (length == 1 || out[length] == '/');
}
/* Cache identity follows the evaluator's normalized semantic values, not padding,
 * cosmetic layer names, object IDs, transforms or channel strengths. Layer IDs
 * are included because they contribute to procedural seed identity. */
static bool sampling_program_key(const RuntimeMaterialTextureStack *stack,
                                 const RuntimeMaterialSurfaceEval *base, const double period[2],
                                 uint32_t seed, char out[96]) {
    struct {
        double header[12];
        struct {
            double values[28];
            char id[RUNTIME_MATERIAL_TEXTURE_LAYER_ID_SIZE];
        } layers[RUNTIME_MATERIAL_TEXTURE_STACK_MAX_LAYERS];
    } key;
    memset(&key, 0, sizeof(key));
    RuntimeMaterialTextureStack normalized = RuntimeMaterialTextureStackNormalize(*stack);
    double head[] = {1,
                     seed,
                     period[0],
                     period[1],
                     base->colorR,
                     base->colorG,
                     base->colorB,
                     base->roughness,
                     base->reflectivity,
                     base->specWeight,
                     base->diffuseWeight,
                     base->transparency};
    memcpy(key.header, head, sizeof(head));
    for (int i = 0; i < normalized.layerCount; ++i) {
        const RuntimeMaterialTextureLayer *l = &normalized.layers[i];
        double values[] = {1,
                           l->enabled,
                           l->kind,
                           l->role,
                           l->blendMode,
                           l->opacity,
                           l->placement.offsetU,
                           l->placement.offsetV,
                           l->placement.scale,
                           l->placement.strength,
                           l->placement.rotation,
                           l->params.patternMode,
                           l->params.coverage,
                           l->params.grain,
                           l->params.edgeSoftness,
                           l->params.contrast,
                           l->params.flow,
                           l->params.colorDepth,
                           l->params.surfaceDamage,
                           l->params.seed,
                           l->roughnessInfluence,
                           l->reflectivityInfluence,
                           l->specularInfluence,
                           l->diffuseInfluence,
                           l->transparencyInfluence};
        memcpy(key.layers[i].values, values, sizeof(values));
        snprintf(key.layers[i].id, sizeof(key.layers[i].id), "%s", l->layerId);
    }
    char digest[65];
    if (!ray_tracing_sha256_bytes(&key, sizeof(key), digest))
        return false;
    snprintf(out, 96, "program-v1:%s", digest);
    return true;
}
/* Candidate-only budget estimate. Actual staging additionally accounts for any
 * previous generation still held alive. Unknown row metadata does not multiply
 * identical program estimates across instances. */
static bool sampling_program_estimate_key(json_object *object, json_object *row,
                                          const double period[2], char out[65]) {
    RuntimeMaterialTextureStack stack;
    RuntimeMaterialSurfaceEval base = {0};
    CoreAuthoredSurfaceMapping map;
    char program[96];
    if (!RuntimeSurfaceMaterialCompileSource(row, &stack) || !parse(mapping(object), &map) ||
        !sampling_program_key(&stack, &base, period, map.seed, program))
        return false;
    json_object *identity = json_object_new_object();
    if (!identity)
        return false;
    json_object_object_add(identity, "program", json_object_new_string(program));
    const char *keys[] = {"material_id", "object_color", "roughness",        "reflectivity",
                          "alpha",       "transparency", "emissive_strength"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(*keys); ++i)
        if (field(row, keys[i]))
            json_object_object_add(identity, keys[i], json_object_get(field(row, keys[i])));
    if (field(object, "material_ref"))
        json_object_object_add(identity, "material_ref",
                               json_object_get(field(object, "material_ref")));
    const char *json = json_object_to_json_string_ext(identity, JSON_C_TO_STRING_PLAIN);
    bool ok = ray_tracing_sha256_bytes(json, strlen(json), out);
    json_object_put(identity);
    return ok;
}

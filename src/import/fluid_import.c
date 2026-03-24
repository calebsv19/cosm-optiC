#include "import/fluid_import.h"
#include "import/fluid_pack_import.h"
#include "import/scene_bundle_import.h"
#include "core_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "cJSON.h"

typedef struct VolumeFrameHeaderV2 {
    uint32_t magic;
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double   time_seconds;
    uint64_t frame_index;
    double   dt_seconds;
    float    origin_x;
    float    origin_y;
    float    cell_size;
    uint32_t obstacle_mask_crc32;
    uint32_t reserved[3];
} VolumeFrameHeaderV2;

typedef struct VolumeFrameHeaderV1 {
    uint32_t magic;
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double   time_seconds;
    uint64_t frame_index;
} VolumeFrameHeaderV1;

static const uint32_t VOLUME_MAGIC = ('V' << 24) | ('F' << 16) | ('R' << 8) | ('M');
static const uint32_t VOLUME_VERSION_V2 = 2;
static const uint32_t VOLUME_VERSION_V1 = 1;

static bool file_exists(const char *path) {
    if (!path || !path[0]) return false;
    return core_io_path_exists(path);
}

static bool has_extension(const char *path, const char *ext) {
    if (!path || !ext) return false;
    size_t path_len = strlen(path);
    size_t ext_len = strlen(ext);
    if (path_len < ext_len) return false;
    return strcmp(path + path_len - ext_len, ext) == 0;
}

static char *dup_string(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

static char *join_path(const char *dir, const char *file) {
    if (!dir || !file) return NULL;
    size_t len_dir = strlen(dir);
    size_t len_file = strlen(file);
    int needs_slash = (len_dir > 0 && dir[len_dir - 1] != '/');
    size_t total = len_dir + needs_slash + len_file + 1;
    char *buf = (char *)malloc(total);
    if (!buf) return NULL;
    memcpy(buf, dir, len_dir);
    if (needs_slash) buf[len_dir] = '/';
    memcpy(buf + len_dir + (needs_slash ? 1 : 0), file, len_file + 1);
    return buf;
}

static char *resolve_relative_upwards(const char *base_dir, const char *relative) {
    if (!base_dir || !relative) return NULL;
    char *candidate = join_path(base_dir, relative);
    if (candidate && file_exists(candidate)) return candidate;
    free(candidate);

    // Walk up the directory tree looking for a match (helps when manifest is inside export/volume_frames/<preset>/)
    char dirbuf[PATH_MAX];
    strncpy(dirbuf, base_dir, sizeof(dirbuf) - 1);
    dirbuf[sizeof(dirbuf) - 1] = '\0';
    while (true) {
        char *slash = strrchr(dirbuf, '/');
        if (!slash) break;
        *slash = '\0';
        char *joined = join_path(dirbuf, relative);
        if (joined && file_exists(joined)) {
            return joined;
        }
        free(joined);
    }
    return NULL;
}

static bool read_header(const char *path, FluidFrameMeta *out_meta) {
    if (!path || !out_meta) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    uint32_t magic = 0;
    if (fread(&magic, sizeof(magic), 1, f) != 1) { fclose(f); return false; }
    if (magic != VOLUME_MAGIC) { fclose(f); return false; }
    uint32_t version = 0;
    if (fread(&version, sizeof(version), 1, f) != 1) { fclose(f); return false; }

    memset(out_meta, 0, sizeof(*out_meta));
    out_meta->version = version;
    if (version == VOLUME_VERSION_V2) {
        VolumeFrameHeaderV2 h = {0};
        h.magic = magic;
        h.version = version;
        if (fread(&h.grid_w, sizeof(h) - sizeof(h.magic) - sizeof(h.version), 1, f) != 1) {
            fclose(f);
            return false;
        }
        out_meta->grid_w = h.grid_w;
        out_meta->grid_h = h.grid_h;
        out_meta->time_seconds = h.time_seconds;
        out_meta->frame_index = h.frame_index;
        out_meta->dt_seconds = h.dt_seconds;
        out_meta->origin_x = h.origin_x;
        out_meta->origin_y = h.origin_y;
        out_meta->cell_size = h.cell_size;
        out_meta->obstacle_mask_crc32 = h.obstacle_mask_crc32;
        fclose(f);
        return true;
    } else if (version == VOLUME_VERSION_V1) {
        VolumeFrameHeaderV1 h = {0};
        h.magic = magic;
        h.version = version;
        if (fread(&h.grid_w, sizeof(h) - sizeof(h.magic) - sizeof(h.version), 1, f) != 1) {
            fclose(f);
            return false;
        }
        out_meta->grid_w = h.grid_w;
        out_meta->grid_h = h.grid_h;
        out_meta->time_seconds = h.time_seconds;
        out_meta->frame_index = h.frame_index;
        out_meta->dt_seconds = 0.0;
        out_meta->origin_x = 0.0f;
        out_meta->origin_y = 0.0f;
        out_meta->cell_size = 1.0f;
        out_meta->obstacle_mask_crc32 = 0;
        fclose(f);
        return true;
    }
    fclose(f);
    return false;
}

static bool fluid_frame_load_vf2d(const char *path, FluidFrame *out) {
    if (!path || !out) return false;
    FluidFrameMeta meta;
    if (!read_header(path, &meta)) return false;

    FILE *f = fopen(path, "rb");
    if (!f) return false;
    // skip header already parsed
    if (meta.version == VOLUME_VERSION_V2) {
        fseek(f, (long)sizeof(VolumeFrameHeaderV2), SEEK_SET);
    } else {
        fseek(f, (long)sizeof(VolumeFrameHeaderV1), SEEK_SET);
    }

    size_t count = (size_t)meta.grid_w * (size_t)meta.grid_h;
    FluidFrame frame = {0};
    frame.w = (int)meta.grid_w;
    frame.h = (int)meta.grid_h;
    frame.meta = meta;

    frame.density = (float *)malloc(sizeof(float) * count);
    frame.velX = (float *)malloc(sizeof(float) * count);
    frame.velY = (float *)malloc(sizeof(float) * count);
    if (!frame.density || !frame.velX || !frame.velY) {
        fluid_frame_free(&frame);
        fclose(f);
        return false;
    }

    if (fread(frame.density, sizeof(float), count, f) != count ||
        fread(frame.velX, sizeof(float), count, f) != count ||
        fread(frame.velY, sizeof(float), count, f) != count) {
        fluid_frame_free(&frame);
        fclose(f);
        return false;
    }

    fclose(f);
    *out = frame;
    return true;
}

bool fluid_frame_load(const char *path, FluidFrame *out) {
    if (!path || !out) return false;
    SceneBundleImportResult bundle;
    if (scene_bundle_import_resolve_fluid_source(path, &bundle)) {
        if (strcmp(bundle.fluid_source_path, path) == 0) return false;
        return fluid_frame_load(bundle.fluid_source_path, out);
    }
    if (fluid_pack_path_is_pack(path)) {
        if (fluid_pack_frame_load(path, out)) {
            return true;
        }
        char legacy_vf2d_path[PATH_MAX];
        if (fluid_pack_derive_legacy_vf2d_path(path, legacy_vf2d_path, sizeof(legacy_vf2d_path))) {
            return fluid_frame_load_vf2d(legacy_vf2d_path, out);
        }
        return false;
    }
    return fluid_frame_load_vf2d(path, out);
}

bool fluid_frame_load_single(const char *path, FluidFrame *out) {
    return fluid_frame_load(path, out);
}

void fluid_frame_free(FluidFrame *frame) {
    if (!frame) return;
    free(frame->density);
    free(frame->velX);
    free(frame->velY);
    frame->density = frame->velX = frame->velY = NULL;
    frame->w = frame->h = 0;
    memset(&frame->meta, 0, sizeof(frame->meta));
}

static char *dir_of(const char *path) {
    if (!path) return NULL;
    const char *slash = strrchr(path, '/');
    if (!slash) return dup_string(".");
    size_t len = (size_t)(slash - path);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, path, len);
    out[len] = '\0';
    return out;
}

bool fluid_manifest_load(const char *manifest_path, FluidManifest *out) {
    if (!manifest_path || !out) return false;
    SceneBundleImportResult bundle;
    if (scene_bundle_import_resolve_fluid_source(manifest_path, &bundle)) {
        if (strcmp(bundle.fluid_source_path, manifest_path) == 0) return false;
        return fluid_manifest_load(bundle.fluid_source_path, out);
    }
    if (has_extension(manifest_path, ".pack") || has_extension(manifest_path, ".vf2d")) {
        FluidFrame probe = {0};
        if (!fluid_frame_load(manifest_path, &probe)) return false;

        FluidManifest manifest = {0};
        manifest.paths = (char **)calloc(1u, sizeof(char *));
        manifest.meta = (FluidFrameMeta *)calloc(1u, sizeof(FluidFrameMeta));
        if (!manifest.paths || !manifest.meta) {
            fluid_manifest_free(&manifest);
            fluid_frame_free(&probe);
            return false;
        }

        manifest.paths[0] = dup_string(manifest_path);
        if (!manifest.paths[0]) {
            fluid_manifest_free(&manifest);
            fluid_frame_free(&probe);
            return false;
        }

        manifest.meta[0] = probe.meta;
        manifest.count = 1;
        manifest.grid_w = (uint32_t)probe.w;
        manifest.grid_h = (uint32_t)probe.h;
        manifest.cell_size = probe.meta.cell_size;
        manifest.origin_x = probe.meta.origin_x;
        manifest.origin_y = probe.meta.origin_y;
        manifest.obstacle_mask_crc32 = probe.meta.obstacle_mask_crc32;
        manifest.space_contract_version = 0;
        manifest.space_author_window_w = 1200;
        manifest.space_author_window_h = 800;
        manifest.space_desired_fit = 0.25f;
        fluid_frame_free(&probe);
        *out = manifest;
        return true;
    }

    CoreBuffer manifest_data = {0};
    CoreResult read_r = core_io_read_all(manifest_path, &manifest_data);
    if (read_r.code != CORE_OK || !manifest_data.data || manifest_data.size == 0u) return false;
    char *buf = (char *)malloc(manifest_data.size + 1u);
    if (!buf) {
        core_io_buffer_free(&manifest_data);
        return false;
    }
    memcpy(buf, manifest_data.data, manifest_data.size);
    buf[manifest_data.size] = '\0';
    core_io_buffer_free(&manifest_data);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return false;

    FluidManifest manifest = {0};
    const cJSON *grid_w = cJSON_GetObjectItem(root, "grid_w");
    const cJSON *grid_h = cJSON_GetObjectItem(root, "grid_h");
    const cJSON *cell_size = cJSON_GetObjectItem(root, "cell_size");
    const cJSON *origin_x = cJSON_GetObjectItem(root, "origin_x");
    const cJSON *origin_y = cJSON_GetObjectItem(root, "origin_y");
    const cJSON *crc = cJSON_GetObjectItem(root, "obstacle_mask_crc32");
    manifest.grid_w = (grid_w && cJSON_IsNumber(grid_w)) ? (uint32_t)grid_w->valuedouble : 0;
    manifest.grid_h = (grid_h && cJSON_IsNumber(grid_h)) ? (uint32_t)grid_h->valuedouble : 0;
    manifest.cell_size = (cell_size && cJSON_IsNumber(cell_size)) ? (float)cell_size->valuedouble : 1.0f;
    manifest.origin_x = (origin_x && cJSON_IsNumber(origin_x)) ? (float)origin_x->valuedouble : 0.0f;
    manifest.origin_y = (origin_y && cJSON_IsNumber(origin_y)) ? (float)origin_y->valuedouble : 0.0f;
    manifest.obstacle_mask_crc32 = (crc && cJSON_IsNumber(crc)) ? (uint32_t)crc->valuedouble : 0;
    manifest.space_contract_version = 0;
    manifest.space_author_window_w = 1200;
    manifest.space_author_window_h = 800;
    manifest.space_desired_fit = 0.25f;

    cJSON *space_contract = cJSON_GetObjectItem(root, "space_contract");
    if (cJSON_IsObject(space_contract)) {
        const cJSON *sc_version = cJSON_GetObjectItem(space_contract, "version");
        const cJSON *sc_grid_w = cJSON_GetObjectItem(space_contract, "grid_w");
        const cJSON *sc_grid_h = cJSON_GetObjectItem(space_contract, "grid_h");
        const cJSON *sc_origin_x = cJSON_GetObjectItem(space_contract, "origin_x");
        const cJSON *sc_origin_y = cJSON_GetObjectItem(space_contract, "origin_y");
        const cJSON *sc_cell_size = cJSON_GetObjectItem(space_contract, "cell_size");
        const cJSON *sc_author_w = cJSON_GetObjectItem(space_contract, "author_window_w");
        const cJSON *sc_author_h = cJSON_GetObjectItem(space_contract, "author_window_h");
        const cJSON *sc_fit = cJSON_GetObjectItem(space_contract, "import_fit");

        if (sc_version && cJSON_IsNumber(sc_version)) {
            manifest.space_contract_version = (uint32_t)sc_version->valuedouble;
        }
        if (sc_grid_w && cJSON_IsNumber(sc_grid_w)) manifest.grid_w = (uint32_t)sc_grid_w->valuedouble;
        if (sc_grid_h && cJSON_IsNumber(sc_grid_h)) manifest.grid_h = (uint32_t)sc_grid_h->valuedouble;
        if (sc_origin_x && cJSON_IsNumber(sc_origin_x)) manifest.origin_x = (float)sc_origin_x->valuedouble;
        if (sc_origin_y && cJSON_IsNumber(sc_origin_y)) manifest.origin_y = (float)sc_origin_y->valuedouble;
        if (sc_cell_size && cJSON_IsNumber(sc_cell_size)) manifest.cell_size = (float)sc_cell_size->valuedouble;
        if (sc_author_w && cJSON_IsNumber(sc_author_w)) manifest.space_author_window_w = (int)sc_author_w->valuedouble;
        if (sc_author_h && cJSON_IsNumber(sc_author_h)) manifest.space_author_window_h = (int)sc_author_h->valuedouble;
        if (sc_fit && cJSON_IsNumber(sc_fit)) manifest.space_desired_fit = (float)sc_fit->valuedouble;

        if (manifest.space_author_window_w <= 0) manifest.space_author_window_w = 1200;
        if (manifest.space_author_window_h <= 0) manifest.space_author_window_h = 800;
        if (manifest.space_desired_fit <= 0.0f) manifest.space_desired_fit = 0.25f;
    }

    cJSON *frames = cJSON_GetObjectItem(root, "frames");
    if (!cJSON_IsArray(frames)) { cJSON_Delete(root); return false; }
    int frame_count = cJSON_GetArraySize(frames);
    if (frame_count < 0) frame_count = 0;
    if (frame_count > 0) {
        manifest.paths = (char **)calloc((size_t)frame_count, sizeof(char *));
        manifest.meta = (FluidFrameMeta *)calloc((size_t)frame_count, sizeof(FluidFrameMeta));
        if (!manifest.paths || !manifest.meta) {
            fluid_manifest_free(&manifest);
            cJSON_Delete(root);
            return false;
        }
    }
    manifest.count = (size_t)frame_count;

    char *dir = dir_of(manifest_path);
    for (int i = 0; i < frame_count; ++i) {
        cJSON *entry = cJSON_GetArrayItem(frames, i);
        if (!cJSON_IsObject(entry)) continue;
        const cJSON *path_item = cJSON_GetObjectItem(entry, "path");
        const cJSON *frame_index = cJSON_GetObjectItem(entry, "frame_index");
        const cJSON *time_seconds = cJSON_GetObjectItem(entry, "time_seconds");
        const cJSON *dt_seconds = cJSON_GetObjectItem(entry, "dt_seconds");
        if (!cJSON_IsString(path_item)) continue;
        char *resolved = NULL;
        const char *raw = path_item->valuestring;
        if (raw[0] == '/') {
            resolved = dup_string(raw);
        } else {
            resolved = join_path(dir ? dir : ".", raw);
        }
        manifest.paths[i] = resolved;
        manifest.meta[i].version = VOLUME_VERSION_V2;
        manifest.meta[i].grid_w = manifest.grid_w;
        manifest.meta[i].grid_h = manifest.grid_h;
        manifest.meta[i].frame_index = frame_index && cJSON_IsNumber(frame_index) ? (uint64_t)frame_index->valuedouble : 0;
        manifest.meta[i].time_seconds = time_seconds && cJSON_IsNumber(time_seconds) ? time_seconds->valuedouble : 0.0;
        manifest.meta[i].dt_seconds = dt_seconds && cJSON_IsNumber(dt_seconds) ? dt_seconds->valuedouble : 0.0;
        manifest.meta[i].origin_x = manifest.origin_x;
        manifest.meta[i].origin_y = manifest.origin_y;
        manifest.meta[i].cell_size = manifest.cell_size;
        manifest.meta[i].obstacle_mask_crc32 = manifest.obstacle_mask_crc32;
    }
    // Optional imports
    cJSON *imports = cJSON_GetObjectItem(root, "imports");
    if (cJSON_IsArray(imports)) {
        int icount = cJSON_GetArraySize(imports);
        if (icount > 0) {
            manifest.imports = (FluidImportShape *)calloc((size_t)icount, sizeof(FluidImportShape));
            manifest.import_count = (size_t)icount;
            for (int i = 0; i < icount; ++i) {
                cJSON *item = cJSON_GetArrayItem(imports, i);
                if (!cJSON_IsObject(item)) continue;
                FluidImportShape *dst = &manifest.imports[i];
                const cJSON *p = cJSON_GetObjectItem(item, "path");
                if (cJSON_IsString(p) && p->valuestring) {
                    const char *raw = p->valuestring;
                    if (raw[0] == '/') {
                        dst->path = dup_string(raw);
                    } else {
                        char *resolved = resolve_relative_upwards(dir ? dir : ".", raw);
                        if (resolved) {
                            dst->path = resolved;
                        } else {
                            dst->path = dup_string(raw);
                        }
                    }
                }
                const cJSON *px = cJSON_GetObjectItem(item, "pos_x_norm");
                const cJSON *py = cJSON_GetObjectItem(item, "pos_y_norm");
                const cJSON *rot = cJSON_GetObjectItem(item, "rotation_deg");
                const cJSON *s = cJSON_GetObjectItem(item, "scale");
                const cJSON *st = cJSON_GetObjectItem(item, "is_static");
                dst->pos_x_norm = (px && cJSON_IsNumber(px)) ? (float)px->valuedouble : 0.0f;
                dst->pos_y_norm = (py && cJSON_IsNumber(py)) ? (float)py->valuedouble : 0.0f;
                dst->rotation_deg = (rot && cJSON_IsNumber(rot)) ? (float)rot->valuedouble : 0.0f;
                dst->scale = (s && cJSON_IsNumber(s)) ? (float)s->valuedouble : 1.0f;
                dst->is_static = (st && cJSON_IsBool(st)) ? (cJSON_IsTrue(st) ? 1 : 0) : 0;
            }
        }
    }
    free(dir);
    cJSON_Delete(root);
    *out = manifest;
    return true;
}

void fluid_manifest_free(FluidManifest *manifest) {
    if (!manifest) return;
    if (manifest->paths) {
        for (size_t i = 0; i < manifest->count; ++i) {
            free(manifest->paths[i]);
        }
        free(manifest->paths);
        manifest->paths = NULL;
    }
    free(manifest->meta);
    manifest->meta = NULL;
    if (manifest->imports) {
        for (size_t i = 0; i < manifest->import_count; ++i) {
            free(manifest->imports[i].path);
        }
        free(manifest->imports);
        manifest->imports = NULL;
    }
    manifest->count = 0;
    manifest->grid_w = manifest->grid_h = 0;
    manifest->cell_size = 0.0f;
    manifest->origin_x = manifest->origin_y = 0.0f;
    manifest->obstacle_mask_crc32 = 0;
    manifest->space_contract_version = 0;
    manifest->space_author_window_w = 0;
    manifest->space_author_window_h = 0;
    manifest->space_desired_fit = 0.0f;
    manifest->import_count = 0;
}

#include "app/ray_tracing_headless_job_bundle.h"
#include "app/ray_tracing_request_utils.h"

#include <json-c/json.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static bool parent_dir_of(const char *path, char *out_dir, size_t out_dir_size) {
    if (!path || !path[0] || !out_dir || out_dir_size == 0u) return false;
    RayTracingDirnameOf(path, out_dir, out_dir_size);
    return out_dir[0] != '\0';
}

static bool ensure_directory_exists(const char *path) {
    char tmp[PATH_MAX];
    size_t len = 0u;

    if (!path || !path[0]) return false;
    len = strlen(path);
    if (len >= sizeof(tmp)) return false;
    memcpy(tmp, path, len + 1u);

    for (size_t i = 1u; i < len; ++i) {
        if (tmp[i] != '/') continue;
        tmp[i] = '\0';
        if (tmp[0] != '\0' && mkdir(tmp, 0700) != 0 && access(tmp, F_OK) != 0) {
            return false;
        }
        tmp[i] = '/';
    }

    if (mkdir(tmp, 0700) != 0 && access(tmp, F_OK) != 0) {
        return false;
    }
    return true;
}

static bool ensure_parent_directory_exists(const char *path) {
    char dir[PATH_MAX];
    if (!parent_dir_of(path, dir, sizeof(dir))) return false;
    return ensure_directory_exists(dir);
}

static bool load_payload_ref(json_object *owner,
                             const char *key,
                             CoreHeadlessJobPayloadRef *out_payload) {
    json_object *obj = NULL;
    const char *text = NULL;
    if (!owner || !key || !out_payload || !RayTracingJsonGetObject(owner, key, &obj)) return false;
    memset(out_payload, 0, sizeof(*out_payload));
    if (!RayTracingJsonGetString(obj, "schema_family", &text) ||
        !RayTracingCopyString(out_payload->schema_family, sizeof(out_payload->schema_family), text)) {
        return false;
    }
    if (!RayTracingJsonGetString(obj, "schema_variant", &text) ||
        !RayTracingCopyString(out_payload->schema_variant, sizeof(out_payload->schema_variant), text)) {
        return false;
    }
    if (!RayTracingJsonGetString(obj, "path", &text) ||
        !RayTracingCopyString(out_payload->path, sizeof(out_payload->path), text)) {
        return false;
    }
    return true;
}

bool ray_tracing_headless_job_bundle_load(const char *job_json_path,
                                          RayTracingHeadlessJobBundle *out_bundle,
                                          char *out_diagnostics,
                                          size_t out_diagnostics_size) {
    json_object *root = NULL;
    json_object *tool = NULL;
    json_object *outputs = NULL;
    json_object *metadata = NULL;
    const char *text = NULL;
    RayTracingHeadlessJobBundle bundle;

    RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!job_json_path || !job_json_path[0] || !out_bundle) return false;

    memset(&bundle, 0, sizeof(bundle));
    core_headless_job_envelope_init(&bundle.envelope);
    if (!RayTracingCopyString(bundle.bundle_path, sizeof(bundle.bundle_path), job_json_path) ||
        !parent_dir_of(job_json_path, bundle.bundle_dir, sizeof(bundle.bundle_dir))) {
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "failed to derive bundle directory");
        return false;
    }

    root = json_object_from_file(job_json_path);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "failed to parse outer job json");
        return false;
    }

    if (!RayTracingJsonGetString(root, "schema_family", &text) ||
        !RayTracingCopyString(bundle.envelope.schema_family, sizeof(bundle.envelope.schema_family), text) ||
        !RayTracingJsonGetString(root, "schema_variant", &text) ||
        !RayTracingCopyString(bundle.envelope.schema_variant, sizeof(bundle.envelope.schema_variant), text)) {
        json_object_put(root);
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "missing shared job schema identifiers");
        return false;
    }
    if (strcmp(bundle.envelope.schema_family, "codework_job") != 0 ||
        strcmp(bundle.envelope.schema_variant, "headless_bundle_v1") != 0) {
        json_object_put(root);
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "request is not a codework headless bundle");
        return false;
    }
    if (!RayTracingJsonGetString(root, "job_id", &text) ||
        !RayTracingCopyString(bundle.envelope.job_id, sizeof(bundle.envelope.job_id), text) ||
        !RayTracingJsonGetString(root, "program", &text) ||
        !RayTracingCopyString(bundle.envelope.program, sizeof(bundle.envelope.program), text)) {
        json_object_put(root);
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "missing outer job identity");
        return false;
    }
    if (strcmp(bundle.envelope.program, "ray_tracing") != 0) {
        json_object_put(root);
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "outer job program must be ray_tracing");
        return false;
    }

    if (!RayTracingJsonGetObject(root, "tool", &tool) ||
        !RayTracingJsonGetString(tool, "name", &text) ||
        !RayTracingCopyString(bundle.envelope.tool.name, sizeof(bundle.envelope.tool.name), text) ||
        !RayTracingJsonGetString(tool, "version", &text) ||
        !RayTracingCopyString(bundle.envelope.tool.version, sizeof(bundle.envelope.tool.version), text) ||
        !RayTracingJsonGetString(tool, "target_os", &text) ||
        !RayTracingCopyString(bundle.envelope.tool.target_os, sizeof(bundle.envelope.tool.target_os), text) ||
        !RayTracingJsonGetString(tool, "target_arch", &text) ||
        !RayTracingCopyString(bundle.envelope.tool.target_arch, sizeof(bundle.envelope.tool.target_arch), text)) {
        json_object_put(root);
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "missing outer tool fields");
        return false;
    }

    if (!load_payload_ref(root, "scene_payload", &bundle.envelope.scene_payload) ||
        !load_payload_ref(root, "run_config", &bundle.envelope.run_config)) {
        json_object_put(root);
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "missing scene payload or run config reference");
        return false;
    }

    if (!RayTracingJsonGetObject(root, "outputs", &outputs) ||
        !RayTracingJsonGetString(outputs, "root", &text) ||
        !RayTracingCopyString(bundle.envelope.outputs.root, sizeof(bundle.envelope.outputs.root), text) ||
        !RayTracingJsonGetString(outputs, "report_path", &text) ||
        !RayTracingCopyString(bundle.envelope.outputs.report_path, sizeof(bundle.envelope.outputs.report_path), text) ||
        !RayTracingJsonGetString(outputs, "logs_dir", &text) ||
        !RayTracingCopyString(bundle.envelope.outputs.logs_dir, sizeof(bundle.envelope.outputs.logs_dir), text) ||
        !RayTracingJsonGetString(outputs, "artifacts_dir", &text) ||
        !RayTracingCopyString(bundle.envelope.outputs.artifacts_dir, sizeof(bundle.envelope.outputs.artifacts_dir), text)) {
        json_object_put(root);
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "missing outer outputs block");
        return false;
    }

    if (RayTracingJsonGetObject(root, "metadata", &metadata)) {
        if (RayTracingJsonGetString(metadata, "title", &text)) {
            (void)RayTracingCopyString(bundle.envelope.metadata.title,
                              sizeof(bundle.envelope.metadata.title),
                              text);
        }
        if (RayTracingJsonGetString(metadata, "description", &text)) {
            (void)RayTracingCopyString(bundle.envelope.metadata.description,
                              sizeof(bundle.envelope.metadata.description),
                              text);
        }
        if (RayTracingJsonGetString(metadata, "created_by", &text)) {
            (void)RayTracingCopyString(bundle.envelope.metadata.created_by,
                              sizeof(bundle.envelope.metadata.created_by),
                              text);
        }
        if (RayTracingJsonGetString(metadata, "created_at", &text)) {
            (void)RayTracingCopyString(bundle.envelope.metadata.created_at,
                              sizeof(bundle.envelope.metadata.created_at),
                              text);
        }
    }

    json_object_put(root);

    if (!RayTracingResolveRequestPath(bundle.bundle_dir,
                             bundle.envelope.scene_payload.path,
                             bundle.resolved_scene_payload_path,
                             sizeof(bundle.resolved_scene_payload_path)) ||
        !RayTracingResolveRequestPath(bundle.bundle_dir,
                             bundle.envelope.run_config.path,
                             bundle.resolved_run_config_path,
                             sizeof(bundle.resolved_run_config_path)) ||
        !RayTracingResolveRequestPath(bundle.bundle_dir,
                             bundle.envelope.outputs.report_path,
                             bundle.resolved_report_path,
                             sizeof(bundle.resolved_report_path)) ||
        !RayTracingResolveRequestPath(bundle.bundle_dir,
                             bundle.envelope.outputs.logs_dir,
                             bundle.resolved_logs_dir,
                             sizeof(bundle.resolved_logs_dir)) ||
        !RayTracingResolveRequestPath(bundle.bundle_dir,
                             bundle.envelope.outputs.artifacts_dir,
                             bundle.resolved_artifacts_dir,
                             sizeof(bundle.resolved_artifacts_dir))) {
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "failed to resolve bundle-relative paths");
        return false;
    }

    if (!core_headless_job_envelope_validate(&bundle.envelope)) {
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "outer job envelope failed validation");
        return false;
    }

    *out_bundle = bundle;
    RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool ray_tracing_headless_job_bundle_write(const char *job_json_path,
                                           const CoreHeadlessJobEnvelope *envelope,
                                           char *out_diagnostics,
                                           size_t out_diagnostics_size) {
    FILE *file = NULL;

    RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!job_json_path || !job_json_path[0] || !envelope) return false;
    if (!core_headless_job_envelope_validate(envelope)) {
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "shared job envelope failed validation");
        return false;
    }
    if (!ensure_parent_directory_exists(job_json_path)) {
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "failed to create job.json parent directory");
        return false;
    }

    file = fopen(job_json_path, "wb");
    if (!file) {
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "failed to open job.json");
        return false;
    }

    fprintf(file, "{\n");
    fprintf(file, "  \"schema_family\": ");
    RayTracingJsonWriteString(file, envelope->schema_family);
    fprintf(file, ",\n");
    fprintf(file, "  \"schema_variant\": ");
    RayTracingJsonWriteString(file, envelope->schema_variant);
    fprintf(file, ",\n");
    fprintf(file, "  \"job_id\": ");
    RayTracingJsonWriteString(file, envelope->job_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"program\": ");
    RayTracingJsonWriteString(file, envelope->program);
    fprintf(file, ",\n");
    fprintf(file, "  \"tool\": {\n");
    fprintf(file, "    \"name\": ");
    RayTracingJsonWriteString(file, envelope->tool.name);
    fprintf(file, ",\n");
    fprintf(file, "    \"version\": ");
    RayTracingJsonWriteString(file, envelope->tool.version);
    fprintf(file, ",\n");
    fprintf(file, "    \"target_os\": ");
    RayTracingJsonWriteString(file, envelope->tool.target_os);
    fprintf(file, ",\n");
    fprintf(file, "    \"target_arch\": ");
    RayTracingJsonWriteString(file, envelope->tool.target_arch);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"scene_payload\": {\n");
    fprintf(file, "    \"schema_family\": ");
    RayTracingJsonWriteString(file, envelope->scene_payload.schema_family);
    fprintf(file, ",\n");
    fprintf(file, "    \"schema_variant\": ");
    RayTracingJsonWriteString(file, envelope->scene_payload.schema_variant);
    fprintf(file, ",\n");
    fprintf(file, "    \"path\": ");
    RayTracingJsonWriteString(file, envelope->scene_payload.path);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"run_config\": {\n");
    fprintf(file, "    \"schema_family\": ");
    RayTracingJsonWriteString(file, envelope->run_config.schema_family);
    fprintf(file, ",\n");
    fprintf(file, "    \"schema_variant\": ");
    RayTracingJsonWriteString(file, envelope->run_config.schema_variant);
    fprintf(file, ",\n");
    fprintf(file, "    \"path\": ");
    RayTracingJsonWriteString(file, envelope->run_config.path);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"outputs\": {\n");
    fprintf(file, "    \"root\": ");
    RayTracingJsonWriteString(file, envelope->outputs.root);
    fprintf(file, ",\n");
    fprintf(file, "    \"report_path\": ");
    RayTracingJsonWriteString(file, envelope->outputs.report_path);
    fprintf(file, ",\n");
    fprintf(file, "    \"logs_dir\": ");
    RayTracingJsonWriteString(file, envelope->outputs.logs_dir);
    fprintf(file, ",\n");
    fprintf(file, "    \"artifacts_dir\": ");
    RayTracingJsonWriteString(file, envelope->outputs.artifacts_dir);
    fprintf(file, "\n  },\n");
    fprintf(file, "  \"metadata\": {\n");
    fprintf(file, "    \"title\": ");
    RayTracingJsonWriteString(file, envelope->metadata.title);
    fprintf(file, ",\n");
    fprintf(file, "    \"description\": ");
    RayTracingJsonWriteString(file, envelope->metadata.description);
    fprintf(file, ",\n");
    fprintf(file, "    \"created_by\": ");
    RayTracingJsonWriteString(file, envelope->metadata.created_by);
    fprintf(file, ",\n");
    fprintf(file, "    \"created_at\": ");
    RayTracingJsonWriteString(file, envelope->metadata.created_at);
    fprintf(file, "\n  }\n");
    fprintf(file, "}\n");
    fclose(file);

    RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool ray_tracing_headless_job_report_write(const char *report_path,
                                           const CoreHeadlessJobReport *report,
                                           const CoreHeadlessJobArtifact *artifacts,
                                           size_t artifact_count,
                                           char *out_diagnostics,
                                           size_t out_diagnostics_size) {
    FILE *file = NULL;

    RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!report_path || !report_path[0] || !report) return false;
    if (!ensure_parent_directory_exists(report_path)) {
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "failed to create report parent directory");
        return false;
    }
    if (!core_headless_job_report_validate(report)) {
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "shared report failed validation");
        return false;
    }

    file = fopen(report_path, "wb");
    if (!file) {
        RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "failed to open report.json");
        return false;
    }

    fprintf(file, "{\n");
    fprintf(file, "  \"schema_family\": ");
    RayTracingJsonWriteString(file, report->schema_family);
    fprintf(file, ",\n");
    fprintf(file, "  \"schema_variant\": ");
    RayTracingJsonWriteString(file, report->schema_variant);
    fprintf(file, ",\n");
    fprintf(file, "  \"job_id\": ");
    RayTracingJsonWriteString(file, report->job_id);
    fprintf(file, ",\n");
    fprintf(file, "  \"program\": ");
    RayTracingJsonWriteString(file, report->program);
    fprintf(file, ",\n");
    fprintf(file, "  \"state\": ");
    RayTracingJsonWriteString(file, report->state);
    fprintf(file, ",\n");
    fprintf(file, "  \"stage\": ");
    RayTracingJsonWriteString(file, report->stage);
    fprintf(file, ",\n");
    fprintf(file, "  \"created_at\": ");
    RayTracingJsonWriteString(file, report->created_at);
    fprintf(file, ",\n");
    fprintf(file, "  \"started_at\": ");
    RayTracingJsonWriteString(file, report->started_at);
    fprintf(file, ",\n");
    fprintf(file, "  \"updated_at\": ");
    RayTracingJsonWriteString(file, report->updated_at);
    fprintf(file, ",\n");
    fprintf(file, "  \"finished_at\": ");
    RayTracingJsonWriteString(file, report->finished_at);
    fprintf(file, ",\n");
    fprintf(file, "  \"artifacts\": [\n");
    for (size_t i = 0u; i < artifact_count; ++i) {
        fprintf(file, "    {\n");
        fprintf(file, "      \"type\": ");
        RayTracingJsonWriteString(file, artifacts[i].type);
        fprintf(file, ",\n");
        fprintf(file, "      \"path\": ");
        RayTracingJsonWriteString(file, artifacts[i].path);
        fprintf(file, "\n    }%s\n", (i + 1u < artifact_count) ? "," : "");
    }
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");
    fclose(file);

    RayTracingRequestSetDiag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

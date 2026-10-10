#!/usr/bin/env python3
"""Exercise production progress merge against cancelled and live records."""
from pathlib import Path
import subprocess
import tempfile
from run_checkpoint_retention_stack import block, limits

source = Path("src/app/ray_tracing_job_runner_status.c").read_text()
header = Path("include/app/ray_tracing_job_runner_internal.h").read_text()
production = block(source, r"bool ray_tracing_job_runner_merge_progress_into_record\s*\(")
record = block(header, r"typedef struct RayTracingDetachedJobRecord\s*\{")
prefix = r"""#include <json-c/json.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
static bool copy_string(char* out, size_t size, const char* value) { return snprintf(out,size,"%s",value)>=0; }
static bool ray_tracing_job_runner_file_exists(const char* path) { return access(path,F_OK)==0; }
static bool ray_tracing_job_runner_json_get_string(json_object* root,const char* key,const char** out) { json_object* v; if (!json_object_object_get_ex(root,key,&v)||!json_object_is_type(v,json_type_string)) return false; *out=json_object_get_string(v);return true; }
static bool ray_tracing_job_runner_json_get_int(json_object* root,const char* key,int* out) { json_object* v; if (!json_object_object_get_ex(root,key,&v)||!json_object_is_type(v,json_type_int)) return false; *out=json_object_get_int(v);return true; }
"""
driver = r"""
int main(int argc,char** argv) {
 if (argc!=2) return 1;
 RayTracingDetachedJobRecord cancelled={0}, before, live={0};
 strcpy(cancelled.state,"cancelled");strcpy(cancelled.stage,"cancelled");
 strcpy(cancelled.diagnostics,"cancel requested");cancelled.exit_code=143;
 strcpy(cancelled.finished_at_utc,"2026-10-09T00:11:47Z");before=cancelled;
 if (ray_tracing_job_runner_merge_progress_into_record(argv[1],&cancelled)||memcmp(&before,&cancelled,sizeof(before))) return 2;
 strcpy(live.state,"starting");
 if (!ray_tracing_job_runner_merge_progress_into_record(argv[1],&live)||strcmp(live.state,"running")||strcmp(live.stage,"applying_runtime_scene")||live.frame_index!=3) return 3;
 puts("cancelled terminal state preserved; live renderer progress still merges");return 0;
}
"""
root=Path("build/cancelled-progress-fixtures");root.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(dir=root) as tmp:
 tmp=Path(tmp);c=tmp/"test.c";exe=tmp/"test";progress=tmp/"progress.json"
 c.write_text(prefix+record+"\n"+production+driver)
 progress.write_text('{"state":"running","stage":"applying_runtime_scene","frame_index":3,"diagnostics":"stale renderer progress"}')
 flags=subprocess.check_output(["pkg-config","--cflags","--libs","json-c"],text=True).split()
 subprocess.run(["cc","-O0",str(c),"-o",str(exe),*flags],check=True)
 subprocess.run([str(exe.resolve()),str(progress.resolve())],check=True,timeout=10,preexec_fn=limits)

# =========================
#  Linux headless worker packaging
# =========================
LINUX_WORKER_HOST_ARCH := $(shell uname -m)
ifndef LINUX_WORKER_PLATFORM
ifeq ($(LINUX_WORKER_HOST_ARCH),x86_64)
LINUX_WORKER_PLATFORM := linux-x86_64
else ifeq ($(LINUX_WORKER_HOST_ARCH),amd64)
LINUX_WORKER_PLATFORM := linux-x86_64
else ifeq ($(LINUX_WORKER_HOST_ARCH),aarch64)
LINUX_WORKER_PLATFORM := linux-aarch64
else ifeq ($(LINUX_WORKER_HOST_ARCH),arm64)
LINUX_WORKER_PLATFORM := linux-aarch64
else
$(error Unsupported Linux worker package host architecture: $(LINUX_WORKER_HOST_ARCH))
endif
endif
LINUX_WORKER_SLUG := ray_tracing_headless_worker
ifeq ($(LINUX_WORKER_PLATFORM),linux-x86_64)
LINUX_WORKER_PLATFORM_CAPABILITY := platform-linux-x86_64-v1
else ifeq ($(LINUX_WORKER_PLATFORM),linux-aarch64)
LINUX_WORKER_PLATFORM_CAPABILITY := platform-linux-aarch64-v1
else
$(error Unsupported Linux worker package platform: $(LINUX_WORKER_PLATFORM))
endif
LINUX_WORKER_BASENAME := $(RELEASE_PROGRAM_KEY)-$(RELEASE_VERSION)-$(LINUX_WORKER_PLATFORM)-worker
LINUX_WORKER_DIR := $(RELEASE_DIR)/$(LINUX_WORKER_BASENAME)
LINUX_WORKER_BIN_DIR := $(LINUX_WORKER_DIR)/bin
LINUX_WORKER_CONFIG_DIR := $(LINUX_WORKER_DIR)/config
LINUX_WORKER_DOCS_DIR := $(LINUX_WORKER_DIR)/docs
LINUX_WORKER_MANIFEST_JSON := $(LINUX_WORKER_DIR)/manifest.json
LINUX_WORKER_MANIFEST := $(LINUX_WORKER_DIR)/package_manifest.json
LINUX_WORKER_ARCHIVE := $(RELEASE_DIR)/$(LINUX_WORKER_BASENAME).tar.gz
LINUX_WORKER_MAX_GLIBC ?= 2.39.0

package-linux-worker-contract:
	@echo "Linux worker package contract"
	@echo "  worker slug: $(LINUX_WORKER_SLUG)"
	@echo "  version:     $(RELEASE_VERSION)"
	@echo "  platform:    $(LINUX_WORKER_PLATFORM)"
	@echo "  max glibc:   $(LINUX_WORKER_MAX_GLIBC)"
	@echo "  stage dir:   $(LINUX_WORKER_DIR)"
	@echo "  archive:     $(LINUX_WORKER_ARCHIVE)"
	@echo "  binaries:"
	@echo "    - $(RAY_TRACING_RENDER_HEADLESS_BIN)"
	@echo "    - $(RAY_TRACING_JOB_RUNNER_BIN)"

package-linux-worker-clean:
	@rm -rf "$(LINUX_WORKER_DIR)" "$(LINUX_WORKER_ARCHIVE)"
	@$(MAKE) clean
	@echo "Removed Linux worker package artifacts: $(LINUX_WORKER_BASENAME)"

package-linux-worker: ray-tracing-render-headless ray-tracing-job-runner
	@echo "Preparing Linux worker package..."
	@rm -rf "$(LINUX_WORKER_DIR)"
	@mkdir -p "$(LINUX_WORKER_BIN_DIR)" "$(LINUX_WORKER_CONFIG_DIR)" "$(LINUX_WORKER_DOCS_DIR)"
	@cp "$(RAY_TRACING_RENDER_HEADLESS_BIN)" "$(LINUX_WORKER_BIN_DIR)/ray_tracing_render_headless"
	@cp "$(RAY_TRACING_JOB_RUNNER_BIN)" "$(LINUX_WORKER_BIN_DIR)/ray_tracing_job_runner"
	@printf '#!/usr/bin/env bash\n' > "$(LINUX_WORKER_BIN_DIR)/run_worker.sh"
	@printf 'set -euo pipefail\n' >> "$(LINUX_WORKER_BIN_DIR)/run_worker.sh"
	@printf 'SCRIPT_DIR="$$(cd "$$(dirname "$${BASH_SOURCE[0]}")" && pwd)"\n' >> "$(LINUX_WORKER_BIN_DIR)/run_worker.sh"
	@printf ': "$${CODEWORK_RAY_TRACING_DEFAULT_CPU_PERCENT:=50}"\n' >> "$(LINUX_WORKER_BIN_DIR)/run_worker.sh"
	@printf 'export CODEWORK_RAY_TRACING_DEFAULT_CPU_PERCENT\n' >> "$(LINUX_WORKER_BIN_DIR)/run_worker.sh"
	@printf 'exec "$$SCRIPT_DIR/ray_tracing_render_headless" "$$@"\n' >> "$(LINUX_WORKER_BIN_DIR)/run_worker.sh"
	@chmod +x "$(LINUX_WORKER_BIN_DIR)/run_worker.sh"
	@cp -R config/. "$(LINUX_WORKER_CONFIG_DIR)/"
	@cp README.md "$(LINUX_WORKER_DIR)/"
	@cp docs/README.md docs/headless_agent_render_cli.md "$(LINUX_WORKER_DOCS_DIR)/"
	@printf '{\n' > "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "schema_version": "codework-worker-package/v1",\n' >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "worker_slug": "%s",\n' "$(LINUX_WORKER_SLUG)" >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "version": "%s",\n' "$(RELEASE_VERSION)" >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "platform": "%s",\n' "$(LINUX_WORKER_PLATFORM)" >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "program": "%s",\n' "$(RELEASE_PROGRAM_KEY)" >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "max_glibc_version": "%s",\n' "$(LINUX_WORKER_MAX_GLIBC)" >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "job_types": ["trio_headless_stage"],\n' >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "capabilities": ["trio-headless-v1", "scene-project-portable-v1", "ray-tracing-project-render-v1", "%s"],\n' "$(LINUX_WORKER_PLATFORM_CAPABILITY)" >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "entrypoint": "bin/run_worker.sh",\n' >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "default_args": [],\n' >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "default_resource_budget": {\n' >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '    "cpu_percent": 50\n' >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  },\n' >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '  "runtime_dependencies": ["glibc", "libgcc_s", "libm", "SDL2", "SDL2_ttf", "json-c", "libpng16", "vulkan", "ffmpeg"]\n' >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '}\n' >> "$(LINUX_WORKER_MANIFEST_JSON)"
	@printf '{\n' > "$(LINUX_WORKER_MANIFEST)"
	@printf '  "schema_version": "codework_worker_package_manifest_v1",\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "package_role": "headless-worker",\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "worker_slug": "%s",\n' "$(LINUX_WORKER_SLUG)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "program": "%s",\n' "$(RELEASE_PROGRAM_KEY)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "version": "%s",\n' "$(RELEASE_VERSION)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "platform": "%s",\n' "$(LINUX_WORKER_PLATFORM)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "max_glibc_version": "%s",\n' "$(LINUX_WORKER_MAX_GLIBC)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "entrypoints": {\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '    "headless_cli": "bin/ray_tracing_render_headless",\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '    "job_runner": "bin/ray_tracing_job_runner"\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  },\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "capabilities": ["trio-headless-v1", "scene-project-portable-v1", "ray-tracing-project-render-v1", "%s"],\n' "$(LINUX_WORKER_PLATFORM_CAPABILITY)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "runtime_dependencies": ["glibc", "libgcc_s", "libm", "ffmpeg"],\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "default_resource_budget": {\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '    "cpu_percent": 50\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  },\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "self_test": {\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '    "type": "command",\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '    "argv": ["bin/ray_tracing_job_runner", "--help"]\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  }\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '}\n' >> "$(LINUX_WORKER_MANIFEST)"
	@mkdir -p "$(RELEASE_DIR)"
	@COPYFILE_DISABLE=1 tar -czf "$(LINUX_WORKER_ARCHIVE)" -C "$(RELEASE_DIR)" "$(LINUX_WORKER_BASENAME)"
	@echo "Linux worker package ready: $(LINUX_WORKER_ARCHIVE)"

package-linux-worker-self-test: package-linux-worker
	@test -x "$(LINUX_WORKER_BIN_DIR)/ray_tracing_render_headless" || (echo "Missing ray_tracing_render_headless"; exit 1)
	@test -x "$(LINUX_WORKER_BIN_DIR)/ray_tracing_job_runner" || (echo "Missing ray_tracing_job_runner"; exit 1)
	@test -x "$(LINUX_WORKER_BIN_DIR)/run_worker.sh" || (echo "Missing run_worker.sh"; exit 1)
	@test -f "$(LINUX_WORKER_MANIFEST_JSON)" || (echo "Missing manifest.json"; exit 1)
	@test -f "$(LINUX_WORKER_MANIFEST)" || (echo "Missing package_manifest.json"; exit 1)
	@test -f "$(LINUX_WORKER_DOCS_DIR)/headless_agent_render_cli.md" || (echo "Missing docs/headless_agent_render_cli.md"; exit 1)
	@test -f "$(LINUX_WORKER_CONFIG_DIR)/scene_config.json" || (echo "Missing config/scene_config.json"; exit 1)
	@test -f "$(LINUX_WORKER_ARCHIVE)" || (echo "Missing worker archive"; exit 1)
	@python3 tools/validate_linux_worker_package.py --archive "$(LINUX_WORKER_ARCHIVE)" --package-root "$(LINUX_WORKER_BASENAME)" --platform "$(LINUX_WORKER_PLATFORM)" --max-glibc "$(LINUX_WORKER_MAX_GLIBC)"
	@echo "package-linux-worker-self-test passed."

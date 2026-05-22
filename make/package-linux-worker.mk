# =========================
#  Linux headless worker packaging
# =========================
LINUX_WORKER_PLATFORM ?= linux-x86_64
LINUX_WORKER_SLUG := ray_tracing_headless_worker
LINUX_WORKER_BASENAME := $(RELEASE_PROGRAM_KEY)-$(RELEASE_VERSION)-$(LINUX_WORKER_PLATFORM)-worker
LINUX_WORKER_DIR := $(RELEASE_DIR)/$(LINUX_WORKER_BASENAME)
LINUX_WORKER_BIN_DIR := $(LINUX_WORKER_DIR)/bin
LINUX_WORKER_CONFIG_DIR := $(LINUX_WORKER_DIR)/config
LINUX_WORKER_DOCS_DIR := $(LINUX_WORKER_DIR)/docs
LINUX_WORKER_MANIFEST := $(LINUX_WORKER_DIR)/package_manifest.json
LINUX_WORKER_ARCHIVE := $(RELEASE_DIR)/$(LINUX_WORKER_BASENAME).tar.gz

package-linux-worker-contract:
	@echo "Linux worker package contract"
	@echo "  worker slug: $(LINUX_WORKER_SLUG)"
	@echo "  version:     $(RELEASE_VERSION)"
	@echo "  platform:    $(LINUX_WORKER_PLATFORM)"
	@echo "  stage dir:   $(LINUX_WORKER_DIR)"
	@echo "  archive:     $(LINUX_WORKER_ARCHIVE)"
	@echo "  binaries:"
	@echo "    - $(RAY_TRACING_RENDER_HEADLESS_BIN)"
	@echo "    - $(RAY_TRACING_JOB_RUNNER_BIN)"

package-linux-worker-clean:
	@rm -rf "$(LINUX_WORKER_DIR)" "$(LINUX_WORKER_ARCHIVE)"
	@echo "Removed Linux worker package artifacts: $(LINUX_WORKER_BASENAME)"

package-linux-worker: ray-tracing-render-headless ray-tracing-job-runner
	@echo "Preparing Linux worker package..."
	@rm -rf "$(LINUX_WORKER_DIR)"
	@mkdir -p "$(LINUX_WORKER_BIN_DIR)" "$(LINUX_WORKER_CONFIG_DIR)" "$(LINUX_WORKER_DOCS_DIR)"
	@cp "$(RAY_TRACING_RENDER_HEADLESS_BIN)" "$(LINUX_WORKER_BIN_DIR)/ray_tracing_render_headless"
	@cp "$(RAY_TRACING_JOB_RUNNER_BIN)" "$(LINUX_WORKER_BIN_DIR)/ray_tracing_job_runner"
	@cp -R config/. "$(LINUX_WORKER_CONFIG_DIR)/"
	@cp README.md "$(LINUX_WORKER_DIR)/"
	@cp docs/README.md docs/headless_agent_render_cli.md "$(LINUX_WORKER_DOCS_DIR)/"
	@printf '{\n' > "$(LINUX_WORKER_MANIFEST)"
	@printf '  "schema_version": "codework_worker_package_manifest_v1",\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "package_role": "headless-worker",\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "worker_slug": "%s",\n' "$(LINUX_WORKER_SLUG)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "program": "%s",\n' "$(RELEASE_PROGRAM_KEY)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "version": "%s",\n' "$(RELEASE_VERSION)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "platform": "%s",\n' "$(LINUX_WORKER_PLATFORM)" >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "entrypoints": {\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '    "headless_cli": "bin/ray_tracing_render_headless",\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '    "job_runner": "bin/ray_tracing_job_runner"\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  },\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "runtime_dependencies": ["glibc", "libgcc_s", "libm"],\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  "self_test": {\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '    "type": "command",\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '    "argv": ["bin/ray_tracing_job_runner", "--help"]\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '  }\n' >> "$(LINUX_WORKER_MANIFEST)"
	@printf '}\n' >> "$(LINUX_WORKER_MANIFEST)"
	@mkdir -p "$(RELEASE_DIR)"
	@tar -czf "$(LINUX_WORKER_ARCHIVE)" -C "$(RELEASE_DIR)" "$(LINUX_WORKER_BASENAME)"
	@echo "Linux worker package ready: $(LINUX_WORKER_ARCHIVE)"

package-linux-worker-self-test: package-linux-worker
	@test -x "$(LINUX_WORKER_BIN_DIR)/ray_tracing_render_headless" || (echo "Missing ray_tracing_render_headless"; exit 1)
	@test -x "$(LINUX_WORKER_BIN_DIR)/ray_tracing_job_runner" || (echo "Missing ray_tracing_job_runner"; exit 1)
	@test -f "$(LINUX_WORKER_MANIFEST)" || (echo "Missing package_manifest.json"; exit 1)
	@test -f "$(LINUX_WORKER_DOCS_DIR)/headless_agent_render_cli.md" || (echo "Missing docs/headless_agent_render_cli.md"; exit 1)
	@test -f "$(LINUX_WORKER_CONFIG_DIR)/scene_config.json" || (echo "Missing config/scene_config.json"; exit 1)
	@test -f "$(LINUX_WORKER_ARCHIVE)" || (echo "Missing worker archive"; exit 1)
	@echo "package-linux-worker-self-test passed."

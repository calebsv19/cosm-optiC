# =========================
#  fisiCs memory-check audit
# =========================

MEMORY_CHECK_FISICS_OVERLAYS := physics-units,memory-check
MEMORY_CHECK_REPORT_DIR := $(BUILD_DIR_BASE)/memory_check
MEMORY_CHECK_STDOUT := $(MEMORY_CHECK_REPORT_DIR)/ray_tracing.stdout
MEMORY_CHECK_STDERR := $(MEMORY_CHECK_REPORT_DIR)/ray_tracing.stderr
MEMORY_CHECK_TEST_BIN := $(MEMORY_CHECK_REPORT_DIR)/runtime_triangle_bvh_3d_memory_check_audit
MEMORY_CHECK_TEST_OBJ_DIR := $(MEMORY_CHECK_REPORT_DIR)/obj
MEMORY_CHECK_REPORT_POLICY ?= always
FISICS_MEMCHECK_RUNTIME ?= /Users/calebsv/Desktop/CodeWork/fisiCs/build/unsanitized/libfisics_memcheck_runtime.a
FISICS_MEMCHECK_LINK_LIBS ?=

MEMORY_CHECK_TEST_OBJS := \
	$(MEMORY_CHECK_TEST_OBJ_DIR)/test_runtime_triangle_bvh_3d.o \
	$(MEMORY_CHECK_TEST_OBJ_DIR)/runtime_ray_3d.o \
	$(MEMORY_CHECK_TEST_OBJ_DIR)/runtime_scene_3d.o \
	$(MEMORY_CHECK_TEST_OBJ_DIR)/runtime_triangle_bvh_3d.o \
	$(MEMORY_CHECK_TEST_OBJ_DIR)/runtime_volume_3d.o

MEMORY_CHECK_FISICS_CC := $(FISICS_ENV) $(FISICS_BIN) --overlay=$(MEMORY_CHECK_FISICS_OVERLAYS)

memory-check-build:
	@$(MAKE) BUILD_TOOLCHAIN=fisics FISICS_OVERLAY="$(MEMORY_CHECK_FISICS_OVERLAYS)" FISICS_MEMCHECK_LINK_LIBS="$(FISICS_MEMCHECK_RUNTIME)" -B all

$(MEMORY_CHECK_TEST_OBJ_DIR)/test_runtime_triangle_bvh_3d.o: $(TEST_DIR)/test_runtime_triangle_bvh_3d.c
	@mkdir -p "$(MEMORY_CHECK_TEST_OBJ_DIR)"
	$(MEMORY_CHECK_FISICS_CC) $(CFLAGS) -c "$<" -o "$@"

$(MEMORY_CHECK_TEST_OBJ_DIR)/runtime_ray_3d.o: $(SRC_DIR)/render/runtime_ray_3d.c
	@mkdir -p "$(MEMORY_CHECK_TEST_OBJ_DIR)"
	$(MEMORY_CHECK_FISICS_CC) $(CFLAGS) -c "$<" -o "$@"

$(MEMORY_CHECK_TEST_OBJ_DIR)/runtime_scene_3d.o: $(SRC_DIR)/render/runtime_scene_3d.c
	@mkdir -p "$(MEMORY_CHECK_TEST_OBJ_DIR)"
	$(MEMORY_CHECK_FISICS_CC) $(CFLAGS) -c "$<" -o "$@"

$(MEMORY_CHECK_TEST_OBJ_DIR)/runtime_triangle_bvh_3d.o: $(SRC_DIR)/render/runtime_triangle_bvh_3d.c
	@mkdir -p "$(MEMORY_CHECK_TEST_OBJ_DIR)"
	$(MEMORY_CHECK_FISICS_CC) $(CFLAGS) -c "$<" -o "$@"

$(MEMORY_CHECK_TEST_OBJ_DIR)/runtime_volume_3d.o: $(SRC_DIR)/render/runtime_volume_3d.c
	@mkdir -p "$(MEMORY_CHECK_TEST_OBJ_DIR)"
	$(MEMORY_CHECK_FISICS_CC) $(CFLAGS) -c "$<" -o "$@"

$(MEMORY_CHECK_TEST_BIN): $(MEMORY_CHECK_TEST_OBJS)
	@mkdir -p "$(MEMORY_CHECK_REPORT_DIR)"
	$(CLANG_CC) $(LDFLAGS) -o "$@" $(MEMORY_CHECK_TEST_OBJS) $(FISICS_MEMCHECK_RUNTIME) -lm

memory-check-run: memory-check-build
	@$(MAKE) "$(MEMORY_CHECK_TEST_BIN)"
	@mkdir -p "$(MEMORY_CHECK_REPORT_DIR)"
	set +e; FISICS_MEMCHECK_REPORT="$(MEMORY_CHECK_REPORT_POLICY)" "$(MEMORY_CHECK_TEST_BIN)" > "$(MEMORY_CHECK_STDOUT)" 2> "$(MEMORY_CHECK_STDERR)"; status=$$?; \
	echo "memory-check stdout: $(MEMORY_CHECK_STDOUT)"; \
	echo "memory-check stderr: $(MEMORY_CHECK_STDERR)"; \
	exit $$status

memory-check-audit: memory-check-build
	@$(MAKE) "$(MEMORY_CHECK_TEST_BIN)"
	@mkdir -p "$(MEMORY_CHECK_REPORT_DIR)"
	set +e; FISICS_MEMCHECK_REPORT="$(MEMORY_CHECK_REPORT_POLICY)" "$(MEMORY_CHECK_TEST_BIN)" > "$(MEMORY_CHECK_STDOUT)" 2> "$(MEMORY_CHECK_STDERR)"; status=$$?; \
	echo "memory-check stdout: $(MEMORY_CHECK_STDOUT)"; \
	echo "memory-check stderr: $(MEMORY_CHECK_STDERR)"; \
	echo "memory-check summary:"; \
	grep -E "\\[fisics:memory-check\\] (summary|leak|double free|unknown pointer free)" "$(MEMORY_CHECK_STDERR)" || true; \
	exit $$status

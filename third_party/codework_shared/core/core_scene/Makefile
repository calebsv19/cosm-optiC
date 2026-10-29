CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

CORE_BASE_DIR ?= ../core_base
CORE_OBJECT_DIR ?= ../core_object
CORE_UNITS_DIR ?= ../core_units
INC = -Iinclude -I$(CORE_BASE_DIR)/include -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include
OBJ_DIR = build

LIB = $(OBJ_DIR)/libcore_scene.a
LIB_OBJ = $(OBJ_DIR)/core_scene.o $(OBJ_DIR)/core_scene_rigid.o
TEST_BIN = $(OBJ_DIR)/core_scene_test

.PHONY: all clean test

all: $(LIB)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CORE_BASE_DIR)/build/libcore_base.a:
	$(MAKE) -C $(CORE_BASE_DIR)

$(CORE_OBJECT_DIR)/build/libcore_object.a:
	$(MAKE) -C $(CORE_OBJECT_DIR)

$(CORE_UNITS_DIR)/build/libcore_units.a:
	$(MAKE) -C $(CORE_UNITS_DIR)

$(OBJ_DIR)/%.o: src/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

$(TEST_BIN): tests/core_scene_test.c $(LIB) $(CORE_BASE_DIR)/build/libcore_base.a $(CORE_OBJECT_DIR)/build/libcore_object.a $(CORE_UNITS_DIR)/build/libcore_units.a | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) $< $(LIB) $(CORE_OBJECT_DIR)/build/libcore_object.a $(CORE_UNITS_DIR)/build/libcore_units.a $(CORE_BASE_DIR)/build/libcore_base.a -lm -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(OBJ_DIR)

# Optional strict JSON admission adapter; no JSON dependency in the base library.
CJSON_DIR ?= ../../shape/external/cjson
RIGID_SRC = src/core_scene_rigid.c src/core_scene_rigid_json.c $(CJSON_DIR)/cJSON.c
.PHONY: rigid-scene-tool rigid-contract-test
rigid-scene-tool: $(OBJ_DIR)/rigid_scene_inspect
$(OBJ_DIR)/rigid_scene_inspect: tools/rigid_scene_inspect.c $(RIGID_SRC) include/core_scene_rigid.h include/core_scene_rigid_json.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -I$(CJSON_DIR) $< $(RIGID_SRC) -lm -o $@
rigid-contract-test: $(OBJ_DIR)/core_scene_rigid_test
	$<
$(OBJ_DIR)/core_scene_rigid_test: tests/core_scene_rigid_test.c $(RIGID_SRC) include/core_scene_rigid.h include/core_scene_rigid_json.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -I$(CJSON_DIR) $< $(RIGID_SRC) -lm -o $@
.PHONY: rigid-admission-test
rigid-admission-test: rigid-scene-tool
	@test -n "$(PROOF_ROOT)" || (echo 'Set PROOF_ROOT to a fresh task-owned directory' >&2; exit 2)
	PYTHONDONTWRITEBYTECODE=1 python3 tests/test_rigid_scene_admission.py --tool $(OBJ_DIR)/rigid_scene_inspect --out "$(PROOF_ROOT)"

.PHONY: rigid-result-profile-test
rigid-result-profile-test: $(OBJ_DIR)/core_scene_rigid_result_profile_test
	$<
$(OBJ_DIR)/core_scene_rigid_result_profile_test: tests/core_scene_rigid_result_profile_test.c $(RIGID_SRC) src/core_scene_rigid_result.c include/core_scene_rigid_result.h | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INC) -I$(CJSON_DIR) $< $(RIGID_SRC) src/core_scene_rigid_result.c -lm -o $@

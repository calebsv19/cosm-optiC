.PHONY: test-atmosphere-optics test-atmosphere-optics-sanitize
ATMOSPHERE_OPTICS_TEST_SRCS := tests/atmosphere_optics_contract_test.c src/render/runtime_atmosphere_optics_3d.c
$(BUILD_DIR)/tests/atmosphere_optics_contract_test: $(ATMOSPHERE_OPTICS_TEST_SRCS) include/render/runtime_atmosphere_optics_3d.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(ATMOSPHERE_OPTICS_TEST_SRCS) -o $@ -lm
test-atmosphere-optics: $(BUILD_DIR)/tests/atmosphere_optics_contract_test
	$<
test-atmosphere-optics-sanitize:
	@mkdir -p $(BUILD_DIR)/tests
	$(CC) $(CFLAGS) -Iinclude -fsanitize=address,undefined $(ATMOSPHERE_OPTICS_TEST_SRCS) -o $(BUILD_DIR)/tests/atmosphere_optics_contract_sanitize -lm
	$(BUILD_DIR)/tests/atmosphere_optics_contract_sanitize

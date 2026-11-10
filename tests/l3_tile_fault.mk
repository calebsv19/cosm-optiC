.PHONY: l3-tile-fault
L3_TEST := $(BUILD_DIR)/tests/l3_tile_fault_test
l3-tile-fault: $(L3_TEST)
$(L3_TEST): tests/l3_tile_fault_test.c $(TEST_DEPS)
	$(CC) $(CFLAGS) -Itests -g $(L3_SAN) -c tests/l3_tile_fault_test.c -o $(L3_TEST).o
	$(CC) -Wl,-dead_strip $(L3_SAN) $(L3_TEST).o $(filter-out $(BUILD_DIR)/render/runtime_native_3d_tile_scheduler.o,$(TEST_DEPS)) -o $(L3_TEST) $(LDFLAGS)

.PHONY: l3-receipt-fault
L3_RECEIPT_TEST := $(BUILD_DIR)/tests/l3_render_receipt_fault_test
l3-receipt-fault: $(L3_RECEIPT_TEST)
$(L3_RECEIPT_TEST): tests/l3_render_receipt_fault_test.c tests/l3_json_faults.h src/editor/scene_editor_render_control.c $(TEST_DEPS)
	$(CC) $(CFLAGS) -Itests -g $(L3_SAN) -c tests/l3_render_receipt_fault_test.c -o $(L3_RECEIPT_TEST).o
	$(CC) -Wl,-dead_strip $(L3_SAN) $(L3_RECEIPT_TEST).o $(filter-out $(BUILD_DIR)/editor/scene_editor_render_control.o,$(TEST_DEPS)) -o $(L3_RECEIPT_TEST) $(LDFLAGS)

# OWNER: Rimjhim - replace this stub with the real implementation. Do not edit if you are not the owner.

CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pthread -Iinclude -Isrc
BUILD_DIR = build

BINARIES = $(BUILD_DIR)/satellite_sync \
           $(BUILD_DIR)/router_app \
           $(BUILD_DIR)/memory_management \
           $(BUILD_DIR)/page_replacement \
           $(BUILD_DIR)/bankers \
           $(BUILD_DIR)/csv_check

.PHONY: all clean test tsan

all: $(BINARIES)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/satellite_sync: $(BUILD_DIR)/satellite_sync.o $(BUILD_DIR)/packet_queue.o $(BUILD_DIR)/csv_loader.o $(BUILD_DIR)/common.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/router_app: $(BUILD_DIR)/router_app.o $(BUILD_DIR)/packet_queue.o $(BUILD_DIR)/csv_loader.o $(BUILD_DIR)/common.o $(BUILD_DIR)/resource_manager.o $(BUILD_DIR)/mem_trace.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/memory_management: $(BUILD_DIR)/memory_management.o $(BUILD_DIR)/mem_trace.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/page_replacement: $(BUILD_DIR)/page_replacement.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/bankers: $(BUILD_DIR)/bankers.o $(BUILD_DIR)/resource_manager.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/csv_check: $(BUILD_DIR)/csv_check.o $(BUILD_DIR)/csv_loader.o $(BUILD_DIR)/common.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/test_mem_trace: tests/test_mem_trace.cpp $(BUILD_DIR)/mem_trace.o | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

tsan: | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -fsanitize=thread -g src/satellite_sync.cpp src/packet_queue.cpp src/csv_loader.cpp src/common.cpp -o $(BUILD_DIR)/satellite_sync_tsan

test: all $(BUILD_DIR)/test_mem_trace
	@mkdir -p outputs
	@echo "Running tests / self-tests..."
	@echo "Testing $(BUILD_DIR)/test_mem_trace"; \
	$(BUILD_DIR)/test_mem_trace; \
	code=$$?; \
	if [ $$code -eq 0 ]; then \
		echo "[PASS] $(BUILD_DIR)/test_mem_trace"; \
	else \
		echo "[FAIL] $(BUILD_DIR)/test_mem_trace exited with code $$code"; exit $$code; \
	fi
	@for bin in $(BUILD_DIR)/memory_management $(BUILD_DIR)/page_replacement $(BUILD_DIR)/bankers; do \
		echo "Testing $$bin --selftest"; \
		$$bin --selftest; \
		code=$$?; \
		if [ $$code -eq 0 ]; then \
			echo "[PASS] $$bin"; \
		elif [ $$code -eq 77 ]; then \
			echo "[SKIPPED] $$bin (stub returned 77)"; \
		else \
			echo "[FAIL] $$bin exited with code $$code"; exit $$code; \
		fi; \
	done
	@echo "All tests finished."

clean:
	rm -rf $(BUILD_DIR)

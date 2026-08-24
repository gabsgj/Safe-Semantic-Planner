CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O3 -Iinclude -Ithird_party -I. -pthread

BIN_DIR := bin
BUILD_DIR := build

.PHONY: all clean test_phase1 test_phase2 test_assignment_tc test_phase5 test_phase6 test_phase7 test_bonus benchmark inspect_phase1 inspect_phase2 inspect_phase5 nlp_main agent_main bonus_main server

all: $(BIN_DIR)/test_phase1 $(BIN_DIR)/test_phase2 $(BIN_DIR)/test_assignment_tc $(BIN_DIR)/test_phase5 $(BIN_DIR)/test_phase6 $(BIN_DIR)/test_phase7 $(BIN_DIR)/test_bonus $(BIN_DIR)/inspect_phase1 $(BIN_DIR)/inspect_phase2 $(BIN_DIR)/inspect_phase5 $(BIN_DIR)/nlp_main $(BIN_DIR)/agent_main $(BIN_DIR)/bonus_main $(BIN_DIR)/benchmark_main $(BIN_DIR)/ssp_server

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR)/test_phase1: tests/test_phase1.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN_DIR)/test_phase2: tests/test_phase2.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN_DIR)/test_assignment_tc: tests/test_assignment_tc.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN_DIR)/test_phase5: tests/test_phase5.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN_DIR)/test_phase6: tests/test_phase6.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN_DIR)/test_phase7: tests/test_phase7.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN_DIR)/test_bonus: tests/test_bonus.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN_DIR)/inspect_phase1: src/inspect_phase1.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN_DIR)/inspect_phase2: src/inspect_phase2.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN_DIR)/inspect_phase5: src/inspect_phase5.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN_DIR)/nlp_main: src/nlp_main.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN_DIR)/agent_main: src/agent_main.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN_DIR)/bonus_main: src/bonus_main.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN_DIR)/benchmark_main: src/benchmark_main.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN_DIR)/ssp_server: src/server_main.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

test_phase1: $(BIN_DIR)/test_phase1
	./$(BIN_DIR)/test_phase1

test_phase2: $(BIN_DIR)/test_phase2
	./$(BIN_DIR)/test_phase2

test_assignment_tc: $(BIN_DIR)/test_assignment_tc
	./$(BIN_DIR)/test_assignment_tc

test_phase5: $(BIN_DIR)/test_phase5
	./$(BIN_DIR)/test_phase5

test_phase6: $(BIN_DIR)/test_phase6
	./$(BIN_DIR)/test_phase6

test_phase7: $(BIN_DIR)/test_phase7
	./$(BIN_DIR)/test_phase7

test_bonus: $(BIN_DIR)/test_bonus
	./$(BIN_DIR)/test_bonus

benchmark: $(BIN_DIR)/benchmark_main
	./$(BIN_DIR)/benchmark_main

inspect_phase1: $(BIN_DIR)/inspect_phase1
	./$(BIN_DIR)/inspect_phase1

inspect_phase2: $(BIN_DIR)/inspect_phase2
	./$(BIN_DIR)/inspect_phase2

inspect_phase5: $(BIN_DIR)/inspect_phase5
	./$(BIN_DIR)/inspect_phase5

nlp: $(BIN_DIR)/nlp_main
	./$(BIN_DIR)/nlp_main

agent: $(BIN_DIR)/agent_main
	./$(BIN_DIR)/agent_main

bonus: $(BIN_DIR)/bonus_main
	./$(BIN_DIR)/bonus_main

server: $(BIN_DIR)/ssp_server
	./$(BIN_DIR)/ssp_server

clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR)

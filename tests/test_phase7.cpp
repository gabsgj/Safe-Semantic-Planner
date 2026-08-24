#include <iostream>
#include <cassert>
#include <cmath>

#include "ssp/agent/agent_state.hpp"
#include "ssp/agent/agent_controller.hpp"
#include "ssp/agent/swe_benchmarker.hpp"

using namespace ssp;

void testAgentStateAndActionInitialization() {
    std::cout << ">>> [TEST 1] AI Agent State Snapshots & 5D Health Vector\n";
    agent::AgentState state;
    state.id = 0;
    state.commitHash = "abc12345678";
    state.stageDescription = "Initial Workspace";
    state.healthVector = {0.80, 0.0, 0.75, 0.10, 0.0};
    state.passingTests = 8;
    state.totalTests = 10;

    assert(state.healthVector.size() == 5);
    assert(state.passingTests == 8);
    assert(state.commitHash.substr(0, 7) == "abc1234");
    std::cout << "  [PASSED] Agent State Snapshots Verified.\n\n";
}

void testRegressionQuarantineAndBacktracking() {
    std::cout << ">>> [TEST 2] Dynamic Regression Quarantine & D* Lite Backtracking\n";
    std::vector<agent::AgentState> states = {
        {0, "init000", "Start Buggy Repo", {0.90, 0.0, 0.50, 0.0, 0.0}, 9, 10, 0, 0, 100.0, false, false},
        {1, "feat111", "Target Patch A (Regressive)", {0.70, 0.0, 0.60, 0.0, 2.0}, 7, 10, 0, 2, 500.0, false, false},
        {2, "feat222", "Clean Patch B", {1.00, 0.0, 1.00, 0.0, 0.0}, 10, 10, 0, 0, 600.0, false, true}
    };

    std::vector<std::pair<core::Transition, agent::AgentAction>> transitions = {
        {{101, 0, 1, 3.0, 1.0, 0.70, true, "Patch A (Cheap)"}, {101, agent::ActionType::APPLY_PATCH, "Patch A (Regressive)", 500.0, 50.0, 0.70, true}},
        {{102, 1, 2, 2.0, 1.0, 0.70, true, "Patch A to Goal"}, {102, agent::ActionType::APPLY_PATCH, "Patch A Commit", 500.0, 50.0, 0.70, true}},
        {{103, 0, 2, 8.0, 1.0, 0.99, true, "Patch B (Safe)"}, {103, agent::ActionType::APPLY_PATCH, "Patch B (Clean)", 600.0, 60.0, 0.99, false}}
    };

    agent::NeuroSymbolicGovernor governor;
    governor.initialize(states, transitions, 0, 2);

    auto decisions = governor.runToGoal();

    assert(governor.getCurrentStateId() == 2);
    assert(governor.getBacktrackCount() >= 1);
    std::cout << "  - Backtracks Triggered: " << governor.getBacktrackCount() << "\n";
    std::cout << "  - Total Tokens Spent:   " << governor.getTotalTokensSpent() << "\n";
    std::cout << "  [PASSED] Dynamic Regression Quarantine & Backtracking Verified.\n\n";
}

void testSweBenchBenchmarkRun() {
    std::cout << ">>> [TEST 3] Full SWE-bench Benchmark Comparison\n";
    auto report = agent::SweBenchmarker::runDjangoOrmBenchmark();

    assert(!report.naiveAgentSuccess);
    assert(report.governedAgentSuccess);
    assert(report.governedRegressionsHit == 0);
    assert(report.tokenSavingsPercent > 30.0);
    assert(report.averageGovernorLatencyUs < 50.0);

    std::cout << "  - Task:                  " << report.taskName << "\n";
    std::cout << "  - Naive Status:          FAILED (Trapped in regression loop)\n";
    std::cout << "  - Governed Status:       100% RESOLVED\n";
    std::cout << "  - Token Cost Savings:    " << report.tokenSavingsPercent << "%\n";
    std::cout << "  - Governor Decision Lat: " << report.averageGovernorLatencyUs << " µs\n";
    std::cout << "  [PASSED] SWE-bench Benchmark Verification Verified.\n\n";
}

int main() {
    std::cout << "==========================================================\n";
    std::cout << "      SAFE SEMANTIC PLANNER - PHASE 7 TEST SUITE          \n";
    std::cout << "==========================================================\n\n";

    testAgentStateAndActionInitialization();
    testRegressionQuarantineAndBacktracking();
    testSweBenchBenchmarkRun();

    std::cout << "==========================================================\n";
    std::cout << "  ALL PHASE 7 AI AGENT GOVERNOR TESTS PASSED (100%)       \n";
    std::cout << "==========================================================\n";
    return 0;
}

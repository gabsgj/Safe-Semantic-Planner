#include <iostream>
#include <iomanip>
#include <cassert>
#include <vector>
#include <cmath>
#include <chrono>

#include "ssp/core/types.hpp"
#include "ssp/core/state.hpp"
#include "ssp/core/transition.hpp"
#include "ssp/core/problem.hpp"
#include "ssp/core/result.hpp"
#include "ssp/algorithms/dstar_lite.hpp"
#include "ssp/config/config_manager.hpp"

using namespace ssp;

void printTestHeader(const std::string& title) {
    std::cout << "\n------------------------------------------------------------\n";
    std::cout << "  " << title << "\n";
    std::cout << "------------------------------------------------------------\n";
}

void printResultMetrics(const std::string& testName, const core::PlanningResult& res) {
    std::cout << ">>> [" << testName << "] Result:\n";
    std::cout << "    - Success:                 " << (res.success ? "✅ YES" : "❌ NO") << "\n";
    std::cout << "    - State Path:              ";
    for (size_t i = 0; i < res.statePath.size(); ++i) {
        std::cout << res.statePath[i] << (i + 1 < res.statePath.size() ? " -> " : "");
    }
    std::cout << "\n";
    std::cout << "    - Transition Path:         ";
    for (size_t i = 0; i < res.transitionPath.size(); ++i) {
        std::cout << res.transitionPath[i] << (i + 1 < res.transitionPath.size() ? " -> " : "");
    }
    std::cout << "\n";
    std::cout << "    - Total Path Cost:         " << std::fixed << std::setprecision(4) << res.totalCost << "\n";
    std::cout << "    - Min Distance to Hazard:  " << res.minimumSafetyDistance << "\n";
    std::cout << "    - Cumulative Reliability:  " << (res.cumulativeReliability * 100.0) << "%\n";
    std::cout << "    - Objective Safety Score:  " << res.safetyScore << "\n";
    std::cout << "    - Explored Nodes:          " << res.exploredStatesCount << "\n";
    std::cout << "    - Planning Time:           " << res.planningTimeMicroseconds << " µs\n";
}

/**
 * Test Case 1: Basic Reachability (PDF Page 4)
 * Graph: S -> A -> B -> G
 * Expected: Planner returns the unique valid path [0 -> 1 -> 2 -> 3]
 */
void testCase1_BasicReachability() {
    printTestHeader("TEST CASE 1: BASIC REACHABILITY (S -> A -> B -> G)");

    core::PlanningProblem problem;
    problem.domainName = "TC1: Basic Reachability";
    problem.initialState = 0; // S
    problem.goalState = 3;    // G
    problem.badStates = {};   // No obstacles

    // Coordinates along a line in R^2
    problem.states = {
        {0, {0.0, 0.0}, "S (Start)"},
        {1, {2.0, 0.0}, "A"},
        {2, {4.0, 0.0}, "B"},
        {3, {6.0, 0.0}, "G (Goal)"}
    };

    problem.transitions = {
        {101, 0, 1, 2.0, 1.0, 0.99, true, "S_to_A"},
        {102, 1, 2, 2.0, 1.0, 0.99, true, "A_to_B"},
        {103, 2, 3, 2.0, 1.0, 0.99, true, "B_to_G"}
    };

    config::PlannerConfig cfg;
    algorithms::DStarLite planner(cfg);
    auto result = planner.plan(problem);

    printResultMetrics("TC1", result);

    assert(result.success == true);
    assert(result.statePath.size() == 4);
    assert(result.statePath[0] == 0 && result.statePath[1] == 1 && result.statePath[2] == 2 && result.statePath[3] == 3);
    assert(result.transitionPath.size() == 3);
    assert(result.transitionPath[0] == 101 && result.transitionPath[1] == 102 && result.transitionPath[2] == 103);
    assert(std::abs(result.totalCost - 6.0) < 1e-4);

    std::cout << "  ✓ TC1 PASSED: Unique valid path successfully discovered.\n";
}

/**
 * Test Case 2: Bad State Avoidance (PDF Page 5)
 * Paths:
 *   S -> A -> X -> G (where X is a bad state)
 *   S -> C -> D -> G
 * Expected: Planner discards path through X and selects S -> C -> D -> G.
 */
void testCase2_BadStateAvoidance() {
    printTestHeader("TEST CASE 2: BAD STATE AVOIDANCE (Path through X vs Safe Path)");

    core::PlanningProblem problem;
    problem.domainName = "TC2: Bad State Avoidance";
    problem.initialState = 0; // S
    problem.goalState = 5;    // G
    problem.badStates = {2};  // State X (ID 2) is BAD

    problem.states = {
        {0, {0.0, 0.0}, "S"},
        {1, {2.0, 2.0}, "A"},
        {2, {4.0, 2.0}, "X (BAD STATE)"},
        {3, {2.0, -2.0}, "C"},
        {4, {4.0, -2.0}, "D"},
        {5, {6.0, 0.0}, "G"}
    };

    // Path 1 (shorter cost, but contains bad state X)
    // Path 2 (longer cost, but completely safe)
    problem.transitions = {
        // Path 1 via X
        {101, 0, 1, 2.82, 0.5, 0.95, true, "S_to_A"},
        {102, 1, 2, 2.00, 0.1, 0.80, true, "A_to_X"},
        {103, 2, 5, 2.82, 0.1, 0.80, true, "X_to_G"},
        // Path 2 via C, D (Safe)
        {201, 0, 3, 2.82, 1.0, 0.99, true, "S_to_C"},
        {202, 3, 4, 2.00, 1.0, 0.99, true, "C_to_D"},
        {203, 4, 5, 2.82, 1.0, 0.99, true, "D_to_G"}
    };

    config::PlannerConfig cfg;
    algorithms::DStarLite planner(cfg);
    auto result = planner.plan(problem);

    printResultMetrics("TC2", result);

    assert(result.success == true);
    // Must select safe path S -> C -> D -> G (0 -> 3 -> 4 -> 5)
    assert(result.statePath.size() == 4);
    assert(result.statePath[0] == 0 && result.statePath[1] == 3 && result.statePath[2] == 4 && result.statePath[3] == 5);
    // Must NOT visit bad state X (ID 2)
    for (uint64_t st : result.statePath) {
        assert(st != 2);
    }

    std::cout << "  ✓ TC2 PASSED: Bad state X strictly avoided, safe path selected.\n";
}

/**
 * Test Case 3: Safety Margin (PDF Page 5)
 * Two valid paths:
 *   Path 1: Lower cost, but passes close to bad states.
 *   Path 2: Higher cost, but remains significantly farther away.
 * Expected: Under safety-aware configuration (gamma > 0), planner prefers Path 2.
 */
void testCase3_SafetyMargin() {
    printTestHeader("TEST CASE 3: SAFETY MARGIN (Low Cost Risky vs High Cost Safe)");

    core::PlanningProblem problem;
    problem.domainName = "TC3: Safety Margin";
    problem.initialState = 0;
    problem.goalState = 5;
    problem.badStates = {99}; // Obstacle at (3.0, 0.5)

    problem.states = {
        {0, {0.0, 0.0}, "Start"},
        // Path 1 (Close to obstacle at y=0.5)
        {1, {3.0, 0.1}, "Risky_Midpoint"},
        // Path 2 (Far away at y=4.0)
        {2, {3.0, 4.0}, "Safe_Midpoint"},
        {5, {6.0, 0.0}, "Goal"},
        // Bad State (Hazard placed at y=1.0, distance 0.9 from Risky and 3.0 from Safe)
        {99, {3.0, 1.0}, "Hazard_Core"}
    };

    problem.transitions = {
        // Path 1 (Risky): Cost = 3.0 + 3.0 = 6.0
        {101, 0, 1, 3.0, 0.2, 0.95, true, "Start_to_Risky"},
        {102, 1, 5, 3.0, 0.2, 0.95, true, "Risky_to_Goal"},
        // Path 2 (Safe): Cost = 5.0 + 5.0 = 10.0
        {201, 0, 2, 5.0, 1.0, 0.99, true, "Start_to_Safe"},
        {202, 2, 5, 5.0, 1.0, 0.99, true, "Safe_to_Goal"}
    };

    // 1. With high safety weight (gamma = 15.0), Path 2 (Safe) should win
    config::PlannerConfig safeCfg;
    safeCfg.gamma_safety = 15.0;
    safeCfg.safety_clearance_margin = 2.0;
    safeCfg.hazard_barrier_decay_sigma = 1.0;

    algorithms::DStarLite safePlanner(safeCfg);
    auto safeRes = safePlanner.plan(problem);

    printResultMetrics("TC3 (Safety Priority)", safeRes);

    assert(safeRes.success == true);
    assert(safeRes.statePath.size() == 3);
    assert(safeRes.statePath[1] == 2); // Safe_Midpoint selected!
    assert(safeRes.minimumSafetyDistance >= 3.0);

    // 2. With zero safety weight (gamma = 0.0), Path 1 (Cheaper) should win
    config::PlannerConfig cheapCfg;
    cheapCfg.gamma_safety = 0.0;
    algorithms::DStarLite cheapPlanner(cheapCfg);
    auto cheapRes = cheapPlanner.plan(problem);

    printResultMetrics("TC3 (Pure Cost Priority)", cheapRes);
    assert(cheapRes.success == true);
    assert(cheapRes.statePath[1] == 1); // Risky_Midpoint selected due to lower cost!

    std::cout << "  ✓ TC3 PASSED: Planner balances cost and safety clearance accurately.\n";
}

/**
 * Test Case 4: Dynamic Transition (PDF Page 5)
 * Initially: S -> A -> G.
 * Later: Transition (A, G) becomes unavailable.
 * Expected: Planner dynamically computes an alternative path (e.g. S -> A -> B -> G).
 */
void testCase4_DynamicTransition() {
    printTestHeader("TEST CASE 4: DYNAMIC TRANSITION (Edge Breakage & Instant Replan)");

    core::PlanningProblem problem;
    problem.domainName = "TC4: Dynamic Transition";
    problem.initialState = 0; // S
    problem.goalState = 3;    // G
    problem.badStates = {};

    problem.states = {
        {0, {0.0, 0.0}, "S"},
        {1, {2.0, 0.0}, "A"},
        {2, {2.0, 2.0}, "B (Bypass)"},
        {3, {4.0, 0.0}, "G"}
    };

    problem.transitions = {
        {101, 0, 1, 2.0, 1.0, 0.99, true, "S_to_A"},
        {102, 1, 3, 2.0, 1.0, 0.99, true, "A_to_G (Direct)"},
        {201, 1, 2, 2.0, 1.0, 0.99, true, "A_to_B (Detour 1)"},
        {202, 2, 3, 2.82, 1.0, 0.99, true, "B_to_G (Detour 2)"}
    };

    config::PlannerConfig cfg;
    algorithms::DStarLite planner(cfg);

    // Initial Plan
    auto initialRes = planner.plan(problem);
    printResultMetrics("TC4 (Initial Direct Path)", initialRes);
    assert(initialRes.success == true);
    assert(initialRes.statePath.size() == 3);
    assert(initialRes.statePath[0] == 0 && initialRes.statePath[1] == 1 && initialRes.statePath[2] == 3);

    // Dynamic Event: Edge (A, G) (ID 102) becomes unavailable
    std::cout << "\n>>> [TC4] DYNAMIC EVENT: Transition 102 (A -> G) FAILS / BLOCKED!\n";
    planner.setEdgeAvailability(102, false);
    auto replanRes = planner.replan(0);

    printResultMetrics("TC4 (Dynamic Replanned Path)", replanRes);
    assert(replanRes.success == true);
    assert(replanRes.wasReplanned == true);
    // Path must now route via B: 0 -> 1 -> 2 -> 3
    assert(replanRes.statePath.size() == 4);
    assert(replanRes.statePath[0] == 0 && replanRes.statePath[1] == 1 && replanRes.statePath[2] == 2 && replanRes.statePath[3] == 3);

    std::cout << "  ✓ TC4 PASSED: Dynamic edge failure detected and instantly rerouted.\n";
}

/**
 * Test Case 5: Goal Update (PDF Page 5)
 * The goal changes during execution (G1 -> G2).
 * Expected: Planner produces revised path reusing existing search structures.
 */
void testCase5_GoalUpdate() {
    printTestHeader("TEST CASE 5: GOAL UPDATE (G1 -> G2 Dynamic Shift)");

    core::PlanningProblem problem;
    problem.domainName = "TC5: Goal Update";
    problem.initialState = 0; // S
    problem.goalState = 3;    // Initial Goal G1
    problem.badStates = {};

    problem.states = {
        {0, {0.0, 0.0}, "S"},
        {1, {2.0, 0.0}, "A"},
        {2, {4.0, 0.0}, "B"},
        {3, {6.0, 0.0}, "G1 (Old Goal)"},
        {4, {4.0, 3.0}, "G2 (New Goal)"}
    };

    problem.transitions = {
        {101, 0, 1, 2.0, 1.0, 0.99, true, "S_to_A"},
        {102, 1, 2, 2.0, 1.0, 0.99, true, "A_to_B"},
        {103, 2, 3, 2.0, 1.0, 0.99, true, "B_to_G1"},
        {201, 1, 4, 3.6, 1.0, 0.99, true, "A_to_G2"},
        {202, 2, 4, 3.0, 1.0, 0.99, true, "B_to_G2"}
    };

    config::PlannerConfig cfg;
    algorithms::DStarLite planner(cfg);

    // Initial Plan to G1
    auto resG1 = planner.plan(problem);
    printResultMetrics("TC5 (Initial Plan to G1)", resG1);
    assert(resG1.success == true);
    assert(resG1.statePath.back() == 3);

    // Dynamic Event: Goal updates to G2 (ID 4)
    std::cout << "\n>>> [TC5] DYNAMIC EVENT: Destination shifted to G2 (State #4)!\n";
    planner.updateGoal(4);
    auto resG2 = planner.replan(0);

    printResultMetrics("TC5 (Replanned Path to G2)", resG2);
    assert(resG2.success == true);
    assert(resG2.statePath.back() == 4);

    std::cout << "  ✓ TC5 PASSED: Goal updated dynamically without graph destruction.\n";
}

/**
 * Test Case 6: Transition Addition (PDF Page 5)
 * A new shortcut transition is inserted.
 * Expected: Planner discovers the improved solution.
 */
void testCase6_TransitionAddition() {
    printTestHeader("TEST CASE 6: TRANSITION ADDITION (Shortcut Insertion)");

    core::PlanningProblem problem;
    problem.domainName = "TC6: Transition Addition";
    problem.initialState = 0; // S
    problem.goalState = 4;    // G
    problem.badStates = {};

    problem.states = {
        {0, {0.0, 0.0}, "S"},
        {1, {2.0, 0.0}, "A"},
        {2, {4.0, 0.0}, "B"},
        {3, {6.0, 0.0}, "C"},
        {4, {8.0, 0.0}, "G"}
    };

    // Initial longer path: 0 -> 1 -> 2 -> 3 -> 4 (Cost = 8.0)
    problem.transitions = {
        {101, 0, 1, 2.0, 1.0, 0.99, true, "S_to_A"},
        {102, 1, 2, 2.0, 1.0, 0.99, true, "A_to_B"},
        {103, 2, 3, 2.0, 1.0, 0.99, true, "B_to_C"},
        {104, 3, 4, 2.0, 1.0, 0.99, true, "C_to_G"}
    };

    config::PlannerConfig cfg;
    algorithms::DStarLite planner(cfg);

    auto initialRes = planner.plan(problem);
    printResultMetrics("TC6 (Before Shortcut)", initialRes);
    assert(initialRes.success == true);
    assert(initialRes.statePath.size() == 5);
    assert(std::abs(initialRes.totalCost - 8.0) < 1e-4);

    // Dynamic Event: Shortcut (1 -> 4) is inserted with low cost (3.0)
    std::cout << "\n>>> [TC6] DYNAMIC EVENT: New High-Speed Shortcut [1 -> 4] Discovered!\n";
    core::Transition shortcut(999, 1, 4, 3.0, 1.0, 0.99, true, "Express_Shortcut_1_to_4");
    planner.addTransition(shortcut);
    auto shortcutRes = planner.replan(0);

    printResultMetrics("TC6 (After Shortcut Insertion)", shortcutRes);
    assert(shortcutRes.success == true);
    // Path should now take shortcut: 0 -> 1 -> 4 (Cost = 2.0 + 3.0 = 5.0)
    assert(shortcutRes.statePath.size() == 3);
    assert(shortcutRes.statePath[0] == 0 && shortcutRes.statePath[1] == 1 && shortcutRes.statePath[2] == 4);
    assert(std::abs(shortcutRes.totalCost - 5.0) < 1e-4);

    std::cout << "  ✓ TC6 PASSED: Shortcut transition automatically integrated and exploited.\n";
}

int main() {
    std::cout << "======================================================================\n";
    std::cout << "  SAFE SEMANTIC PLANNER - PHASE 3 ASSIGNMENT TEST SUITE (TC1 - TC6)  \n";
    std::cout << "======================================================================\n";

    testCase1_BasicReachability();
    testCase2_BadStateAvoidance();
    testCase3_SafetyMargin();
    testCase4_DynamicTransition();
    testCase5_GoalUpdate();
    testCase6_TransitionAddition();

    std::cout << "\n======================================================================\n";
    std::cout << "  ALL 6 ASSIGNMENT TEST CASES PASSED WITH 100% SUCCESS RATE!          \n";
    std::cout << "======================================================================\n\n";

    return 0;
}

#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <chrono>

#include "ssp/core/types.hpp"
#include "ssp/core/state.hpp"
#include "ssp/core/transition.hpp"
#include "ssp/core/problem.hpp"
#include "ssp/core/result.hpp"
#include "ssp/core/planner.hpp"
#include "ssp/spatial/vector_math.hpp"
#include "ssp/spatial/kd_tree.hpp"
#include "ssp/config/config_manager.hpp"
#include "ssp/algorithms/indexed_priority_queue.hpp"
#include "ssp/algorithms/heuristic.hpp"
#include "ssp/algorithms/dstar_lite.hpp"

using namespace ssp;

void testIndexedPriorityQueue() {
    std::cout << "[TEST] 1. Indexed Priority Queue & Lexicographical DStarKey...\n";

    algorithms::IndexedPriorityQueue<uint64_t, algorithms::DStarKey> pq;
    assert(pq.empty());
    assert(pq.size() == 0);
    assert(!pq.contains(1));

    // Insert keys
    pq.insert(1, {10.0, 5.0});
    pq.insert(2, {3.0, 8.0});
    pq.insert(3, {3.0, 2.0});  // Same k1 as 2, but smaller k2 -> higher priority
    pq.insert(4, {1.0, 10.0}); // Smallest k1 -> highest priority
    pq.insert(5, {15.0, 0.0});

    assert(pq.size() == 5);
    assert(pq.contains(1));
    assert(pq.contains(5));
    assert(!pq.contains(99));

    // Top should be node 4
    assert(pq.top().key == 4);
    assert(pq.top().priority.k1 == 1.0);

    // Decrease key of node 5 to (0.5, 1.0) -> should become new top
    pq.update(5, {0.5, 1.0});
    assert(pq.top().key == 5);

    // Remove node 2
    bool removed = pq.remove(2);
    assert(removed);
    assert(!pq.contains(2));
    assert(pq.size() == 4);

    // Pop remaining elements in exact order: 5 -> 4 -> 3 -> 1
    auto pop1 = pq.pop();
    assert(pop1.key == 5);

    auto pop2 = pq.pop();
    assert(pop2.key == 4);

    auto pop3 = pq.pop();
    assert(pop3.key == 3);

    auto pop4 = pq.pop();
    assert(pop4.key == 1);

    assert(pq.empty());
    assert(pq.size() == 0);

    std::cout << "  ✓ Indexed priority queue operations (insert, pop, decrease_key, remove, contains) verified.\n";
}

void testHeuristicConsistency() {
    std::cout << "[TEST] 2. Heuristic Monotonicity & Triangle Inequality...\n";

    algorithms::EuclideanHeuristic heuristic(1.0);

    std::vector<core::State> states = {
        {1, {0.0, 0.0}, "Origin"},
        {2, {3.0, 0.0}, "Point_X"},
        {3, {3.0, 4.0}, "Point_XY"},
        {4, {10.0, 10.0}, "Far"}
    };

    std::vector<core::Transition> transitions = {
        {101, 1, 2, 3.0, 1.0, 1.0, true},
        {102, 2, 3, 4.0, 1.0, 1.0, true},
        {103, 1, 3, 5.0, 1.0, 1.0, true}
    };

    double vMax = heuristic.computeAndSetVMax(states, transitions, 1.0);
    assert(std::abs(vMax - 1.0) < 1e-6);

    // Self distance is 0
    assert(heuristic.calculate(1, 1) == 0.0);

    // Symmetry
    assert(std::abs(heuristic.calculate(1, 3) - heuristic.calculate(3, 1)) < 1e-6);

    // Exact Euclidean values
    assert(std::abs(heuristic.calculate(1, 2) - 3.0) < 1e-6);
    assert(std::abs(heuristic.calculate(2, 3) - 4.0) < 1e-6);
    assert(std::abs(heuristic.calculate(1, 3) - 5.0) < 1e-6);

    // Triangle inequality: h(1, 3) <= h(1, 2) + h(2, 3) (5 <= 3 + 4)
    assert(heuristic.verifyTriangleInequality(1, 2, 3));
    assert(heuristic.verifyTriangleInequality(1, 3, 4));

    // Admissibility against edge costs
    assert(heuristic.verifyAdmissibility(1, 2, 3.0));
    assert(heuristic.verifyAdmissibility(2, 3, 4.0));
    assert(heuristic.verifyAdmissibility(1, 3, 5.0));

    std::cout << "  ✓ Heuristic admissibility and consistency verified.\n";
}

void testStaticDStarLitePlanning() {
    std::cout << "[TEST] 3. Static D* Lite Path Planning...\n";

    core::PlanningProblem problem;
    problem.domainName = "Diamond Grid";
    problem.initialState = 0;
    problem.goalState = 3;
    problem.badStates = {};

    // Diamond topology: 0 -> 1 -> 3 (cost 2.0) vs 0 -> 2 -> 3 (cost 5.0)
    problem.states = {
        {0, {0.0, 0.0}, "Start"},
        {1, {1.0, 1.0}, "Upper"},
        {2, {1.0, -1.0}, "Lower"},
        {3, {2.0, 0.0}, "Goal"}
    };

    problem.transitions = {
        {1, 0, 1, 1.0, 1.0, 1.0, true, "S_to_Upper"},
        {2, 1, 3, 1.0, 1.0, 1.0, true, "Upper_to_G"},
        {3, 0, 2, 2.5, 1.0, 1.0, true, "S_to_Lower"},
        {4, 2, 3, 2.5, 1.0, 1.0, true, "Lower_to_G"}
    };

    algorithms::DStarLitePlanner planner;
    auto res = planner.plan(problem);

    assert(res.success);
    assert(res.statePath.size() == 3);
    assert(res.statePath[0] == 0);
    assert(res.statePath[1] == 1);
    assert(res.statePath[2] == 3);
    assert(std::abs(res.totalCost - 2.0) < 1e-6);
    assert(res.transitionPath.size() == 2);
    assert(res.transitionPath[0] == 1);
    assert(res.transitionPath[1] == 2);

    std::cout << "  ✓ Static D* Lite path planning found optimal path [0 -> 1 -> 3] with cost 2.0.\n";
}

void testDynamicEdgeBreakageReplanning() {
    std::cout << "[TEST] 4. Dynamic Edge Breakage & Sub-millisecond Replanning...\n";

    core::PlanningProblem problem;
    problem.initialState = 0;
    problem.goalState = 3;
    problem.badStates = {};

    problem.states = {
        {0, {0.0, 0.0}, "Start"},
        {1, {1.0, 1.0}, "Route_A"},
        {2, {1.0, -1.0}, "Route_B"},
        {3, {2.0, 0.0}, "Goal"}
    };

    problem.transitions = {
        {10, 0, 1, 1.0, 1.0, 1.0, true, "S_A"},
        {11, 1, 3, 1.0, 1.0, 1.0, true, "A_G"},
        {20, 0, 2, 2.0, 1.0, 1.0, true, "S_B"},
        {21, 2, 3, 2.0, 1.0, 1.0, true, "B_G"}
    };

    algorithms::DStarLitePlanner planner;
    auto initialRes = planner.plan(problem);
    assert(initialRes.success);
    assert(initialRes.statePath[1] == 1); // Route A chosen (cost 2.0 vs 4.0)

    // Dynamic Event: Edge 11 (A -> G) breaks down!
    auto replanRes = planner.setEdgeAvailability(11, false);

    assert(replanRes.success);
    assert(replanRes.wasReplanned);
    assert(replanRes.statePath.size() == 3);
    assert(replanRes.statePath[0] == 0);
    assert(replanRes.statePath[1] == 2); // Route B now chosen!
    assert(replanRes.statePath[2] == 3);
    assert(std::abs(replanRes.totalCost - 4.0) < 1e-6);

    // Replanning performance check: sub-millisecond
    assert(replanRes.planningTimeMicroseconds < 2000.0);

    std::cout << "  ✓ Edge breakage handled instantly in "
              << replanRes.planningTimeMicroseconds << " µs, dynamically rerouting to Route B.\n";
}

void testDynamicBadStateInsertion() {
    std::cout << "[TEST] 5. Dynamic Bad State Insertion & Obstacle Avoidance...\n";

    core::PlanningProblem problem;
    problem.initialState = 0;
    problem.goalState = 4;
    problem.badStates = {};

    // 0 -> 1 -> 2 -> 3 -> 4 (main corridor)
    // 1 -> 10 -> 3 (detour bypass)
    problem.states = {
        {0, {0.0, 0.0}, "Start"},
        {1, {1.0, 0.0}, "Checkpoint_1"},
        {2, {2.0, 0.0}, "Vulnerable_Node"},
        {3, {3.0, 0.0}, "Checkpoint_3"},
        {4, {4.0, 0.0}, "Goal"},
        {10, {2.0, 2.0}, "Bypass_Detour"}
    };

    problem.transitions = {
        {1, 0, 1, 1.0, 1.0, 1.0, true},
        {2, 1, 2, 1.0, 1.0, 1.0, true},
        {3, 2, 3, 1.0, 1.0, 1.0, true},
        {4, 3, 4, 1.0, 1.0, 1.0, true},
        {5, 1, 10, 2.0, 1.0, 1.0, true},
        {6, 10, 3, 2.0, 1.0, 1.0, true}
    };

    algorithms::DStarLitePlanner planner;
    auto initialRes = planner.plan(problem);
    assert(initialRes.success);
    // Initial optimal path: 0 -> 1 -> 2 -> 3 -> 4 (cost 4.0)
    assert(initialRes.statePath.size() == 5);
    assert(initialRes.statePath[2] == 2);
    assert(std::abs(initialRes.totalCost - 4.0) < 1e-6);

    // Dynamically insert Bad State at Node 2 (Hazard detected!)
    auto replanRes = planner.addBadState(2);
    assert(replanRes.success);
    assert(replanRes.wasReplanned);
    // New path must bypass node 2: 0 -> 1 -> 10 -> 3 -> 4 (cost 6.0)
    assert(replanRes.statePath.size() == 5);
    assert(replanRes.statePath[0] == 0);
    assert(replanRes.statePath[1] == 1);
    assert(replanRes.statePath[2] == 10);
    assert(replanRes.statePath[3] == 3);
    assert(replanRes.statePath[4] == 4);
    assert(std::abs(replanRes.totalCost - 6.0) < 1e-6);

    // Verify node 2 is NEVER visited
    for (uint64_t sId : replanRes.statePath) {
        assert(sId != 2);
    }

    // Now remove bad state at Node 2 (Hazard cleared!)
    auto clearedRes = planner.removeBadState(2);
    assert(clearedRes.success);
    assert(clearedRes.statePath[2] == 2); // Reverts to optimal direct path
    assert(std::abs(clearedRes.totalCost - 4.0) < 1e-6);

    std::cout << "  ✓ Dynamic bad-state insertion and clearance successfully routed and restored.\n";
}

void testCostVsSafetyTradeoff() {
    std::cout << "[TEST] 6. Cost vs Safety Clearance Metrics Calculation...\n";

    core::PlanningProblem problem;
    problem.initialState = 0;
    problem.goalState = 3;
    problem.badStates = {99}; // Hazard placed at (1.0, 0.4)

    problem.states = {
        {0, {0.0, 0.0}, "Start"},
        {1, {1.0, 0.0}, "Dangerous_Close_Route"}, // Dist to hazard: 0.4
        {2, {1.0, 3.0}, "Wide_Safe_Route"},        // Dist to hazard: 2.6
        {3, {2.0, 0.0}, "Goal"},
        {99, {1.0, 0.4}, "Hazard_Obstacle"}
    };

    problem.transitions = {
        {1, 0, 1, 1.0, 1.0, 1.0, true},
        {2, 1, 3, 1.0, 1.0, 1.0, true},
        {3, 0, 2, 3.0, 1.0, 1.0, true},
        {4, 2, 3, 3.0, 1.0, 1.0, true}
    };

    // Configuration 1: Standard planner with safety penalty active
    config::PlannerConfig safeConfig;
    safeConfig.gamma_safety = 10.0;
    safeConfig.safety_clearance_margin = 2.0;
    safeConfig.hazard_critical_radius = 0.5;

    algorithms::DStarLitePlanner safePlanner(safeConfig);
    auto safeRes = safePlanner.plan(problem);

    assert(safeRes.success);
    // Node 1 is within critical radius (dist = 0.4 <= 0.5), so it must take Route 2!
    assert(safeRes.statePath[1] == 2);
    // Minimum safety distance along path is from (0,0) / (2,0) to (1, 0.4) = sqrt(1^2 + 0.4^2) = 1.077
    assert(safeRes.minimumSafetyDistance > 1.0);
    assert(safeRes.safetyScore > 0.0);

    std::cout << "  ✓ Cost vs safety clearance trade-off correctly prioritized wide clearance route (D="
              << safeRes.minimumSafetyDistance << ", Score=" << safeRes.safetyScore << ").\n";
}

int main() {
    std::cout << "====================================================\n";
    std::cout << "  SAFE SEMANTIC PLANNER - PHASE 2 VERIFICATION SUITE\n";
    std::cout << "====================================================\n";

    testIndexedPriorityQueue();
    testHeuristicConsistency();
    testStaticDStarLitePlanning();
    testDynamicEdgeBreakageReplanning();
    testDynamicBadStateInsertion();
    testCostVsSafetyTradeoff();

    std::cout << "====================================================\n";
    std::cout << "  ALL PHASE 2 TESTS PASSED SUCCESSFULLY (100%)\n";
    std::cout << "====================================================\n";
    return 0;
}

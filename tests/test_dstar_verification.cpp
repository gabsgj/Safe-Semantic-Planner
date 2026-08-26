#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <unordered_set>

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

void testIndexedPriorityQueueStress() {
    std::cout << "[VERIFICATION] 1. IndexedPriorityQueue Stress & Invariant Test...\n";

    algorithms::IndexedPriorityQueue<uint64_t, algorithms::DStarKey> pq;
    assert(pq.empty());
    assert(pq.size() == 0);

    // Test tie-breaking on k2
    pq.insert(10, {5.0, 2.0});
    pq.insert(20, {5.0, 1.0}); // same k1, smaller k2
    pq.insert(30, {5.0, 3.0});
    pq.insert(40, {2.0, 10.0}); // smaller k1

    assert(pq.top().key == 40);
    assert(pq.pop().key == 40);
    assert(pq.pop().key == 20); // k2 = 1.0
    assert(pq.pop().key == 10); // k2 = 2.0
    assert(pq.pop().key == 30); // k2 = 3.0
    assert(pq.empty());

    // Randomized stress test with 500 elements
    std::mt19937 rng(1337);
    std::uniform_real_distribution<double> distK(0.0, 1000.0);
    std::uniform_int_distribution<int> opDist(0, 3);

    std::unordered_map<uint64_t, algorithms::DStarKey> mirror;
    uint64_t nextId = 1;

    for (int step = 0; step < 1000; ++step) {
        int op = opDist(rng);
        if (op == 0 || mirror.empty()) {
            // Insert
            uint64_t id = nextId++;
            algorithms::DStarKey key{distK(rng), distK(rng)};
            pq.insert(id, key);
            mirror[id] = key;
        } else if (op == 1) {
            // Update
            auto it = mirror.begin();
            std::advance(it, rng() % mirror.size());
            algorithms::DStarKey newKey{distK(rng), distK(rng)};
            pq.update(it->first, newKey);
            it->second = newKey;
        } else if (op == 2) {
            // Remove
            auto it = mirror.begin();
            std::advance(it, rng() % mirror.size());
            bool removed = pq.remove(it->first);
            assert(removed);
            mirror.erase(it);
        } else {
            // Pop top and verify against mirror min
            if (!pq.empty()) {
                auto minIt = mirror.begin();
                for (auto it = mirror.begin(); it != mirror.end(); ++it) {
                    if (it->second < minIt->second) {
                        minIt = it;
                    }
                }
                auto topEntry = pq.pop();
                assert(topEntry.key == minIt->first || topEntry.priority == minIt->second);
                mirror.erase(topEntry.key);
            }
        }
        assert(pq.size() == mirror.size());
    }

    std::cout << "  ✓ Priority Queue stress test & invariant verification passed.\n";
}

void testHeuristicHighDimConsistency() {
    std::cout << "[VERIFICATION] 2. Heuristic High-Dim Admissibility & Consistency...\n";

    algorithms::EuclideanHeuristic heuristic(1.0);
    size_t dim = 16;
    size_t numStates = 40;

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> coordDist(-50.0, 50.0);

    std::vector<core::State> states;
    for (size_t i = 0; i < numStates; ++i) {
        std::vector<double> coords(dim);
        for (size_t d = 0; d < dim; ++d) {
            coords[d] = coordDist(rng);
        }
        states.emplace_back(i, coords, "State_" + std::to_string(i));
    }

    std::vector<core::Transition> transitions;
    uint64_t transId = 1;
    for (size_t i = 0; i < numStates; ++i) {
        for (size_t j = i + 1; j < numStates; ++j) {
            double dist = spatial::euclideanDistance(states[i].embedding, states[j].embedding);
            double baseCost = dist * 1.2 + 0.5; // Cost > dist -> vMax will be <= 1.0
            transitions.emplace_back(transId++, states[i].id, states[j].id, baseCost);
            transitions.emplace_back(transId++, states[j].id, states[i].id, baseCost);
        }
    }

    heuristic.computeAndSetVMax(states, transitions, 1.0);

    // Verify Admissibility across all transitions
    for (const auto& t : transitions) {
        double hVal = heuristic.calculate(t.from, t.to);
        assert(hVal <= t.cost + core::EPSILON);
    }

    // Verify Monotonic Consistency (Triangle Inequality) across all triples
    for (size_t i = 0; i < numStates; ++i) {
        for (size_t j = 0; j < numStates; ++j) {
            for (size_t k = 0; k < numStates; ++k) {
                assert(heuristic.verifyTriangleInequality(states[i].id, states[j].id, states[k].id));
            }
        }
    }

    std::cout << "  ✓ 16-D Euclidean heuristic admissibility & consistency verified on all triples.\n";
}

void testMultipleAndOverlappingConstraints() {
    std::cout << "[VERIFICATION] 3. Multiple & Overlapping Constraints Handling...\n";

    core::PlanningProblem problem;
    problem.domainName = "Grid with Multiple Constraints";
    problem.initialState = 0;
    problem.goalState = 8;
    // 3x3 Grid:
    // 0 -- 1 -- 2
    // |    |    |
    // 3 -- 4 -- 5
    // |    |    |
    // 6 -- 7 -- 8
    // Constrain states 1, 4, 7 (vertical obstacle wall)
    problem.badStates = {1, 4, 7};

    problem.states = {
        {0, {0.0, 2.0}, "N0"}, {1, {1.0, 2.0}, "N1"}, {2, {2.0, 2.0}, "N2"},
        {3, {0.0, 1.0}, "N3"}, {4, {1.0, 1.0}, "N4"}, {5, {2.0, 1.0}, "N5"},
        {6, {0.0, 0.0}, "N6"}, {7, {1.0, 0.0}, "N7"}, {8, {2.0, 0.0}, "N8"}
    };

    problem.transitions = {
        {101, 0, 1, 1.0}, {102, 1, 2, 1.0},
        {103, 0, 3, 1.0}, {104, 1, 4, 1.0}, {105, 2, 5, 1.0},
        {106, 3, 4, 1.0}, {107, 4, 5, 1.0},
        {108, 3, 6, 1.0}, {109, 4, 7, 1.0}, {110, 5, 8, 1.0},
        {111, 6, 7, 1.0}, {112, 7, 8, 1.0},
        // External safe detour bridge: 6 -> 8 directly
        {201, 6, 8, 3.0, 1.0, 0.99, true, "South_Detour_Bridge"}
    };

    config::PlannerConfig cfg;
    algorithms::DStarLite planner(cfg);

    auto result = planner.plan(problem);
    assert(result.success);
    // Path must navigate around all bad states: 0 -> 3 -> 6 -> 8
    assert(result.statePath.size() == 4);
    assert(result.statePath[0] == 0);
    assert(result.statePath[1] == 3);
    assert(result.statePath[2] == 6);
    assert(result.statePath[3] == 8);

    // Verify NONE of {1, 4, 7} are in the path
    for (uint64_t sId : result.statePath) {
        assert(sId != 1 && sId != 4 && sId != 7);
    }

    // Now add another overlapping constraint: disable transition 201 (detour bridge)
    planner.setEdgeAvailability(201, false);
    auto blockedResult = planner.replan();
    // Since all routes to 8 are now blocked by bad states {1, 4, 7} or unavailable edge 201:
    assert(!blockedResult.success);
    assert(blockedResult.message.find("unreachable") != std::string::npos);

    // Re-enable edge 201
    planner.setEdgeAvailability(201, true);
    auto restoredResult = planner.replan();
    assert(restoredResult.success);
    assert(restoredResult.statePath.back() == 8);

    std::cout << "  ✓ Multiple constraints & overlapping edge blockers properly avoided.\n";
}

void testEdgeCases() {
    std::cout << "[VERIFICATION] 4. Edge Cases (Start==Goal, Bad Start/Goal, Disconnected)...\n";

    // 1. Start == Goal
    {
        core::PlanningProblem problem;
        problem.initialState = 5;
        problem.goalState = 5;
        problem.states = {{5, {1.0, 1.0}, "Self"}};
        problem.transitions = {};

        algorithms::DStarLite planner;
        auto res = planner.plan(problem);
        assert(res.success);
        assert(res.statePath.size() == 1 && res.statePath[0] == 5);
        assert(res.totalCost == 0.0);
    }

    // 2. Start is Bad State
    {
        core::PlanningProblem problem;
        problem.initialState = 1;
        problem.goalState = 2;
        problem.badStates = {1};
        problem.states = {{1, {0.0, 0.0}}, {2, {1.0, 1.0}}};
        problem.transitions = {{10, 1, 2, 1.0}};

        algorithms::DStarLite planner;
        auto res = planner.plan(problem);
        assert(!res.success);
        assert(res.message.find("bad state") != std::string::npos);
    }

    // 3. Goal is Bad State
    {
        core::PlanningProblem problem;
        problem.initialState = 1;
        problem.goalState = 2;
        problem.badStates = {2};
        problem.states = {{1, {0.0, 0.0}}, {2, {1.0, 1.0}}};
        problem.transitions = {{10, 1, 2, 1.0}};

        algorithms::DStarLite planner;
        auto res = planner.plan(problem);
        assert(!res.success);
        assert(res.message.find("bad state") != std::string::npos);
    }

    // 4. Replan to State 0 from another state
    {
        core::PlanningProblem problem;
        problem.initialState = 2;
        problem.goalState = 4;
        problem.states = {
            {0, {0.0, 0.0}, "Origin"},
            {1, {1.0, 0.0}, "Mid1"},
            {2, {2.0, 0.0}, "Mid2"},
            {4, {4.0, 0.0}, "Goal"}
        };
        problem.transitions = {
            {1, 0, 1, 1.0}, {2, 1, 2, 1.0}, {3, 2, 4, 2.0}, {4, 0, 4, 5.0}
        };

        algorithms::DStarLite planner;
        auto resInitial = planner.plan(problem);
        assert(resInitial.success);
        assert(resInitial.statePath[0] == 2);

        // Move agent backwards to Node 0 and replan(0)
        auto resMoved = planner.replan(0);
        assert(resMoved.success);
        assert(resMoved.statePath[0] == 0);
        assert(resMoved.statePath.back() == 4);
    }

    // 5. Cyclic graph with alternative shortcuts
    {
        core::PlanningProblem problem;
        problem.initialState = 1;
        problem.goalState = 3;
        problem.states = {
            {1, {0.0, 0.0}}, {2, {1.0, 0.0}}, {3, {2.0, 0.0}}
        };
        // Cycle between 1 and 2
        problem.transitions = {
            {1, 1, 2, 1.0}, {2, 2, 1, 1.0}, {3, 2, 3, 1.0}
        };

        algorithms::DStarLite planner;
        auto res = planner.plan(problem);
        assert(res.success);
        assert(res.statePath.size() == 3);
        assert(res.statePath[0] == 1 && res.statePath[1] == 2 && res.statePath[2] == 3);
    }

    std::cout << "  ✓ All edge cases correctly handled.\n";
}

void testDynamicAgentTraversalWithKeyModifier() {
    std::cout << "[VERIFICATION] 5. Agent Movement & km Key Modifier Accumulation...\n";

    // 1D chain of 6 states: 0 -> 1 -> 2 -> 3 -> 4 -> 5
    core::PlanningProblem problem;
    problem.initialState = 0;
    problem.goalState = 5;
    problem.states = {
        {0, {0.0, 0.0}}, {1, {1.0, 0.0}}, {2, {2.0, 0.0}},
        {3, {3.0, 0.0}}, {4, {4.0, 0.0}}, {5, {5.0, 0.0}}
    };
    problem.transitions = {
        {1, 0, 1, 1.0}, {2, 1, 2, 1.0}, {3, 2, 3, 1.0},
        {4, 3, 4, 1.0}, {5, 4, 5, 1.0}
    };

    algorithms::DStarLite planner;
    auto res0 = planner.plan(problem);
    assert(res0.success);
    assert(res0.statePath.size() == 6);

    // Agent moves to state 1
    auto res1 = planner.replan(1);
    assert(res1.success);
    assert(res1.statePath[0] == 1);
    assert(res1.statePath.size() == 5);

    // Agent moves to state 2, but edge 3 (2 -> 3) cost increases to 10.0 and edge 4 (3 -> 4) fails!
    // A detour (2 -> 5) is discovered!
    core::Transition detour(99, 2, 5, 3.5, 1.0, 0.99, true, "Express_Detour");
    planner.addTransition(detour);
    planner.setEdgeAvailability(4, false);

    auto res2 = planner.replan(2);
    assert(res2.success);
    assert(res2.statePath[0] == 2);
    assert(res2.statePath[1] == 5); // Takes express detour!
    assert(std::abs(res2.totalCost - 3.5) < 1e-6);

    std::cout << "  ✓ Agent traversal with km key modifier incremental update verified.\n";
}

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  SAFE SEMANTIC PLANNER - D* LITE VERIFICATION SUITE      \n";
    std::cout << "==========================================================\n";

    testIndexedPriorityQueueStress();
    testHeuristicHighDimConsistency();
    testMultipleAndOverlappingConstraints();
    testEdgeCases();
    testDynamicAgentTraversalWithKeyModifier();

    std::cout << "==========================================================\n";
    std::cout << "  ALL VERIFICATION TESTS PASSED WITH 100% SUCCESS RATE!   \n";
    std::cout << "==========================================================\n";
    return 0;
}

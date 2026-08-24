#include <iostream>
#include <cassert>
#include <vector>

#include "ssp/bonus/multi_goal_tsp.hpp"
#include "ssp/bonus/temporal_planner.hpp"
#include "ssp/bonus/knowledge_graph_planner.hpp"

using namespace ssp;

void testMultiGoalTsp() {
    std::cout << ">>> [BONUS TEST 1] Multi-Goal TSP Waypoint Sequencer (Bonus #2)\n";
    core::PlanningProblem prob;
    prob.domainName = "TSP Multi-Goal Grid";
    prob.initialState = 0;
    prob.goalState = 4;
    prob.badStates = {99};
    prob.states = {
        {0, {0.0, 0.0}, "Origin_S0"},
        {1, {2.0, 4.0}, "Waypoint_Goal_1"},
        {2, {6.0, 4.0}, "Waypoint_Goal_2"},
        {3, {4.0, 0.0}, "Waypoint_Goal_3"},
        {4, {8.0, 0.0}, "Terminal_Goal_4"},
        {99, {4.0, 2.0}, "Hazard_Core"}
    };
    prob.transitions = {
        {101, 0, 1, 4.0, 1.0, 0.99, true, "0 -> 1"},
        {102, 1, 2, 4.0, 1.0, 0.99, true, "1 -> 2"},
        {103, 0, 3, 4.0, 1.0, 0.99, true, "0 -> 3"},
        {104, 3, 4, 4.0, 1.0, 0.99, true, "3 -> 4"},
        {105, 2, 4, 4.0, 1.0, 0.99, true, "2 -> 4"},
        {106, 1, 3, 5.0, 1.0, 0.95, true, "1 -> 3"}
    };

    bonus::MultiGoalTspPlanner tspPlanner;
    std::vector<uint64_t> multiGoals = {1, 2, 4}; // Must visit 1, then 2, then 4

    auto res = tspPlanner.planMultiGoal(prob, multiGoals);
    assert(res.success);
    assert(res.optimalGoalOrder.size() == 3);
    assert(res.fullStatePath.front() == 0);
    assert(res.fullStatePath.back() == 4);

    std::cout << "  - Optimal Multi-Goal Order: ";
    for (size_t i = 0; i < res.optimalGoalOrder.size(); ++i) {
        std::cout << res.optimalGoalOrder[i] << (i + 1 < res.optimalGoalOrder.size() ? " -> " : "");
    }
    std::cout << "\n  - Total Spliced Cost: " << res.totalCost << "\n";
    std::cout << "  - Execution Latency:  " << res.executionTimeUs << " µs\n";
    std::cout << "  [PASSED] Multi-Goal TSP Waypoint Sequencing Verified.\n\n";
}

void testTemporalPlanner() {
    std::cout << ">>> [BONUS TEST 2] Time-Dependent Transition Availability (Bonus #3)\n";
    core::PlanningProblem prob;
    prob.domainName = "Temporal Network";
    prob.initialState = 0;
    prob.goalState = 2;
    prob.states = {
        {0, {0.0, 0.0}, "Start"},
        {1, {2.0, 2.0}, "HighSpeed_Bridge"},
        {2, {4.0, 0.0}, "Destination"},
        {3, {2.0, -2.0}, "Slow_Detour"}
    };
    prob.transitions = {
        {101, 0, 1, 2.0, 1.0, 0.99, true, "0 -> Bridge"},
        {102, 1, 2, 2.0, 1.0, 0.99, true, "Bridge -> 2"},
        {201, 0, 3, 5.0, 1.0, 0.99, true, "0 -> Detour"},
        {202, 3, 2, 5.0, 1.0, 0.99, true, "Detour -> 2"}
    };

    bonus::TemporalGraphPlanner tempPlanner;

    // Case A: Bridge is open in window [0.0, 10.0]
    std::unordered_map<uint64_t, bonus::TimeWindow> openWindows = {
        {101, {0.0, 10.0}},
        {102, {0.0, 10.0}}
    };
    auto resA = tempPlanner.planWithTimeWindows(prob, openWindows, 0.0);
    assert(resA.success);
    assert(resA.statePath[1] == 1); // Traverses high-speed bridge
    std::cout << "  - Day Window (Bridge Open):  Cost = " << resA.totalCost << " (Path: 0 -> 1 -> 2)\n";

    // Case B: Bridge closed for maintenance in window [0.0, 10.0] -> takes slow detour
    std::unordered_map<uint64_t, bonus::TimeWindow> closedWindows = {
        {101, {50.0, 100.0}}, // Closed now
        {102, {50.0, 100.0}}
    };
    auto resB = tempPlanner.planWithTimeWindows(prob, closedWindows, 0.0);
    assert(resB.success);
    assert(resB.statePath[1] == 3); // Traverses detour
    std::cout << "  - Night Window (Maintenance): Cost = " << resB.totalCost << " (Path: 0 -> 3 -> 2)\n";

    std::cout << "  [PASSED] Time-Dependent Transition Availability Verified.\n\n";
}

void testKnowledgeGraphReasoning() {
    std::cout << ">>> [BONUS TEST 3] Knowledge Graph Reasoning & Toxic Concept Avoidance (Bonus #6)\n";
    std::vector<std::string> entities = {
        "Symptom: Acute Severe Pain",
        "Drug: Non-Steroidal Anti-Inflammatory (NSAID)",
        "Condition: Peptic Ulcer Gastric Bleeding", // TOXIC / HAZARDOUS CONTRAINDICATION!
        "Drug: Selective COX-2 Inhibitor (Celecoxib)",
        "Target: Pain Relieved & Gastric Mucosa Protected"
    };

    std::vector<bonus::KnowledgeTriple> triples = {
        {"Symptom: Acute Severe Pain", "indicated_for", "Drug: Non-Steroidal Anti-Inflammatory (NSAID)", 0.95, 2.0},
        {"Drug: Non-Steroidal Anti-Inflammatory (NSAID)", "causes_adverse_event", "Condition: Peptic Ulcer Gastric Bleeding", 0.99, 1.0},
        {"Condition: Peptic Ulcer Gastric Bleeding", "leads_to", "Target: Pain Relieved & Gastric Mucosa Protected", 0.10, 10.0},
        {"Symptom: Acute Severe Pain", "safe_alternative", "Drug: Selective COX-2 Inhibitor (Celecoxib)", 0.99, 3.0},
        {"Drug: Selective COX-2 Inhibitor (Celecoxib)", "achieves_target", "Target: Pain Relieved & Gastric Mucosa Protected", 0.99, 3.0}
    };

    bonus::KnowledgeGraphPlanner kgPlanner;
    std::vector<std::string> toxicConcepts = {
        "Condition: Peptic Ulcer Gastric Bleeding"
    };

    auto res = kgPlanner.findSafeReasoningPath(
        entities,
        triples,
        "Symptom: Acute Severe Pain",
        "Target: Pain Relieved & Gastric Mucosa Protected",
        toxicConcepts
    );

    assert(res.success);
    // Must strictly avoid toxic gastric ulcer
    for (const auto& ent : res.entityPath) {
        assert(ent != "Condition: Peptic Ulcer Gastric Bleeding");
    }

    std::cout << "  - Reasoning Pathway: " << res.reasoningChain << "\n";
    std::cout << "  - Semantic Cost:     " << res.totalSemanticCost << "\n";
    std::cout << "  - Execution Latency: " << res.executionTimeUs << " µs\n";
    std::cout << "  [PASSED] Knowledge Graph Reasoning & Toxic Concept Avoidance Verified.\n\n";
}

int main() {
    std::cout << "==========================================================\n";
    std::cout << "    SAFE SEMANTIC PLANNER - BONUS FEATURES TEST SUITE     \n";
    std::cout << "==========================================================\n\n";

    testMultiGoalTsp();
    testTemporalPlanner();
    testKnowledgeGraphReasoning();

    std::cout << "==========================================================\n";
    std::cout << "  ALL BONUS ASSIGNMENT FEATURES PASSED (100% SUCCESS)     \n";
    std::cout << "==========================================================\n";
    return 0;
}

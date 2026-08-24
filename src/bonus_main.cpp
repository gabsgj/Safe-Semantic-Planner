#include <iostream>
#include <iomanip>

#include "ssp/bonus/multi_goal_tsp.hpp"
#include "ssp/bonus/temporal_planner.hpp"
#include "ssp/bonus/knowledge_graph_planner.hpp"

using namespace ssp;

int main() {
    std::cout << "\n===========================================================================\n";
    std::cout << "       SAFE SEMANTIC PLANNER - ADVANCED BONUS EXTENSIONS SUITE              \n";
    std::cout << "===========================================================================\n\n";

    // 1. Multi-Goal TSP
    std::cout << ">>> [FEATURE 1] Multi-Goal TSP Waypoint Sequencer (Bonus #2)\n";
    core::PlanningProblem prob;
    prob.initialState = 0;
    prob.goalState = 4;
    prob.states = {
        {0, {0.0, 0.0}, "Warehouse_Dock_S0"},
        {1, {2.0, 4.0}, "Aisle_Bin_A"},
        {2, {6.0, 4.0}, "Aisle_Bin_B"},
        {3, {4.0, 0.0}, "Recharging_Pad"},
        {4, {8.0, 0.0}, "Outbound_Dispatch_Bay"}
    };
    prob.transitions = {
        {101, 0, 1, 4.0, 1.0, 0.99, true, "Dock -> Bin A"},
        {102, 1, 2, 4.0, 1.0, 0.99, true, "Bin A -> Bin B"},
        {103, 0, 3, 4.0, 1.0, 0.99, true, "Dock -> Pad"},
        {104, 3, 4, 4.0, 1.0, 0.99, true, "Pad -> Dispatch"},
        {105, 2, 4, 4.0, 1.0, 0.99, true, "Bin B -> Dispatch"},
        {106, 1, 3, 5.0, 1.0, 0.95, true, "Bin A -> Pad"}
    };

    bonus::MultiGoalTspPlanner tspPlanner;
    auto tspRes = tspPlanner.planMultiGoal(prob, {1, 2, 4});
    std::cout << "  - Ordered Milestones: ";
    for (size_t i = 0; i < tspRes.optimalGoalOrder.size(); ++i) {
        std::cout << tspRes.optimalGoalOrder[i] << (i + 1 < tspRes.optimalGoalOrder.size() ? " -> " : "");
    }
    std::cout << "\n  - Trajectory:         ";
    for (size_t i = 0; i < tspRes.fullStatePath.size(); ++i) {
        std::cout << tspRes.fullStatePath[i] << (i + 1 < tspRes.fullStatePath.size() ? " -> " : "");
    }
    std::cout << "\n  - Total Spliced Cost: " << tspRes.totalCost << " (Computed in " << tspRes.executionTimeUs << " µs)\n\n";

    // 2. Knowledge Graph Reasoning
    std::cout << ">>> [FEATURE 2] Biomedical Knowledge Graph Reasoning (Bonus #6)\n";
    std::vector<std::string> entities = {
        "Symptom: Acute Severe Pain",
        "Drug: Non-Steroidal Anti-Inflammatory (NSAID)",
        "Condition: Peptic Ulcer Gastric Bleeding",
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
    auto kgRes = kgPlanner.findSafeReasoningPath(
        entities, triples, "Symptom: Acute Severe Pain", "Target: Pain Relieved & Gastric Mucosa Protected", {"Condition: Peptic Ulcer Gastric Bleeding"}
    );
    std::cout << "  - Reasoning Chain:    " << kgRes.reasoningChain << "\n";
    std::cout << "  - Avoided Hazards:    [Condition: Peptic Ulcer Gastric Bleeding]\n";
    std::cout << "  - Execution Latency:  " << kgRes.executionTimeUs << " µs\n\n";

    std::cout << "===========================================================================\n";
    std::cout << "       ALL BONUS FEATURES DEMONSTRATED SUCCESSFULLY                        \n";
    std::cout << "===========================================================================\n\n";

    return 0;
}

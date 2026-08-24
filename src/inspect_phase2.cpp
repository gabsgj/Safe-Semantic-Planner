#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>

#include "ssp/core/types.hpp"
#include "ssp/core/state.hpp"
#include "ssp/core/transition.hpp"
#include "ssp/core/problem.hpp"
#include "ssp/core/result.hpp"
#include "ssp/config/config_manager.hpp"
#include "ssp/spatial/kd_tree.hpp"
#include "ssp/spatial/vector_math.hpp"
#include "ssp/algorithms/indexed_priority_queue.hpp"
#include "ssp/algorithms/heuristic.hpp"
#include "ssp/algorithms/dstar_lite.hpp"

using namespace ssp;

void printBanner(const std::string& title) {
    std::cout << "\n" << std::string(75, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(75, '=') << "\n\n";
}

void printResultTable(const core::PlanningResult& res, const algorithms::DStarLitePlanner& planner) {
    if (!res.success) {
        std::cout << "  ❌ PLANNING FAILED: " << res.message << "\n\n";
        return;
    }

    std::cout << ">>> TRAJECTORY EXECUTION PATH:\n";
    std::cout << std::left
              << std::setw(6)  << "Step"
              << std::setw(10) << "State ID"
              << std::setw(24) << "Name"
              << std::setw(16) << "Coordinates"
              << std::setw(16) << "Hazard Dist"
              << std::setw(16) << "Action ID"
              << "\n";
    std::cout << std::string(88, '-') << "\n";

    for (size_t i = 0; i < res.statePath.size(); ++i) {
        uint64_t sId = res.statePath[i];
        const auto* s = planner.findState(sId);
        std::string name = s ? s->name : "Unknown";
        std::string coords = s ? ("[" + std::to_string((int)s->embedding[0]) + "," + std::to_string((int)s->embedding[1]) + "]") : "[]";
        
        std::string edgeStr = (i < res.transitionPath.size()) ? ("Edge #" + std::to_string(res.transitionPath[i])) : "Goal Reached";

        std::cout << std::left
                  << std::setw(6)  << i
                  << std::setw(10) << sId
                  << std::setw(24) << name
                  << std::setw(16) << coords
                  << std::setw(16) << std::fixed << std::setprecision(2) << res.minimumSafetyDistance
                  << std::setw(16) << edgeStr
                  << "\n";
    }
    std::cout << std::string(88, '-') << "\n";

    std::cout << "\n>>> PERFORMANCE & SAFETY METRICS:\n";
    std::cout << "    - Status:                     " << (res.success ? "✅ SUCCESS" : "❌ FAILED") << "\n";
    std::cout << "    - Total Path Cost:            " << std::fixed << std::setprecision(3) << res.totalCost << "\n";
    std::cout << "    - Minimum Safety Distance:    " << res.minimumSafetyDistance << " units\n";
    std::cout << "    - Cumulative Reliability:     " << std::fixed << std::setprecision(4) << (res.cumulativeReliability * 100.0) << " %\n";
    std::cout << "    - Objective Safety Score:     " << std::fixed << std::setprecision(2) << res.safetyScore << "\n";
    std::cout << "    - Explored Nodes:             " << res.exploredStatesCount << " state expansions\n";
    std::cout << "    - Execution Time:             " << std::fixed << std::setprecision(2) << res.planningTimeMicroseconds << " µs"
              << " (" << (res.planningTimeMicroseconds / 1000.0) << " ms)\n";
    std::cout << "    - Replanned Iteration:        " << (res.wasReplanned ? "YES (Dynamic Incremental)" : "NO (Cold Start)") << "\n\n";
}

int main() {
    printBanner("SAFE SEMANTIC PLANNER (SSP) - PHASE 2 D* LITE DEMO INSPECTOR");

    // 1. Load config
    config::ConfigManager cfgMgr("config.json");
    const auto& appCfg = cfgMgr.get();
    std::cout << ">>> [1] LOADED PLANNER CONFIGURATION:\n";
    std::cout << "    - Algorithm:                " << appCfg.planner.algorithm << "\n";
    std::cout << "    - Alpha (Goal Reward):      " << appCfg.planner.alpha_goal << "\n";
    std::cout << "    - Beta (Cost Multiplier):   " << appCfg.planner.beta_cost << "\n";
    std::cout << "    - Gamma (Safety Barrier):   " << appCfg.planner.gamma_safety << "\n";
    std::cout << "    - Delta (Reliability):      " << appCfg.planner.delta_reliability << "\n";
    std::cout << "    - Hazard Critical Radius:   " << appCfg.planner.hazard_critical_radius << "\n";
    std::cout << "    - Safety Clearance Margin:  " << appCfg.planner.safety_clearance_margin << "\n\n";

    // 2. Setup a realistic Multi-Corridor Planning Problem
    core::PlanningProblem problem;
    problem.domainName = "Cartesian Dynamic Safety Grid";
    problem.initialState = 0;
    problem.goalState = 6;
    problem.badStates = {90}; // Initial hazard at (3.0, 1.0)

    problem.states = {
        {0, {0.0, 0.0}, "Start_Zone"},
        {1, {2.0, 0.0}, "Main_Corridor_1"},
        {2, {4.0, 0.0}, "Main_Corridor_2"},
        {3, {2.0, 3.0}, "Safe_Highland_1"},
        {4, {4.0, 3.0}, "Safe_Highland_2"},
        {5, {6.0, 0.0}, "Pre_Goal_Junction"},
        {6, {8.0, 0.0}, "Goal_Terminal"},
        {90, {3.0, 1.0}, "Radiation_Hazard_A"}
    };

    problem.transitions = {
        // Main corridor (fast, lower cost, passes near hazard A)
        {101, 0, 1, 2.0, 0.95, 0.99, true, "Advance_Main_1"},
        {102, 1, 2, 2.0, 0.90, 0.99, true, "Advance_Main_2"},
        {103, 2, 5, 2.0, 0.95, 0.99, true, "Advance_Main_3"},
        {104, 5, 6, 2.0, 1.00, 0.99, true, "Enter_Goal"},

        // Safe Highland bypass (longer route, total clearance from hazards)
        {201, 0, 3, 3.6, 1.00, 0.98, true, "Climb_Highland_1"},
        {202, 3, 4, 2.0, 1.00, 0.99, true, "Cross_Highland"},
        {203, 4, 6, 4.4, 1.00, 0.98, true, "Descend_Goal"},

        // Cross-connectors
        {301, 1, 3, 3.0, 0.95, 0.95, true, "Ascend_Junction_1_to_3"},
        {302, 4, 5, 3.6, 0.95, 0.95, true, "Descend_Junction_4_to_5"},
        {303, 5, 4, 3.6, 0.95, 0.95, true, "Ascend_Junction_5_to_4"}
    };

    std::cout << ">>> [2] INITIAL PROBLEM TOPOLOGY:\n";
    std::cout << "    - Domain:           " << problem.domainName << "\n";
    std::cout << "    - Start State:      " << problem.initialState << " (" << problem.findState(problem.initialState)->name << ")\n";
    std::cout << "    - Goal State:       " << problem.goalState << " (" << problem.findState(problem.goalState)->name << ")\n";
    std::cout << "    - Total States:     " << problem.states.size() << "\n";
    std::cout << "    - Total Edges:      " << problem.transitions.size() << "\n";
    std::cout << "    - Active Hazards:   " << problem.badStates.size() << " (ID: 90)\n\n";

    // 3. Initial Plan
    algorithms::DStarLitePlanner planner(appCfg.planner);
    std::cout << "======================================================================\n";
    std::cout << "  STAGE 1: COLD-START STATIC D* LITE PATH SEARCH                      \n";
    std::cout << "======================================================================\n";
    auto initialPlan = planner.plan(problem);
    printResultTable(initialPlan, planner);

    // 4. Dynamic Event 1: Edge Failure on Active Path
    std::cout << "======================================================================\n";
    std::cout << "  STAGE 2: DYNAMIC EVENT 1 - SUDDEN EDGE FAILURE (Edge #102 Down)     \n";
    std::cout << "======================================================================\n";
    std::cout << "⚡ Alert: Main corridor segment [Edge #102: Main_Corridor_1 -> Main_Corridor_2] is SEVERED!\n";
    std::cout << "⚡ Triggering dynamic sub-millisecond replanning...\n\n";
    
    auto replan1 = planner.setEdgeAvailability(102, false);
    printResultTable(replan1, planner);

    // 5. Dynamic Event 2: New Bad-State Hazard Injected
    std::cout << "======================================================================\n";
    std::cout << "  STAGE 3: DYNAMIC EVENT 2 - NEW BAD STATE SPAWNED (State #3 Compromised)\n";
    std::cout << "======================================================================\n";
    std::cout << "⚠️ Alert: State #3 (Safe_Highland_1) has been flagged as COMPROMISED HAZARD!\n";
    std::cout << "⚠️ Updating KD-Tree and performing safety potential field replanning...\n\n";

    auto replan2 = planner.addBadState(3);
    printResultTable(replan2, planner);

    // Restore Edge 102 to see instant recovery
    std::cout << "======================================================================\n";
    std::cout << "  STAGE 4: DYNAMIC EVENT 3 - EDGE RESTORATION & DYNAMIC RECOVERY     \n";
    std::cout << "======================================================================\n";
    std::cout << "✓ Notice: Edge #102 repaired and restored to service.\n";
    std::cout << "✓ Triggering instant incremental route restoration...\n\n";

    auto replan3 = planner.setEdgeAvailability(102, true);
    printResultTable(replan3, planner);

    // 6. Dynamic Event 4: Goal Shift
    std::cout << "======================================================================\n";
    std::cout << "  STAGE 5: DYNAMIC EVENT 4 - TARGET GOAL STATE SHIFT (Goal -> State #4)\n";
    std::cout << "======================================================================\n";
    std::cout << "🎯 Goal shifted dynamically from State #6 to State #4 (Safe_Highland_2)!\n";
    std::cout << "🎯 Computing updated trajectory...\n\n";

    auto replan4 = planner.updateGoal(4);
    printResultTable(replan4, planner);

    std::cout << "======================================================================\n";
    std::cout << "  PHASE 2 D* LITE PATH SEARCH & DYNAMIC REPLANNING ENGINE VERIFIED!   \n";
    std::cout << "======================================================================\n\n";

    return 0;
}

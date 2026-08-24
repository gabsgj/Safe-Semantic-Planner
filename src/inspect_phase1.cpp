#include <iostream>
#include <iomanip>
#include <vector>
#include "ssp/core/problem.hpp"
#include "ssp/core/result.hpp"
#include "ssp/spatial/vector_math.hpp"
#include "ssp/spatial/kd_tree.hpp"
#include "ssp/config/config_manager.hpp"

using namespace ssp;

int main() {
    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "          SAFE SEMANTIC PLANNER (SSP) - PHASE 1 DEMO INSPECTOR        \n";
    std::cout << "======================================================================\n\n";

    // 1. Load root config.json
    config::ConfigManager cfgMgr("config.json");
    const auto& cfg = cfgMgr.get();
    std::cout << ">>> [1] CONFIGURATION LOADED (config.json):\n";
    std::cout << "    - Algorithm:                " << cfg.planner.algorithm << "\n";
    std::cout << "    - Alpha (Goal Reward):      " << cfg.planner.alpha_goal << "\n";
    std::cout << "    - Beta (Cost Penalty):      " << cfg.planner.beta_cost << "\n";
    std::cout << "    - Gamma (Safety Multiplier):" << cfg.planner.gamma_safety << "\n";
    std::cout << "    - Delta (Reliability):      " << cfg.planner.delta_reliability << "\n";
    std::cout << "    - Spatial Index:            " << cfg.spatial.index_type << "\n\n";

    // 2. Create sample problem with 2D/3D embeddings
    core::PlanningProblem problem;
    problem.domainName = "Cartesian Safety Corridor";
    problem.initialState = 0;
    problem.goalState = 4;
    problem.badStates = {2, 5}; // Bad state hazards!

    problem.states = {
        {0, {0.0, 0.0}, "Start (s_I)"},
        {1, {2.0, 1.0}, "WayPoint_A"},
        {2, {2.5, 3.0}, "Hazard_LethalZone (B1)"},
        {3, {4.0, 1.0}, "WayPoint_B"},
        {4, {6.0, 2.0}, "Goal (s_G)"},
        {5, {4.5, 3.5}, "Hazard_Radiation (B2)"}
    };

    problem.transitions = {
        {101, 0, 1, 2.23, 1.0, 0.99, true, "Edge_0_to_1"},
        {102, 1, 3, 2.00, 0.9, 0.95, true, "Edge_1_to_3"},
        {103, 3, 4, 2.23, 1.0, 0.99, true, "Edge_3_to_4"},
        {104, 1, 2, 2.06, 0.1, 0.50, true, "Edge_1_to_Danger"}
    };

    std::cout << ">>> [2] CREATED PLANNING PROBLEM:\n";
    std::cout << "    - Domain:        " << problem.domainName << "\n";
    std::cout << "    - Initial State: " << problem.initialState << " (" << problem.findState(problem.initialState)->name << ")\n";
    std::cout << "    - Goal State:    " << problem.goalState << " (" << problem.findState(problem.goalState)->name << ")\n";
    std::cout << "    - Bad States:    " << problem.badStates.size() << " obstacles (IDs: ";
    for (size_t i = 0; i < problem.badStates.size(); ++i) {
        std::cout << problem.badStates[i] << (i + 1 < problem.badStates.size() ? ", " : "");
    }
    std::cout << ")\n\n";

    // 3. Build KD-Tree on Bad States
    std::cout << ">>> [3] BUILDING SPATIAL KD-TREE ON BAD STATES:\n";
    std::vector<spatial::KDPoint> hazardPoints;
    for (uint64_t badId : problem.badStates) {
        const auto* s = problem.findState(badId);
        if (s) {
            hazardPoints.emplace_back(s->id, s->embedding);
            std::cout << "    + Indexed Bad State #" << s->id << " (" << s->name << ") at coordinates: [";
            for (size_t d = 0; d < s->embedding.size(); ++d) {
                std::cout << s->embedding[d] << (d + 1 < s->embedding.size() ? ", " : "");
            }
            std::cout << "]\n";
        }
    }

    spatial::KDTree kdTree;
    kdTree.build(hazardPoints);
    std::cout << "    ✓ KD-Tree built successfully with " << kdTree.size() << " hazard nodes.\n\n";

    // 4. Live Spatial Clearance Queries
    std::cout << ">>> [4] LIVE SPATIAL CLEARANCE & HAZARD POTENTIAL EVALUATION:\n";
    std::cout << std::left 
              << std::setw(10) << "State ID"
              << std::setw(25) << "Name"
              << std::setw(18) << "Coordinates"
              << std::setw(18) << "Nearest Hazard"
              << std::setw(18) << "Clearance Dist"
              << std::setw(15) << "Status" << "\n";
    std::cout << std::string(104, '-') << "\n";

    for (const auto& st : problem.states) {
        auto nn = kdTree.nearestNeighbor(st.embedding);
        bool isBad = problem.isBadState(st.id);

        std::string coordStr = "[" + std::to_string((int)st.embedding[0]) + "," + std::to_string((int)st.embedding[1]) + "]";
        std::string status = isBad ? "⚠️ LETHAL (BAD)" : (nn.distance < 1.5 ? "⚡ WARNING ZONE" : "✓ SAFE CLEAR");

        std::cout << std::left 
                  << std::setw(10) << st.id
                  << std::setw(25) << st.name
                  << std::setw(18) << coordStr
                  << std::setw(18) << ("BadState #" + std::to_string(nn.id))
                  << std::setw(18) << std::fixed << std::setprecision(3) << nn.distance
                  << std::setw(15) << status << "\n";
    }

    std::cout << "\n======================================================================\n";
    std::cout << "  PHASE 1 ENGINE IS READY FOR PHASE 2 D* LITE PATH SEARCH ENGINE!     \n";
    std::cout << "======================================================================\n\n";

    return 0;
}

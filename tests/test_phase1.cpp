#include <iostream>
#include <cassert>
#include <cmath>
#include <random>
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

using namespace ssp;

void testVectorMath() {
    std::cout << "[TEST] Running Vector Math Tests...\n";
    std::vector<double> v1 = {0.0, 0.0};
    std::vector<double> v2 = {3.0, 4.0};

    double dist = spatial::euclideanDistance(v1, v2);
    assert(std::abs(dist - 5.0) < 1e-6);

    double manhattan = spatial::manhattanDistance(v1, v2);
    assert(std::abs(manhattan - 7.0) < 1e-6);

    std::vector<double> v3 = {1.0, 0.0};
    std::vector<double> v4 = {0.0, 1.0};
    double cosDist = spatial::cosineDistance(v3, v4);
    assert(std::abs(cosDist - 1.0) < 1e-6); // Orthogonal vectors -> cosine dist = 1.0

    std::cout << "  ✓ Vector Math calculations verified.\n";
}

void testCoreInterfacesAndJson() {
    std::cout << "[TEST] Running Core Interfaces & JSON Serialization Tests...\n";

    core::PlanningProblem problem;
    problem.initialState = 0;
    problem.goalState = 3;
    problem.badStates = {2};
    problem.states = {
        {0, {0.0, 0.0}, "Start"},
        {1, {1.0, 1.0}, "Checkpoint"},
        {2, {2.0, 2.0}, "HazardZone"},
        {3, {4.0, 4.0}, "Goal"}
    };
    problem.transitions = {
        {101, 0, 1, 1.414, 1.0, 0.99, true, "Advance_1"},
        {102, 1, 3, 4.242, 0.9, 0.95, true, "Advance_2"}
    };

    nlohmann::json j = problem;
    std::string serialized = j.dump();
    assert(!serialized.empty());

    nlohmann::json parsedJson = nlohmann::json::parse(serialized);
    auto deserializedProblem = parsedJson.get<core::PlanningProblem>();

    assert(deserializedProblem.initialState == 0);
    assert(deserializedProblem.goalState == 3);
    assert(deserializedProblem.badStates.size() == 1);
    assert(deserializedProblem.badStates[0] == 2);
    assert(deserializedProblem.states.size() == 4);
    assert(deserializedProblem.transitions.size() == 2);
    assert(deserializedProblem.isBadState(2));
    assert(!deserializedProblem.isBadState(1));

    std::cout << "  ✓ Core struct JSON roundtrip verified.\n";
}

void testKDTree() {
    std::cout << "[TEST] Running KD-Tree Spatial Index Tests...\n";

    // 1. Basic 2D KD-Tree
    std::vector<spatial::KDPoint> points = {
        {10, {2.0, 3.0}},
        {20, {5.0, 4.0}},
        {30, {9.0, 6.0}},
        {40, {4.0, 7.0}},
        {50, {8.0, 1.0}},
        {60, {7.0, 2.0}}
    };

    spatial::KDTree tree;
    tree.build(points);
    assert(tree.size() == 6);

    auto nn = tree.nearestNeighbor({9.0, 2.0});
    assert(nn.id == 50 || nn.id == 60);
    double expectedMinDist = std::min(
        spatial::euclideanDistance({9.0, 2.0}, {8.0, 1.0}),
        spatial::euclideanDistance({9.0, 2.0}, {7.0, 2.0})
    );
    assert(std::abs(nn.distance - expectedMinDist) < 1e-6);

    // 2. High-Dimensional KD-Tree vs Brute Force (d=8)
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(-100.0, 100.0);
    size_t numPoints = 200;
    size_t dim = 8;

    std::vector<spatial::KDPoint> highDimPoints;
    for (size_t i = 0; i < numPoints; ++i) {
        std::vector<double> coords(dim);
        for (size_t d = 0; d < dim; ++d) {
            coords[d] = dist(rng);
        }
        highDimPoints.emplace_back(i + 1, coords);
    }

    spatial::KDTree highDimTree;
    highDimTree.build(highDimPoints);

    // Query 50 random points and assert KD-Tree finds the EXACT same nearest neighbor as brute force
    for (int q = 0; q < 50; ++q) {
        std::vector<double> query(dim);
        for (size_t d = 0; d < dim; ++d) {
            query[d] = dist(rng);
        }

        // Brute force nearest neighbor
        uint64_t bfBestId = 0;
        double bfBestDist = core::INF_COST;
        for (const auto& pt : highDimPoints) {
            double dVal = spatial::euclideanDistance(pt.coordinates, query);
            if (dVal < bfBestDist) {
                bfBestDist = dVal;
                bfBestId = pt.id;
            }
        }

        // KD-Tree nearest neighbor
        auto kdResult = highDimTree.nearestNeighbor(query);
        assert(std::abs(kdResult.distance - bfBestDist) < 1e-6);
        (void)bfBestId; // silence unused warning if compiler checks
    }

    // 3. Empty Tree Edge Case
    spatial::KDTree emptyTree;
    auto emptyRes = emptyTree.nearestNeighbor({1.0, 2.0});
    assert(emptyRes.distance == core::INF_COST);

    std::cout << "  ✓ KD-Tree spatial verification and brute-force equivalence passed.\n";
}

void testConfigManager() {
    std::cout << "[TEST] Running Config Manager Tests...\n";
    config::ConfigManager cfgMgr("config.json");
    const auto& appCfg = cfgMgr.get();

    assert(appCfg.planner.algorithm == "dstar_lite");
    assert(appCfg.planner.alpha_goal > 0.0);
    assert(appCfg.planner.beta_cost > 0.0);
    assert(appCfg.planner.gamma_safety > 0.0);
    assert(appCfg.server.port == 8080);
    assert(appCfg.spatial.index_type == "kdtree");

    std::cout << "  ✓ ConfigManager loaded root config.json successfully.\n";
}

int main() {
    std::cout << "====================================================\n";
    std::cout << "  SAFE SEMANTIC PLANNER - PHASE 1 VERIFICATION SUITE\n";
    std::cout << "====================================================\n";

    testVectorMath();
    testCoreInterfacesAndJson();
    testKDTree();
    testConfigManager();

    std::cout << "====================================================\n";
    std::cout << "  ALL PHASE 1 TESTS PASSED SUCCESSFULLY (100%)\n";
    std::cout << "====================================================\n";
    return 0;
}

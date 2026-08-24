#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <iostream>

#include "ssp/core/problem.hpp"
#include "ssp/core/result.hpp"
#include "ssp/algorithms/dstar_lite.hpp"

namespace ssp::bonus {

/**
 * @brief Multi-Goal TSP Waypoint Sequencer (Bonus Feature #2)
 * 
 * Computes the optimal ordering to visit multiple goals G = {g1, g2, ..., gm}
 * while avoiding hazardous bad states B and minimizing cumulative path cost.
 */
class MultiGoalTspPlanner {
public:
    explicit MultiGoalTspPlanner(config::PlannerConfig config = config::PlannerConfig{})
        : config_(std::move(config)), planner_(config_) {}

    struct MultiGoalResult {
        bool success{false};
        std::vector<uint64_t> optimalGoalOrder;
        std::vector<uint64_t> fullStatePath;
        std::vector<uint64_t> fullTransitionPath;
        double totalCost{0.0};
        double minSafetyClearance{999.0};
        double cumulativeReliability{1.0};
        double executionTimeUs{0.0};
    };

    [[nodiscard]] MultiGoalResult planMultiGoal(
        const core::PlanningProblem& baseProblem,
        const std::vector<uint64_t>& goals
    ) {
        MultiGoalResult result;
        if (goals.empty()) return result;

        auto startTs = std::chrono::high_resolution_clock::now();

        // 1. If only 1 goal, standard planning
        if (goals.size() == 1) {
            core::PlanningProblem prob = baseProblem;
            prob.goalState = goals[0];
            auto planRes = planner_.plan(prob);
            if (planRes.success) {
                result.success = true;
                result.optimalGoalOrder = goals;
                result.fullStatePath = planRes.statePath;
                result.fullTransitionPath = planRes.transitionPath;
                result.totalCost = planRes.totalCost;
                result.minSafetyClearance = planRes.minimumSafetyDistance;
                result.cumulativeReliability = planRes.cumulativeReliability;
            }
            auto endTs = std::chrono::high_resolution_clock::now();
            result.executionTimeUs = std::chrono::duration<double, std::micro>(endTs - startTs).count();
            return result;
        }

        // 2. Solve Multi-Goal ordering via Permutation / Branch-and-Bound over D* Lite distance matrix
        std::vector<uint64_t> currentPerm = goals;
        std::sort(currentPerm.begin(), currentPerm.end());

        double bestCost = std::numeric_limits<double>::infinity();
        std::vector<uint64_t> bestOrder;
        std::vector<core::PlanningResult> bestSegments;

        do {
            double permCost = 0.0;
            uint64_t currStart = baseProblem.initialState;
            bool permFeasible = true;
            std::vector<core::PlanningResult> segResults;

            for (uint64_t targetGoal : currentPerm) {
                core::PlanningProblem segProb = baseProblem;
                segProb.initialState = currStart;
                segProb.goalState = targetGoal;

                auto segRes = planner_.plan(segProb);
                if (!segRes.success) {
                    permFeasible = false;
                    break;
                }

                permCost += segRes.totalCost;
                segResults.push_back(segRes);
                currStart = targetGoal;
            }

            if (permFeasible && permCost < bestCost) {
                bestCost = permCost;
                bestOrder = currentPerm;
                bestSegments = segResults;
            }
        } while (std::next_permutation(currentPerm.begin(), currentPerm.end()));

        if (bestSegments.empty()) {
            result.success = false;
            return result;
        }

        // 3. Stitch optimal trajectory
        result.success = true;
        result.optimalGoalOrder = bestOrder;
        result.totalCost = bestCost;

        for (size_t i = 0; i < bestSegments.size(); ++i) {
            const auto& seg = bestSegments[i];
            result.minSafetyClearance = std::min(result.minSafetyClearance, seg.minimumSafetyDistance);
            result.cumulativeReliability *= seg.cumulativeReliability;

            if (i == 0) {
                result.fullStatePath = seg.statePath;
                result.fullTransitionPath = seg.transitionPath;
            } else {
                for (size_t k = 1; k < seg.statePath.size(); ++k) {
                    result.fullStatePath.push_back(seg.statePath[k]);
                }
                for (uint64_t t : seg.transitionPath) {
                    result.fullTransitionPath.push_back(t);
                }
            }
        }

        auto endTs = std::chrono::high_resolution_clock::now();
        result.executionTimeUs = std::chrono::duration<double, std::micro>(endTs - startTs).count();
        return result;
    }

private:
    config::PlannerConfig config_;
    algorithms::DStarLitePlanner planner_;
};

} // namespace ssp::bonus

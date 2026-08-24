#pragma once

#include <vector>
#include <unordered_map>
#include <chrono>
#include <iostream>

#include "ssp/core/problem.hpp"
#include "ssp/core/result.hpp"
#include "ssp/algorithms/dstar_lite.hpp"

namespace ssp::bonus {

struct TimeWindow {
    double startTime{0.0};
    double endTime{std::numeric_limits<double>::infinity()};
};

struct TemporalTransition {
    core::Transition baseTransition;
    TimeWindow window;
};

/**
 * @brief Time-Dependent Transition Availability Planner (Bonus Feature #3)
 */
class TemporalGraphPlanner {
public:
    explicit TemporalGraphPlanner(config::PlannerConfig config = config::PlannerConfig{})
        : config_(std::move(config)), planner_(config_) {}

    struct TemporalPlanResult {
        bool success{false};
        std::vector<uint64_t> statePath;
        std::vector<uint64_t> transitionPath;
        double totalCost{0.0};
        double totalDuration{0.0};
        double minSafetyClearance{0.0};
        double executionTimeUs{0.0};
    };

    [[nodiscard]] TemporalPlanResult planWithTimeWindows(
        const core::PlanningProblem& baseProblem,
        const std::unordered_map<uint64_t, TimeWindow>& timeWindows,
        double initialTime = 0.0
    ) {
        TemporalPlanResult result;
        auto startTs = std::chrono::high_resolution_clock::now();

        core::PlanningProblem prob = baseProblem;
        double currentTime = initialTime;

        // Iteratively plan and check temporal window validity
        int maxIters = 20;
        int iter = 0;

        while (iter < maxIters) {
            iter++;
            // Mask unavailable transitions at currentTime
            for (auto& t : prob.transitions) {
                auto it = timeWindows.find(t.id);
                if (it != timeWindows.end()) {
                    bool isOpen = (currentTime >= it->second.startTime && currentTime <= it->second.endTime);
                    t.available = isOpen;
                }
            }

            auto planRes = planner_.plan(prob);
            if (!planRes.success) {
                break;
            }

            // Verify if schedule matches
            bool allWindowsValid = true;
            double simTime = initialTime;

            for (size_t i = 0; i < planRes.transitionPath.size(); ++i) {
                uint64_t tid = planRes.transitionPath[i];
                auto it = timeWindows.find(tid);
                if (it != timeWindows.end()) {
                    if (simTime < it->second.startTime || simTime > it->second.endTime) {
                        allWindowsValid = false;
                        // Temporarily disable closed transition and replan
                        for (auto& t : prob.transitions) {
                            if (t.id == tid) { t.available = false; break; }
                        }
                        break;
                    }
                }
                simTime += 2.0; // Simulated edge traversal delay
            }

            if (allWindowsValid) {
                result.success = true;
                result.statePath = planRes.statePath;
                result.transitionPath = planRes.transitionPath;
                result.totalCost = planRes.totalCost;
                result.totalDuration = simTime - initialTime;
                result.minSafetyClearance = planRes.minimumSafetyDistance;
                break;
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

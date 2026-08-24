#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "third_party/nlohmann/json.hpp"

namespace ssp::core {

class PlanningResult {
public:
    bool success{false};
    std::vector<uint64_t> statePath;
    std::vector<uint64_t> transitionPath;
    double totalCost{0.0};
    double safetyScore{0.0};

    // Extended benchmark and profiling metrics
    double minimumSafetyDistance{0.0};
    double cumulativeReliability{1.0};
    size_t exploredStatesCount{0};
    double planningTimeMicroseconds{0.0};
    bool wasReplanned{false};
    std::string message{""};

    PlanningResult() = default;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        PlanningResult,
        success,
        statePath,
        transitionPath,
        totalCost,
        safetyScore,
        minimumSafetyDistance,
        cumulativeReliability,
        exploredStatesCount,
        planningTimeMicroseconds,
        wasReplanned,
        message
    )
};

} // namespace ssp::core

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "ssp/core/state.hpp"
#include "ssp/core/transition.hpp"
#include "third_party/nlohmann/json.hpp"

namespace ssp::core {

class PlanningProblem {
public:
    uint64_t initialState{0};
    uint64_t goalState{0};
    std::vector<uint64_t> badStates;
    std::vector<State> states;
    std::vector<Transition> transitions;
    std::string domainName{"General Cartesian Domain"};

    PlanningProblem() = default;

    // Helper utilities for fast lookup
    [[nodiscard]] bool isBadState(uint64_t stateId) const {
        for (uint64_t b : badStates) {
            if (b == stateId) return true;
        }
        return false;
    }

    [[nodiscard]] const State* findState(uint64_t stateId) const {
        for (const auto& s : states) {
            if (s.id == stateId) return &s;
        }
        return nullptr;
    }

    [[nodiscard]] const Transition* findTransition(uint64_t transId) const {
        for (const auto& t : transitions) {
            if (t.id == transId) return &t;
        }
        return nullptr;
    }

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(PlanningProblem, initialState, goalState, badStates, states, transitions, domainName)
};

} // namespace ssp::core

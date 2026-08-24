#pragma once

#include "ssp/core/problem.hpp"
#include "ssp/core/result.hpp"

namespace ssp::core {

class Planner {
public:
    virtual ~Planner() = default;
    virtual PlanningResult plan(const PlanningProblem& problem) = 0;
};

} // namespace ssp::core

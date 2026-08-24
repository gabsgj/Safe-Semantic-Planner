#pragma once

#include <string>
#include <vector>
#include <memory>

#include "ssp/core/problem.hpp"

namespace ssp::domains {

/**
 * @brief Abstract base class for enterprise domain model generators.
 */
class DomainTemplate {
public:
    virtual ~DomainTemplate() = default;

    [[nodiscard]] virtual std::string getName() const = 0;
    [[nodiscard]] virtual std::string getDescription() const = 0;
    [[nodiscard]] virtual std::vector<std::string> getDimensionNames() const = 0;
    [[nodiscard]] virtual core::PlanningProblem createProblem() const = 0;
};

} // namespace ssp::domains

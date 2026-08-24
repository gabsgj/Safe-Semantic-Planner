#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <limits>

namespace ssp::core {

using StateId = uint64_t;
using TransitionId = uint64_t;

constexpr double INF_COST = std::numeric_limits<double>::infinity();
constexpr double EPSILON = 1e-9;

} // namespace ssp::core

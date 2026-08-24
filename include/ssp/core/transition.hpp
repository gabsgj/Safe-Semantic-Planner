#pragma once

#include <cstdint>
#include <string>
#include "third_party/nlohmann/json.hpp"

namespace ssp::core {

class Transition {
public:
    uint64_t id{0};
    uint64_t from{0};
    uint64_t to{0};
    double cost{1.0};
    double safety{1.0};
    double reliability{1.0};
    bool available{true};
    std::string name{""}; // Optional action name (e.g., "POST /charge_card")

    Transition() = default;
    Transition(uint64_t id, uint64_t from, uint64_t to, double cost = 1.0,
               double safety = 1.0, double reliability = 1.0, bool available = true,
               std::string name = "")
        : id(id), from(from), to(to), cost(cost), safety(safety),
          reliability(reliability), available(available), name(std::move(name)) {}

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Transition, id, from, to, cost, safety, reliability, available, name)
};

} // namespace ssp::core

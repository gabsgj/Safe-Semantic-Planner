#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "third_party/nlohmann/json.hpp"

namespace ssp::core {

class State {
public:
    uint64_t id{0};
    std::vector<double> embedding;
    std::string name{""}; // Optional semantic label (e.g., "PaymentService")

    State() = default;
    State(uint64_t id, std::vector<double> embedding, std::string name = "")
        : id(id), embedding(std::move(embedding)), name(std::move(name)) {}

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(State, id, embedding, name)
};

} // namespace ssp::core

#pragma once

#include <vector>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <iostream>
#include "ssp/core/types.hpp"
#include "ssp/core/state.hpp"
#include "ssp/core/transition.hpp"
#include "ssp/spatial/vector_math.hpp"

namespace ssp::algorithms {

/**
 * @brief Admissible and consistent Euclidean heuristic in R^d with v_max velocity scaling.
 * 
 * Given state embeddings x_u, x_v in R^d, and maximum transition efficiency:
 *   v_max = max_{(u,v) in T, cost > 0} ( ||x_u - x_v||_2 / cost(u,v) )
 * The heuristic is:
 *   h(u, v) = ||x_u - x_v||_2 / v_max
 * 
 * Guarantee:
 * 1. Admissibility: h(u, v) <= c(u, v) <= c'(u, v) for all valid edges.
 * 2. Monotonic consistency: h(u, w) <= h(u, v) + h(v, w) <= c'(u, v) + h(v, w).
 */
class EuclideanHeuristic {
private:
    std::unordered_map<core::StateId, std::vector<double>> embeddings_;
    double vMax_{1.0};

public:
    explicit EuclideanHeuristic(double vMax = 1.0) : vMax_(vMax > 0.0 ? vMax : 1.0) {}

    void setVMax(double vMax) noexcept {
        vMax_ = (vMax > 0.0) ? vMax : 1.0;
    }

    [[nodiscard]] double getVMax() const noexcept {
        return vMax_;
    }

    void setEmbedding(core::StateId id, std::vector<double> embedding) {
        embeddings_[id] = std::move(embedding);
    }

    void loadStates(const std::vector<core::State>& states) {
        embeddings_.clear();
        for (const auto& s : states) {
            embeddings_[s.id] = s.embedding;
        }
    }

    /**
     * @brief Computes v_max = max (distance / cost) across all given transitions.
     * Ensures heuristic is strictly admissible across the whole graph.
     */
    double computeAndSetVMax(
        const std::vector<core::State>& states,
        const std::vector<core::Transition>& transitions,
        double minVMax = 1.0
    ) {
        loadStates(states);

        double maxEfficiency = minVMax > 0.0 ? minVMax : 1.0;
        for (const auto& t : transitions) {
            if (!t.available || t.cost <= core::EPSILON) continue;

            auto itFrom = embeddings_.find(t.from);
            auto itTo = embeddings_.find(t.to);
            if (itFrom != embeddings_.end() && itTo != embeddings_.end()) {
                double dist = spatial::euclideanDistance(itFrom->second, itTo->second);
                double efficiency = dist / t.cost;
                if (efficiency > maxEfficiency) {
                    maxEfficiency = efficiency;
                }
            }
        }

        vMax_ = maxEfficiency;
        return vMax_;
    }

    /**
     * @brief Evaluates heuristic h(fromState, toState) = ||x_from - x_to||_2 / v_max.
     */
    [[nodiscard]] double calculate(core::StateId fromState, core::StateId toState) const {
        if (fromState == toState) return 0.0;

        auto itFrom = embeddings_.find(fromState);
        auto itTo = embeddings_.find(toState);
        if (itFrom == embeddings_.end() || itTo == embeddings_.end()) {
            return 0.0;
        }

        double dist = spatial::euclideanDistance(itFrom->second, itTo->second);
        return dist / vMax_;
    }

    /**
     * @brief Evaluates heuristic between two raw coordinate vectors.
     */
    [[nodiscard]] double calculate(const std::vector<double>& fromCoords, const std::vector<double>& toCoords) const {
        double dist = spatial::euclideanDistance(fromCoords, toCoords);
        return dist / vMax_;
    }

    /**
     * @brief Verifies that triangle inequality h(u, w) <= h(u, v) + h(v, w) holds.
     */
    [[nodiscard]] bool verifyTriangleInequality(core::StateId u, core::StateId v, core::StateId w) const {
        double h_uw = calculate(u, w);
        double h_uv = calculate(u, v);
        double h_vw = calculate(v, w);
        return h_uw <= (h_uv + h_vw + core::EPSILON);
    }

    /**
     * @brief Verifies that admissibility h(u, v) <= edgeCost holds.
     */
    [[nodiscard]] bool verifyAdmissibility(core::StateId u, core::StateId v, double edgeCost) const {
        double h_uv = calculate(u, v);
        return h_uv <= (edgeCost + core::EPSILON);
    }
};

} // namespace ssp::algorithms

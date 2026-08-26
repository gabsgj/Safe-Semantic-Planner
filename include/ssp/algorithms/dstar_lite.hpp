#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <string>
#include <iostream>
#include <optional>

#include "ssp/core/types.hpp"
#include "ssp/core/state.hpp"
#include "ssp/core/transition.hpp"
#include "ssp/core/problem.hpp"
#include "ssp/core/result.hpp"
#include "ssp/core/planner.hpp"
#include "ssp/spatial/kd_tree.hpp"
#include "ssp/spatial/vector_math.hpp"
#include "ssp/config/config_manager.hpp"
#include "ssp/algorithms/indexed_priority_queue.hpp"
#include "ssp/algorithms/heuristic.hpp"

namespace ssp::algorithms {

/**
 * @brief High-performance D* Lite Incremental Path Planner with Continuous
 * Euclidean Hazard Potential Fields.
 * 
 * Implements the ssp::core::Planner interface.
 * Features:
 * - Reverse heuristic search (s_G -> s_I) maintaining g(u) and rhs(u) consistency.
 * - Key modifier (k_m) for O(1) amortized heuristic updates during agent movement.
 * - KD-Tree indexed continuous Euclidean hazard repulsive barrier fields.
 * - Dynamic sub-millisecond replanning on edge breakage, cost changes, goal updates, and obstacle insertion.
 */
class DStarLitePlanner : public core::Planner {
public:
    struct Edge {
        uint64_t id{0};
        core::StateId from{0};
        core::StateId to{0};
        double baseCost{1.0};
        double safety{1.0};
        double reliability{1.0};
        bool available{true};
        std::string name{""};
    };

private:
    config::PlannerConfig config_;
    core::StateId startState_{0};
    core::StateId lastStartState_{0};
    core::StateId goalState_{0};
    double km_{0.0};
    size_t expandedNodes_{0};

    std::unordered_map<core::StateId, core::State> states_;
    std::unordered_map<uint64_t, Edge> edges_;
    std::unordered_map<core::StateId, std::vector<uint64_t>> outgoingEdges_;
    std::unordered_map<core::StateId, std::vector<uint64_t>> incomingEdges_;
    std::unordered_set<core::StateId> badStatesSet_;

    spatial::KDTree kdTree_;
    EuclideanHeuristic heuristic_;
    IndexedPriorityQueue<core::StateId, DStarKey> queue_;
    std::unordered_map<core::StateId, double> g_;
    std::unordered_map<core::StateId, double> rhs_;

    const std::vector<uint64_t> emptyEdgesList_{};

    [[nodiscard]] double getG(core::StateId u) const {
        auto it = g_.find(u);
        return (it != g_.end()) ? it->second : core::INF_COST;
    }

    [[nodiscard]] double getRHS(core::StateId u) const {
        auto it = rhs_.find(u);
        return (it != rhs_.end()) ? it->second : core::INF_COST;
    }

    void setG(core::StateId u, double val) {
        g_[u] = val;
    }

    void setRHS(core::StateId u, double val) {
        rhs_[u] = val;
    }

    [[nodiscard]] const std::vector<uint64_t>& getOutgoingEdges(core::StateId u) const {
        auto it = outgoingEdges_.find(u);
        return (it != outgoingEdges_.end()) ? it->second : emptyEdgesList_;
    }

    [[nodiscard]] const std::vector<uint64_t>& getIncomingEdges(core::StateId u) const {
        auto it = incomingEdges_.find(u);
        return (it != incomingEdges_.end()) ? it->second : emptyEdgesList_;
    }

    void rebuildKDTree() {
        std::vector<spatial::KDPoint> badPoints;
        badPoints.reserve(badStatesSet_.size());
        for (core::StateId bId : badStatesSet_) {
            auto it = states_.find(bId);
            if (it != states_.end()) {
                badPoints.emplace_back(it->first, it->second.embedding);
            }
        }
        kdTree_.build(std::move(badPoints));
    }

public:
    explicit DStarLitePlanner(config::PlannerConfig config = config::PlannerConfig{})
        : config_(std::move(config)), heuristic_(config_.max_velocity_heuristic) {}

    [[nodiscard]] const config::PlannerConfig& getConfig() const noexcept { return config_; }
    void setConfig(const config::PlannerConfig& cfg) {
        config_ = cfg;
        heuristic_.setVMax(config_.max_velocity_heuristic);
    }

    [[nodiscard]] bool isBadState(core::StateId u) const noexcept {
        return badStatesSet_.find(u) != badStatesSet_.end();
    }

    [[nodiscard]] const core::State* findState(core::StateId u) const {
        auto it = states_.find(u);
        return (it != states_.end()) ? &(it->second) : nullptr;
    }

    [[nodiscard]] const Edge* findEdge(uint64_t edgeId) const {
        auto it = edges_.find(edgeId);
        return (it != edges_.end()) ? &(it->second) : nullptr;
    }

    /**
     * @brief Computes safety-augmented edge cost c'(u, v).
     * Incorporates soft exponential hazard potential field, hard barrier, and reliability penalty.
     */
    [[nodiscard]] double computeEdgeCost(const Edge& e) const {
        if (!e.available) return core::INF_COST;
        if (isBadState(e.from) || isBadState(e.to)) return core::INF_COST;

        const auto* toState = findState(e.to);
        if (!toState) return core::INF_COST;

        double hazardCost = 0.0;
        if (!badStatesSet_.empty() && !kdTree_.empty()) {
            auto nn = kdTree_.nearestNeighbor(toState->embedding);
            double dist = nn.distance;
            if (dist <= config_.hazard_critical_radius) {
                return core::INF_COST; // Hard bad-state critical collision
            } else if (dist <= config_.safety_clearance_margin) {
                double decay = (dist - config_.hazard_critical_radius) /
                               (config_.hazard_barrier_decay_sigma > 0.0 ? config_.hazard_barrier_decay_sigma : 1.0);
                hazardCost = config_.gamma_safety * std::exp(-decay);
            }
        }

        double rel = std::max(e.reliability, core::EPSILON);
        double reliabilityPenalty = config_.delta_reliability * (-std::log(rel));

        double intrinsicSafetyPenalty = 0.0;
        if (e.safety < 1.0) {
            intrinsicSafetyPenalty = config_.gamma_safety * (1.0 - std::max(0.0, e.safety));
        }

        return (config_.beta_cost * e.baseCost) + hazardCost + reliabilityPenalty + intrinsicSafetyPenalty;
    }

    /**
     * @brief Computes priority key k(u) = [min(g, rhs) + h(s_start, u) + km, min(g, rhs)].
     */
    [[nodiscard]] DStarKey calculateKey(core::StateId u) const {
        double min_g_rhs = std::min(getG(u), getRHS(u));
        if (min_g_rhs >= core::INF_COST) {
            return DStarKey::infinity();
        }
        double h = heuristic_.calculate(startState_, u);
        return DStarKey(min_g_rhs + h + km_, min_g_rhs);
    }

    /**
     * @brief D* Lite UpdateVertex procedure maintaining rhs and queue consistency.
     */
    void updateVertex(core::StateId u) {
        if (u != goalState_) {
            double minRhs = core::INF_COST;
            for (uint64_t outEdgeId : getOutgoingEdges(u)) {
                auto itEdge = edges_.find(outEdgeId);
                if (itEdge == edges_.end()) continue;
                const auto& edge = itEdge->second;

                double cost = computeEdgeCost(edge);
                if (cost < core::INF_COST) {
                    double g_succ = getG(edge.to);
                    if (g_succ < core::INF_COST) {
                        minRhs = std::min(minRhs, cost + g_succ);
                    }
                }
            }
            setRHS(u, minRhs);
        }

        if (queue_.contains(u)) {
            queue_.remove(u);
        }

        if (std::abs(getG(u) - getRHS(u)) > core::EPSILON) {
            queue_.insert(u, calculateKey(u));
        }
    }

    /**
     * @brief Core D* Lite search loop expanding locally inconsistent states.
     */
    bool computeShortestPath() {
        size_t expansions = 0;
        while (!queue_.empty()) {
            DStarKey topK = queue_.topKey();
            DStarKey startK = calculateKey(startState_);
            double g_start = getG(startState_);
            double rhs_start = getRHS(startState_);

            if (topK >= startK && std::abs(g_start - rhs_start) <= core::EPSILON) {
                break;
            }

            if (expansions++ >= config_.max_expansions) {
                return false;
            }
            expandedNodes_++;

            auto [u, k_old] = queue_.pop();
            DStarKey k_new = calculateKey(u);

            if (k_old < k_new) {
                queue_.insert(u, k_new);
            } else if (getG(u) > getRHS(u) + core::EPSILON) {
                // Locally overconsistent: g(u) > rhs(u)
                setG(u, getRHS(u));
                for (uint64_t inEdgeId : getIncomingEdges(u)) {
                    auto itEdge = edges_.find(inEdgeId);
                    if (itEdge != edges_.end()) {
                        updateVertex(itEdge->second.from);
                    }
                }
            } else {
                // Locally underconsistent: g(u) <= rhs(u)
                setG(u, core::INF_COST);
                updateVertex(u);
                for (uint64_t inEdgeId : getIncomingEdges(u)) {
                    auto itEdge = edges_.find(inEdgeId);
                    if (itEdge != edges_.end()) {
                        updateVertex(itEdge->second.from);
                    }
                }
            }
        }
        return true;
    }

    /**
     * @brief Initializes graph structures and reverse search tree rooted at s_G.
     */
    void initialize(const core::PlanningProblem& problem) {
        startState_ = problem.initialState;
        lastStartState_ = problem.initialState;
        goalState_ = problem.goalState;
        km_ = 0.0;
        expandedNodes_ = 0;

        states_.clear();
        edges_.clear();
        outgoingEdges_.clear();
        incomingEdges_.clear();
        badStatesSet_.clear();
        queue_.clear();
        g_.clear();
        rhs_.clear();

        for (const auto& s : problem.states) {
            states_[s.id] = s;
        }

        for (core::StateId bId : problem.badStates) {
            badStatesSet_.insert(bId);
        }
        rebuildKDTree();

        for (const auto& t : problem.transitions) {
            edges_[t.id] = {t.id, t.from, t.to, t.cost, t.safety, t.reliability, t.available, t.name};
            outgoingEdges_[t.from].push_back(t.id);
            incomingEdges_[t.to].push_back(t.id);
        }

        heuristic_.computeAndSetVMax(problem.states, problem.transitions, config_.max_velocity_heuristic);

        setRHS(goalState_, 0.0);
        queue_.insert(goalState_, calculateKey(goalState_));
    }

    /**
     * @brief Extracts optimal path from startState_ to goalState_.
     */
    [[nodiscard]] core::PlanningResult extractPath() const {
        core::PlanningResult res;
        res.exploredStatesCount = expandedNodes_;

        if (isBadState(startState_) || isBadState(goalState_)) {
            res.success = false;
            res.message = "Start or Goal state is marked as bad state hazard";
            return res;
        }

        if (getG(startState_) >= core::INF_COST && getRHS(startState_) >= core::INF_COST) {
            res.success = false;
            res.message = "Goal state unreachable from start";
            return res;
        }

        core::StateId curr = startState_;
        res.statePath.push_back(curr);

        double totalBaseCost = 0.0;
        double cumReliability = 1.0;
        double minSafetyDist = core::INF_COST;

        // Check start state safety distance
        auto itStart = states_.find(curr);
        if (itStart != states_.end() && !badStatesSet_.empty() && !kdTree_.empty()) {
            minSafetyDist = std::min(minSafetyDist, kdTree_.nearestNeighbor(itStart->second.embedding).distance);
        }

        std::unordered_set<core::StateId> visited;
        visited.insert(curr);

        size_t maxSteps = states_.size() + 2;
        size_t step = 0;

        while (curr != goalState_ && step++ < maxSteps) {
            double bestCost = core::INF_COST;
            uint64_t bestEdgeId = 0;
            core::StateId nextState = curr;

            for (uint64_t outEdgeId : getOutgoingEdges(curr)) {
                auto itEdge = edges_.find(outEdgeId);
                if (itEdge == edges_.end()) continue;
                const auto& edge = itEdge->second;

                double c = computeEdgeCost(edge);
                if (c < core::INF_COST) {
                    double g_succ = getG(edge.to);
                    if (g_succ < core::INF_COST) {
                        double totalEdgePathCost = c + g_succ;
                        if (totalEdgePathCost < bestCost) {
                            bestCost = totalEdgePathCost;
                            bestEdgeId = edge.id;
                            nextState = edge.to;
                        }
                    }
                }
            }

            if (bestEdgeId == 0 || nextState == curr || visited.find(nextState) != visited.end()) {
                // Dead end or loop
                res.success = false;
                res.message = "Failed to construct loop-free trajectory to goal";
                return res;
            }

            const auto& selectedEdge = edges_.at(bestEdgeId);
            res.transitionPath.push_back(selectedEdge.id);
            res.statePath.push_back(nextState);
            visited.insert(nextState);

            totalBaseCost += selectedEdge.baseCost;
            cumReliability *= selectedEdge.reliability;

            auto itNext = states_.find(nextState);
            if (itNext != states_.end() && !badStatesSet_.empty() && !kdTree_.empty()) {
                double d = kdTree_.nearestNeighbor(itNext->second.embedding).distance;
                minSafetyDist = std::min(minSafetyDist, d);
            }

            curr = nextState;
        }

        if (curr != goalState_) {
            res.success = false;
            res.message = "Exceeded maximum trajectory length without reaching goal";
            return res;
        }

        res.success = true;
        res.totalCost = totalBaseCost;
        res.minimumSafetyDistance = (minSafetyDist < core::INF_COST) ? minSafetyDist : 100.0;
        res.cumulativeReliability = cumReliability;

        // Optimization objective score: Score = alpha*G - beta*C + gamma*D + delta*R
        res.safetyScore = (config_.alpha_goal * 1.0)
                        - (config_.beta_cost * res.totalCost)
                        + (config_.gamma_safety * res.minimumSafetyDistance)
                        + (config_.delta_reliability * res.cumulativeReliability);
        res.message = "Optimal trajectory generated successfully";
        return res;
    }

    /**
     * @brief Computes from-scratch plan for a given problem.
     */
    core::PlanningResult plan(const core::PlanningProblem& problem) override {
        auto startTime = std::chrono::high_resolution_clock::now();
        initialize(problem);
        computeShortestPath();
        auto result = extractPath();
        auto endTime = std::chrono::high_resolution_clock::now();
        result.planningTimeMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        result.wasReplanned = false;
        return result;
    }

    // =========================================================================
    // DYNAMIC REPLANNING API
    // =========================================================================

    /**
     * @brief Dynamically updates transition cost and triggers incremental replan.
     */
    core::PlanningResult updateEdgeCost(uint64_t transitionId, double newCost) {
        auto startTime = std::chrono::high_resolution_clock::now();
        auto it = edges_.find(transitionId);
        if (it != edges_.end()) {
            it->second.baseCost = newCost;
            updateVertex(it->second.from);
        }
        computeShortestPath();
        auto res = extractPath();
        auto endTime = std::chrono::high_resolution_clock::now();
        res.planningTimeMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        res.wasReplanned = true;
        return res;
    }

    /**
     * @brief Dynamically sets edge availability (e.g. edge failure or restoration).
     */
    core::PlanningResult setEdgeAvailability(uint64_t transitionId, bool available) {
        auto startTime = std::chrono::high_resolution_clock::now();
        auto it = edges_.find(transitionId);
        if (it != edges_.end()) {
            it->second.available = available;
            updateVertex(it->second.from);
        }
        computeShortestPath();
        auto res = extractPath();
        auto endTime = std::chrono::high_resolution_clock::now();
        res.planningTimeMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        res.wasReplanned = true;
        return res;
    }

    /**
     * @brief Dynamically adds a new transition to the graph.
     */
    core::PlanningResult addTransition(const core::Transition& t) {
        auto startTime = std::chrono::high_resolution_clock::now();
        edges_[t.id] = {t.id, t.from, t.to, t.cost, t.safety, t.reliability, t.available, t.name};
        
        auto& outList = outgoingEdges_[t.from];
        if (std::find(outList.begin(), outList.end(), t.id) == outList.end()) {
            outList.push_back(t.id);
        }
        auto& inList = incomingEdges_[t.to];
        if (std::find(inList.begin(), inList.end(), t.id) == inList.end()) {
            inList.push_back(t.id);
        }
        updateVertex(t.from);

        computeShortestPath();
        auto res = extractPath();
        auto endTime = std::chrono::high_resolution_clock::now();
        res.planningTimeMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        res.wasReplanned = true;
        return res;
    }

    /**
     * @brief Dynamically marks a state as a bad-state hazard.
     */
    core::PlanningResult addBadState(core::StateId badStateId) {
        auto startTime = std::chrono::high_resolution_clock::now();
        badStatesSet_.insert(badStateId);
        rebuildKDTree();

        for (const auto& [sId, _] : states_) {
            updateVertex(sId);
        }

        computeShortestPath();
        auto res = extractPath();
        auto endTime = std::chrono::high_resolution_clock::now();
        res.planningTimeMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        res.wasReplanned = true;
        return res;
    }

    /**
     * @brief Dynamically removes a state from bad-state hazard set.
     */
    core::PlanningResult removeBadState(core::StateId badStateId) {
        auto startTime = std::chrono::high_resolution_clock::now();
        badStatesSet_.erase(badStateId);
        rebuildKDTree();

        for (const auto& [sId, _] : states_) {
            updateVertex(sId);
        }

        computeShortestPath();
        auto res = extractPath();
        auto endTime = std::chrono::high_resolution_clock::now();
        res.planningTimeMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        res.wasReplanned = true;
        return res;
    }

    /**
     * @brief Updates goal state to a new location.
     */
    core::PlanningResult updateGoal(core::StateId newGoalId) {
        auto startTime = std::chrono::high_resolution_clock::now();
        goalState_ = newGoalId;
        km_ = 0.0;
        queue_.clear();
        g_.clear();
        rhs_.clear();

        setRHS(goalState_, 0.0);
        queue_.insert(goalState_, calculateKey(goalState_));

        computeShortestPath();
        auto res = extractPath();
        auto endTime = std::chrono::high_resolution_clock::now();
        res.planningTimeMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        res.wasReplanned = true;
        return res;
    }

    /**
     * @brief Incremental replanning when agent moves or environment mutates.
     */
    core::PlanningResult replan(std::optional<core::StateId> currentStartState = std::nullopt) {
        auto startTime = std::chrono::high_resolution_clock::now();
        if (currentStartState.has_value() && *currentStartState != startState_) {
            km_ += heuristic_.calculate(lastStartState_, *currentStartState);
            startState_ = *currentStartState;
            lastStartState_ = *currentStartState;
        }

        computeShortestPath();
        auto res = extractPath();
        auto endTime = std::chrono::high_resolution_clock::now();
        res.planningTimeMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        res.wasReplanned = true;
        return res;
    }
};

using DStarLite = DStarLitePlanner;

} // namespace ssp::algorithms

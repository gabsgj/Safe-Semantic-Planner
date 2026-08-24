#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <iostream>
#include <iomanip>

#include "ssp/agent/agent_state.hpp"
#include "ssp/core/problem.hpp"
#include "ssp/core/result.hpp"
#include "ssp/algorithms/dstar_lite.hpp"
#include "ssp/config/config_manager.hpp"

namespace ssp::agent {

struct StepDecision {
    uint64_t fromStateId{0};
    uint64_t toStateId{0};
    uint64_t actionId{0};
    std::string actionName;
    bool wasBacktrack{false};
    double decisionTimeUs{0.0};
    double tokenCost{0.0};
    std::string rationale;
};

class NeuroSymbolicGovernor {
public:
    explicit NeuroSymbolicGovernor(config::PlannerConfig config = config::PlannerConfig{})
        : config_(std::move(config)), planner_(std::make_unique<algorithms::DStarLitePlanner>(config_)) {}

    void initialize(
        const std::vector<AgentState>& states,
        const std::vector<std::pair<core::Transition, AgentAction>>& transitions,
        uint64_t initialStateId,
        uint64_t goalStateId
    ) {
        states_ = states;
        problem_.domainName = "SWE-bench Neuro-Symbolic Agent Workspace";
        problem_.initialState = initialStateId;
        problem_.goalState = goalStateId;
        problem_.badStates.clear();
        problem_.states.clear();
        problem_.transitions.clear();

        for (const auto& s : states) {
            core::State coreS;
            coreS.id = s.id;
            coreS.embedding = s.healthVector;
            coreS.name = s.stageDescription + " [" + s.commitHash.substr(0, 7) + "]";
            problem_.states.push_back(coreS);

            if (s.isRegressionState) {
                problem_.badStates.push_back(s.id);
            }
        }

        for (const auto& [coreT, agentAct] : transitions) {
            problem_.transitions.push_back(coreT);
            actionMap_[coreT.id] = agentAct;
        }

        currentStateId_ = initialStateId;
        planner_->plan(problem_);
    }

    /**
     * @brief Execute full governed trajectory from current state to the clean bug-fix goal.
     */
    [[nodiscard]] std::vector<StepDecision> runToGoal() {
        std::vector<StepDecision> history;
        int maxSteps = 50;
        int step = 0;

        while (currentStateId_ != problem_.goalState && step < maxSteps) {
            step++;
            auto startTs = std::chrono::high_resolution_clock::now();

            auto planRes = planner_->plan(problem_);
            if (!planRes.success || planRes.statePath.size() < 2) {
                // Dead end reached -> Force backtrack to parent of previous state
                break;
            }

            uint64_t nextStateId = planRes.statePath[1];
            uint64_t nextTransId = planRes.transitionPath[0];
            const auto& act = actionMap_[nextTransId];

            auto endTs = std::chrono::high_resolution_clock::now();
            double decisionUs = std::chrono::duration<double, std::micro>(endTs - startTs).count();

            StepDecision decision;
            decision.fromStateId = currentStateId_;
            decision.toStateId = nextStateId;
            decision.actionId = nextTransId;
            decision.actionName = act.description;
            decision.tokenCost = act.tokenCost;
            decision.decisionTimeUs = decisionUs;

            // Check if the next state is a regression or broken build
            const auto& nextState = getState(nextStateId);
            if (nextState.isRegressionState || act.introducesRegression) {
                // Dynamic Quarantine: Quarantine state as a bad state barrier and trigger instant D* Lite replan
                problem_.badStates.push_back(nextStateId);
                planner_->addBadState(nextStateId);

                decision.wasBacktrack = true;
                decision.rationale = "Regression detected in commit " + nextState.commitHash.substr(0, 7) + 
                    " (" + std::to_string(nextState.regressionCount) + " failing tests). Quarantined snapshot #" + 
                    std::to_string(nextStateId) + " and automatically backtracked to optimal search branch in " + 
                    std::to_string(decisionUs) + " µs.";
                history.push_back(decision);

                backtrackCount_++;
                totalTokensSpent_ += act.tokenCost;
                continue;
            }

            decision.wasBacktrack = false;
            decision.rationale = "Transitioned safely to " + nextState.stageDescription + 
                " [" + nextState.commitHash.substr(0, 7) + "] with " + 
                std::to_string(nextState.passingTests) + "/" + std::to_string(nextState.totalTests) + " tests passing.";
            history.push_back(decision);

            currentStateId_ = nextStateId;
            problem_.initialState = currentStateId_;
            totalTokensSpent_ += act.tokenCost;
        }

        return history;
    }

    [[nodiscard]] size_t getBacktrackCount() const { return backtrackCount_; }
    [[nodiscard]] double getTotalTokensSpent() const { return totalTokensSpent_; }
    [[nodiscard]] uint64_t getCurrentStateId() const { return currentStateId_; }
    [[nodiscard]] const core::PlanningProblem& getProblem() const { return problem_; }

private:
    config::PlannerConfig config_;
    std::unique_ptr<algorithms::DStarLitePlanner> planner_;
    core::PlanningProblem problem_;
    std::vector<AgentState> states_;
    std::unordered_map<uint64_t, AgentAction> actionMap_;
    uint64_t currentStateId_{0};
    size_t backtrackCount_{0};
    double totalTokensSpent_{0.0};

    [[nodiscard]] const AgentState& getState(uint64_t id) const {
        for (const auto& s : states_) {
            if (s.id == id) return s;
        }
        return states_[0];
    }
};

} // namespace ssp::agent

#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include "third_party/nlohmann/json.hpp"

namespace ssp::agent {

enum class ActionType {
    ANALYZE_ISSUE,
    LOCATE_FAULT,
    APPLY_PATCH,
    RUN_COMPILER,
    RUN_TEST_SUITE,
    LINT_FIX,
    REVERT_COMMIT,
    REFACTOR_MODULE,
    VERIFY_REGRESSION
};

inline std::string actionTypeToString(ActionType type) {
    switch (type) {
        case ActionType::ANALYZE_ISSUE: return "ANALYZE_ISSUE";
        case ActionType::LOCATE_FAULT: return "LOCATE_FAULT";
        case ActionType::APPLY_PATCH: return "APPLY_PATCH";
        case ActionType::RUN_COMPILER: return "RUN_COMPILER";
        case ActionType::RUN_TEST_SUITE: return "RUN_TEST_SUITE";
        case ActionType::LINT_FIX: return "LINT_FIX";
        case ActionType::REVERT_COMMIT: return "REVERT_COMMIT";
        case ActionType::REFACTOR_MODULE: return "REFACTOR_MODULE";
        case ActionType::VERIFY_REGRESSION: return "VERIFY_REGRESSION";
        default: return "UNKNOWN_ACTION";
    }
}

/**
 * @brief LLM Tool Action / State Transition
 */
struct AgentAction {
    uint64_t id{0};
    ActionType type{ActionType::ANALYZE_ISSUE};
    std::string description;
    double tokenCost{100.0};       // Token cost ($ / tokens)
    double executionLatencyMs{50.0};
    double confidence{0.95};       // Model confidence / reliability
    bool introducesRegression{false};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        AgentAction,
        id,
        type,
        description,
        tokenCost,
        executionLatencyMs,
        confidence,
        introducesRegression
    )
};

/**
 * @brief Repository / Workspace Snapshot State
 */
struct AgentState {
    uint64_t id{0};
    std::string commitHash;
    std::string stageDescription;
    
    // 5D Codebase Health Vector:
    // [0] Passing Tests Ratio (0.0 to 1.0)
    // [1] Compiler / Syntax Errors Count
    // [2] AST Similarity to Clean Target (0.0 to 1.0)
    // [3] Cumulative Context Tokens Burned (Normalized)
    // [4] Regression Count
    std::vector<double> healthVector{0.0, 0.0, 0.0, 0.0, 0.0};

    int passingTests{0};
    int totalTests{10};
    int compilerErrors{0};
    int regressionCount{0};
    double cumulativeTokensSpent{0.0};
    bool isRegressionState{false};
    bool isResolvedGoal{false};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        AgentState,
        id,
        commitHash,
        stageDescription,
        healthVector,
        passingTests,
        totalTests,
        compilerErrors,
        regressionCount,
        cumulativeTokensSpent,
        isRegressionState,
        isResolvedGoal
    )
};

} // namespace ssp::agent

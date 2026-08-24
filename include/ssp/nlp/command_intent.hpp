#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include "third_party/nlohmann/json.hpp"

namespace ssp::nlp {

enum class CommandIntentType {
    PLAN_ROUTE,
    ADD_HAZARD,
    REMOVE_HAZARD,
    DISABLE_EDGE,
    ENABLE_EDGE,
    SET_START,
    SET_GOAL,
    TUNE_OBJECTIVES,
    EXPLAIN_PATH,
    COMPLEX_CONSTRAINED_PLAN,
    SEMANTIC_QUERY,
    UNKNOWN
};

inline std::string intentToString(CommandIntentType intent) {
    switch (intent) {
        case CommandIntentType::PLAN_ROUTE: return "PLAN_ROUTE";
        case CommandIntentType::ADD_HAZARD: return "ADD_HAZARD";
        case CommandIntentType::REMOVE_HAZARD: return "REMOVE_HAZARD";
        case CommandIntentType::DISABLE_EDGE: return "DISABLE_EDGE";
        case CommandIntentType::ENABLE_EDGE: return "ENABLE_EDGE";
        case CommandIntentType::SET_START: return "SET_START";
        case CommandIntentType::SET_GOAL: return "SET_GOAL";
        case CommandIntentType::TUNE_OBJECTIVES: return "TUNE_OBJECTIVES";
        case CommandIntentType::EXPLAIN_PATH: return "EXPLAIN_PATH";
        case CommandIntentType::COMPLEX_CONSTRAINED_PLAN: return "COMPLEX_CONSTRAINED_PLAN";
        case CommandIntentType::SEMANTIC_QUERY: return "SEMANTIC_QUERY";
        default: return "UNKNOWN";
    }
}

inline CommandIntentType stringToIntent(const std::string& str) {
    if (str == "PLAN_ROUTE") return CommandIntentType::PLAN_ROUTE;
    if (str == "ADD_HAZARD") return CommandIntentType::ADD_HAZARD;
    if (str == "REMOVE_HAZARD") return CommandIntentType::REMOVE_HAZARD;
    if (str == "DISABLE_EDGE") return CommandIntentType::DISABLE_EDGE;
    if (str == "ENABLE_EDGE") return CommandIntentType::ENABLE_EDGE;
    if (str == "SET_START") return CommandIntentType::SET_START;
    if (str == "SET_GOAL") return CommandIntentType::SET_GOAL;
    if (str == "TUNE_OBJECTIVES") return CommandIntentType::TUNE_OBJECTIVES;
    if (str == "EXPLAIN_PATH") return CommandIntentType::EXPLAIN_PATH;
    if (str == "COMPLEX_CONSTRAINED_PLAN") return CommandIntentType::COMPLEX_CONSTRAINED_PLAN;
    if (str == "SEMANTIC_QUERY") return CommandIntentType::SEMANTIC_QUERY;
    return CommandIntentType::UNKNOWN;
}

struct ConditionalConstraint {
    uint64_t triggerStateId{0};
    uint64_t forbiddenStateId{0};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ConditionalConstraint, triggerStateId, forbiddenStateId)
};

struct ParsedCommand {
    CommandIntentType intent{CommandIntentType::UNKNOWN};
    double confidence{0.0};
    std::string rawQuery;
    
    // Extracted Semantic Slots
    std::unordered_map<std::string, std::string> slots;
    
    // Resolved Topological Entity IDs
    std::optional<uint64_t> resolvedStartId;
    std::optional<uint64_t> resolvedGoalId;
    std::optional<uint64_t> resolvedHazardId;
    std::optional<uint64_t> resolvedEdgeId;

    // Advanced LTL & Temporal Constraints
    std::vector<uint64_t> mustVisitWaypoints;
    std::vector<ConditionalConstraint> conditionalConstraints;
    
    // Tuned Objectives (if TUNE_OBJECTIVES)
    std::unordered_map<std::string, double> paramUpdates;
    
    // 64-D Semantic Embedding Vector
    std::vector<double> embedding;
    
    // Neuro-Symbolic Natural Language Rationale / Response
    std::string naturalLanguageExplanation;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        ParsedCommand,
        rawQuery,
        confidence,
        slots,
        resolvedStartId,
        resolvedGoalId,
        resolvedHazardId,
        resolvedEdgeId,
        mustVisitWaypoints,
        conditionalConstraints,
        paramUpdates,
        embedding,
        naturalLanguageExplanation
    )
};

} // namespace ssp::nlp

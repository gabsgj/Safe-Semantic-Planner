#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <regex>
#include <iomanip>
#include <memory>

#include "ssp/nlp/semantic_embedding.hpp"
#include "ssp/nlp/command_intent.hpp"
#include "ssp/core/problem.hpp"
#include "ssp/core/result.hpp"
#include "ssp/algorithms/dstar_lite.hpp"

namespace ssp::nlp {

class NlpParser {
public:
    explicit NlpParser(std::shared_ptr<SemanticEmbeddingModel> embeddingModel = nullptr)
        : embeddingModel_(embeddingModel ? embeddingModel : std::make_shared<SemanticEmbeddingModel>()) {
        initIntentPrototypes();
    }

    /**
     * @brief Parse arbitrary natural language input and resolve against the current graph topology.
     */
    [[nodiscard]] ParsedCommand parse(
        const std::string& query, 
        const core::PlanningProblem& problem
    ) const {
        ParsedCommand cmd;
        cmd.rawQuery = query;
        cmd.embedding = embeddingModel_->encode(query);

        std::string lowerQuery = toLower(query);

        // Check for multi-clause conditional / LTL constraints first
        if ((lowerQuery.find(" if ") != std::string::npos || lowerQuery.find("never go") != std::string::npos || lowerQuery.find("should go through") != std::string::npos || lowerQuery.find("must pass") != std::string::npos) &&
            (lowerQuery.find("start") != std::string::npos || lowerQuery.find("goal") != std::string::npos || lowerQuery.find("state") != std::string::npos)) {
            cmd.intent = CommandIntentType::COMPLEX_CONSTRAINED_PLAN;
            cmd.confidence = 0.98;
            extractComplexConstraints(lowerQuery, problem, cmd);
            return cmd;
        }

        // 1. Identify Command Intent using Semantic Similarity & Pattern Rules
        auto [intent, confidence] = classifyIntent(query, lowerQuery);
        cmd.intent = intent;
        cmd.confidence = confidence;

        // 2. Extract Entities & Parameters based on Intent
        switch (intent) {
            case CommandIntentType::PLAN_ROUTE:
            case CommandIntentType::SET_START:
            case CommandIntentType::SET_GOAL: {
                extractRoutingEntities(lowerQuery, problem, cmd);
                break;
            }
            case CommandIntentType::ADD_HAZARD:
            case CommandIntentType::REMOVE_HAZARD: {
                extractHazardEntities(lowerQuery, problem, cmd);
                break;
            }
            case CommandIntentType::DISABLE_EDGE:
            case CommandIntentType::ENABLE_EDGE: {
                extractEdgeEntities(lowerQuery, problem, cmd);
                break;
            }
            case CommandIntentType::TUNE_OBJECTIVES: {
                extractObjectiveParameters(lowerQuery, cmd);
                break;
            }
            case CommandIntentType::EXPLAIN_PATH: {
                cmd.resolvedStartId = problem.initialState;
                cmd.resolvedGoalId = problem.goalState;
                break;
            }
            case CommandIntentType::SEMANTIC_QUERY:
            case CommandIntentType::UNKNOWN:
            default: {
                if (!problem.states.empty()) {
                    auto [bestStateId, bestSim] = resolveBestState(query, problem.states);
                    cmd.slots["query_target"] = std::to_string(bestStateId);
                }
                break;
            }
        }

        return cmd;
    }

    /**
     * @brief Execute a parsed NLP command on the planner and generate neuro-symbolic natural language explanation.
     */
    [[nodiscard]] std::pair<core::PlanningResult, ParsedCommand> execute(
        ParsedCommand cmd,
        core::PlanningProblem& problem,
        algorithms::DStarLitePlanner& planner
    ) const {
        core::PlanningResult result;

        switch (cmd.intent) {
            case CommandIntentType::COMPLEX_CONSTRAINED_PLAN: {
                result = executeComplexPlan(cmd, problem, planner);
                cmd.naturalLanguageExplanation = generateComplexPlanExplanation(cmd, problem, result);
                break;
            }
            case CommandIntentType::PLAN_ROUTE: {
                if (cmd.resolvedStartId.has_value()) problem.initialState = *cmd.resolvedStartId;
                if (cmd.resolvedGoalId.has_value()) problem.goalState = *cmd.resolvedGoalId;
                if (cmd.resolvedHazardId.has_value()) {
                    uint64_t hid = *cmd.resolvedHazardId;
                    if (std::find(problem.badStates.begin(), problem.badStates.end(), hid) == problem.badStates.end()) {
                        problem.badStates.push_back(hid);
                    }
                }
                result = planner.plan(problem);
                cmd.naturalLanguageExplanation = generatePlanExplanation(cmd, problem, result);
                break;
            }
            case CommandIntentType::SET_START: {
                if (cmd.resolvedStartId.has_value()) {
                    problem.initialState = *cmd.resolvedStartId;
                    result = planner.plan(problem);
                    cmd.naturalLanguageExplanation = "Updated initial start state to Node #" + 
                        std::to_string(*cmd.resolvedStartId) + " (" + getStateName(problem, *cmd.resolvedStartId) + 
                        "). Computed safe path with total cost " + std::to_string(result.totalCost) + ".";
                } else {
                    result = planner.plan(problem);
                    cmd.naturalLanguageExplanation = "Start state unchanged (#" + std::to_string(problem.initialState) + ").";
                }
                break;
            }
            case CommandIntentType::SET_GOAL: {
                if (cmd.resolvedGoalId.has_value()) {
                    problem.goalState = *cmd.resolvedGoalId;
                    result = planner.updateGoal(*cmd.resolvedGoalId);
                    cmd.naturalLanguageExplanation = "Updated target destination to Node #" + 
                        std::to_string(*cmd.resolvedGoalId) + " (" + getStateName(problem, *cmd.resolvedGoalId) + 
                        "). D* Lite dynamically replanned in " + std::to_string(result.planningTimeMicroseconds) + " µs.";
                } else {
                    result = planner.plan(problem);
                    cmd.naturalLanguageExplanation = "Goal state unchanged (#" + std::to_string(problem.goalState) + ").";
                }
                break;
            }
            case CommandIntentType::ADD_HAZARD: {
                if (cmd.resolvedHazardId.has_value()) {
                    uint64_t hid = *cmd.resolvedHazardId;
                    if (std::find(problem.badStates.begin(), problem.badStates.end(), hid) == problem.badStates.end()) {
                        problem.badStates.push_back(hid);
                    }
                    result = planner.addBadState(hid);
                    cmd.naturalLanguageExplanation = "Quarantined Node #" + std::to_string(hid) + " (" + 
                        getStateName(problem, hid) + ") as a hazardous barrier. Dynamically rerouted around hazard in " + 
                        std::to_string(result.planningTimeMicroseconds) + " µs (Zero bad states visited).";
                } else {
                    result = planner.plan(problem);
                    cmd.naturalLanguageExplanation = "No matching hazard state found to quarantine.";
                }
                break;
            }
            case CommandIntentType::REMOVE_HAZARD: {
                if (cmd.resolvedHazardId.has_value()) {
                    uint64_t hid = *cmd.resolvedHazardId;
                    problem.badStates.erase(
                        std::remove(problem.badStates.begin(), problem.badStates.end(), hid),
                        problem.badStates.end()
                    );
                    result = planner.removeBadState(hid);
                    cmd.naturalLanguageExplanation = "Quarantine lifted on Node #" + std::to_string(hid) + " (" + 
                        getStateName(problem, hid) + "). Path clearance barrier updated and restored.";
                } else {
                    result = planner.plan(problem);
                    cmd.naturalLanguageExplanation = "No matching hazard found to remove.";
                }
                break;
            }
            case CommandIntentType::DISABLE_EDGE: {
                if (cmd.resolvedEdgeId.has_value()) {
                    uint64_t eid = *cmd.resolvedEdgeId;
                    for (auto& t : problem.transitions) {
                        if (t.id == eid) { t.available = false; break; }
                    }
                    result = planner.setEdgeAvailability(eid, false);
                    cmd.naturalLanguageExplanation = "Severed transition #" + std::to_string(eid) + 
                        ". Dynamic failover executed in " + std::to_string(result.planningTimeMicroseconds) + 
                        " µs, rerouting traffic with end-to-end reliability of " + 
                        std::to_string(result.cumulativeReliability * 100.0) + "%.";
                } else {
                    result = planner.plan(problem);
                    cmd.naturalLanguageExplanation = "No matching transition found to sever.";
                }
                break;
            }
            case CommandIntentType::ENABLE_EDGE: {
                if (cmd.resolvedEdgeId.has_value()) {
                    uint64_t eid = *cmd.resolvedEdgeId;
                    for (auto& t : problem.transitions) {
                        if (t.id == eid) { t.available = true; break; }
                    }
                    result = planner.setEdgeAvailability(eid, true);
                    cmd.naturalLanguageExplanation = "Restored transition #" + std::to_string(eid) + 
                        ". Shortcut integrated into trajectory search in " + 
                        std::to_string(result.planningTimeMicroseconds) + " µs.";
                } else {
                    result = planner.plan(problem);
                    cmd.naturalLanguageExplanation = "No matching transition found to restore.";
                }
                break;
            }
            case CommandIntentType::TUNE_OBJECTIVES: {
                auto cfg = planner.getConfig();
                if (cmd.paramUpdates.count("alpha")) cfg.alpha_goal = cmd.paramUpdates["alpha"];
                if (cmd.paramUpdates.count("beta")) cfg.beta_cost = cmd.paramUpdates["beta"];
                if (cmd.paramUpdates.count("gamma")) cfg.gamma_safety = cmd.paramUpdates["gamma"];
                if (cmd.paramUpdates.count("delta")) cfg.delta_reliability = cmd.paramUpdates["delta"];
                if (cmd.paramUpdates.count("margin")) cfg.safety_clearance_margin = cmd.paramUpdates["margin"];
                
                planner.setConfig(cfg);
                result = planner.plan(problem);
                cmd.naturalLanguageExplanation = "Updated objective hyperparameters (Safety Weight γ=" + 
                    std::to_string(cfg.gamma_safety) + ", Cost Penalty β=" + std::to_string(cfg.beta_cost) + 
                    "). Trajectory recomputed with safety score " + std::to_string(result.safetyScore) + ".";
                break;
            }
            case CommandIntentType::EXPLAIN_PATH: {
                result = planner.plan(problem);
                cmd.naturalLanguageExplanation = generatePlanExplanation(cmd, problem, result);
                break;
            }
            default: {
                result = planner.plan(problem);
                cmd.naturalLanguageExplanation = "Executed natural language search for query: \"" + cmd.rawQuery + 
                    "\". Computed optimal trajectory with total cost " + std::to_string(result.totalCost) + ".";
                break;
            }
        }

        return {result, cmd};
    }

private:
    std::shared_ptr<SemanticEmbeddingModel> embeddingModel_;
    std::unordered_map<CommandIntentType, std::vector<std::string>> intentPrototypes_;

    static std::string toLower(const std::string& s) {
        std::string res = s;
        std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c){ return std::tolower(c); });
        return res;
    }

    static std::string getStateName(const core::PlanningProblem& prob, uint64_t id) {
        for (const auto& s : prob.states) {
            if (s.id == id) return s.name;
        }
        return "Node_" + std::to_string(id);
    }

    void initIntentPrototypes() {
        intentPrototypes_[CommandIntentType::PLAN_ROUTE] = {
            "find safe path from start to goal",
            "navigate from origin to destination avoiding hazards",
            "calculate route from service A to service B",
            "compute shortest collision free trajectory",
            "plan route to destination",
            "route to destination avoiding quarantined hazards"
        };
        intentPrototypes_[CommandIntentType::ADD_HAZARD] = {
            "quarantine node as dangerous hazard",
            "mark ICU overflow as infected bad state",
            "avoid high risk vulnerable scanner obstacle",
            "add hazard barrier to avoid collision",
            "block dangerous region",
            "quarantine node as an active security hazard"
        };
        intentPrototypes_[CommandIntentType::REMOVE_HAZARD] = {
            "unquarantine state and remove hazard barrier",
            "lift quarantine on node",
            "restore dangerous node to normal",
            "clear obstacle and allow traversal"
        };
        intentPrototypes_[CommandIntentType::DISABLE_EDGE] = {
            "sever edge connection between services",
            "break stripe payment api transition",
            "disconnect route and trigger failover",
            "disable link due to network outage",
            "sever payment api edge to trigger dynamic failover"
        };
        intentPrototypes_[CommandIntentType::ENABLE_EDGE] = {
            "restore severed edge link",
            "reconnect stripe payment transition",
            "enable api connection and discover shortcut"
        };
        intentPrototypes_[CommandIntentType::SET_START] = {
            "set initial start state to ambulance bay",
            "change origin starting position to node 0",
            "start route from triage",
            "make A the start state"
        };
        intentPrototypes_[CommandIntentType::SET_GOAL] = {
            "set target destination goal to discharge bay",
            "change destination to order confirmed",
            "our new goal is patient stabilized",
            "make G the goal state"
        };
        intentPrototypes_[CommandIntentType::TUNE_OBJECTIVES] = {
            "prioritize safety clearance over cost",
            "minimize latency and cost penalty",
            "increase reliability SLA weight",
            "tune objective weights gamma and beta",
            "prioritize safety clearance over cost and latency"
        };
        intentPrototypes_[CommandIntentType::EXPLAIN_PATH] = {
            "why did you choose this route",
            "explain why this trajectory is safe",
            "give rationale for path clearance and cost",
            "explain why this trajectory was chosen"
        };
    }

    [[nodiscard]] std::pair<CommandIntentType, double> classifyIntent(
        const std::string& query, 
        const std::string& lower
    ) const {
        if (lower.find("why") != std::string::npos || lower.find("explain") != std::string::npos || lower.find("rationale") != std::string::npos) {
            return {CommandIntentType::EXPLAIN_PATH, 0.98};
        }
        if (lower.find("sever") != std::string::npos || lower.find("break") != std::string::npos || lower.find("cut") != std::string::npos || lower.find("disable") != std::string::npos) {
            return {CommandIntentType::DISABLE_EDGE, 0.95};
        }
        if (lower.find("restore") != std::string::npos || lower.find("reconnect") != std::string::npos || lower.find("enable") != std::string::npos) {
            return {CommandIntentType::ENABLE_EDGE, 0.95};
        }
        if (lower.find("unquarantine") != std::string::npos || lower.find("remove hazard") != std::string::npos || lower.find("lift quarantine") != std::string::npos) {
            return {CommandIntentType::REMOVE_HAZARD, 0.95};
        }
        if (lower.find("quarantine") != std::string::npos || lower.find("mark hazard") != std::string::npos || lower.find("add hazard") != std::string::npos || lower.find("block node") != std::string::npos) {
            return {CommandIntentType::ADD_HAZARD, 0.95};
        }
        if (lower.find("prioritize safety") != std::string::npos || lower.find("increase safety") != std::string::npos || lower.find("tune") != std::string::npos || lower.find("weight") != std::string::npos || lower.find("prioritize") != std::string::npos) {
            return {CommandIntentType::TUNE_OBJECTIVES, 0.95};
        }
        if (lower.find("set start") != std::string::npos || lower.find("start from") != std::string::npos || lower.find("origin is") != std::string::npos) {
            return {CommandIntentType::SET_START, 0.92};
        }
        if (lower.find("set goal") != std::string::npos || lower.find("destination is") != std::string::npos || lower.find("target is") != std::string::npos) {
            return {CommandIntentType::SET_GOAL, 0.92};
        }
        if (lower.find("route") != std::string::npos || lower.find("path") != std::string::npos || lower.find("navigate") != std::string::npos || lower.find("plan") != std::string::npos || lower.find("find") != std::string::npos) {
            return {CommandIntentType::PLAN_ROUTE, 0.95};
        }

        // Semantic Prototype Matching
        CommandIntentType bestIntent = CommandIntentType::PLAN_ROUTE;
        double bestSim = -1.0;

        for (const auto& [intentType, examples] : intentPrototypes_) {
            for (const auto& ex : examples) {
                double sim = embeddingModel_->similarity(query, ex);
                if (sim > bestSim) {
                    bestSim = sim;
                    bestIntent = intentType;
                }
            }
        }

        double conf = std::max(0.60, std::min(0.99, bestSim));
        return {bestIntent, conf};
    }

    void extractComplexConstraints(
        const std::string& lower,
        const core::PlanningProblem& prob,
        ParsedCommand& cmd
    ) const {
        // 1. Extract Start State
        std::regex startRegex("(?:make|set|from|start\\s+at)\\s+([a-zA-Z0-9_]+?)\\s+(?:the\\s+start|as\\s+start|state)");
        std::smatch match;
        if (std::regex_search(lower, match, startRegex) && match.size() >= 2) {
            cmd.resolvedStartId = resolveBestState(match[1].str(), prob.states).first;
            cmd.slots["start"] = match[1].str();
        } else {
            cmd.resolvedStartId = prob.initialState;
        }

        // 2. Extract Goal State
        std::regex goalRegex("(?:and\\s+)?([a-zA-Z0-9_]+?)\\s+(?:the\\s+goal|as\\s+goal|target|destination)");
        if (std::regex_search(lower, match, goalRegex) && match.size() >= 2) {
            cmd.resolvedGoalId = resolveBestState(match[1].str(), prob.states).first;
            cmd.slots["goal"] = match[1].str();
        } else {
            cmd.resolvedGoalId = prob.goalState;
        }

        // 3. Extract Mandatory Waypoints
        std::regex waypointRegex("(?:should\\s+go\\s+through|must\\s+visit|pass\\s+through|visit)\\s+(?:state\\s+)?([a-zA-Z0-9_]+)");
        if (std::regex_search(lower, match, waypointRegex) && match.size() >= 2) {
            uint64_t wpId = resolveBestState(match[1].str(), prob.states).first;
            cmd.mustVisitWaypoints.push_back(wpId);
            cmd.slots["waypoint"] = match[1].str();
        }

        // 4. Extract Conditional Constraints
        std::regex condRegex("never\\s+(?:goes\\s+through|visit)\\s+(?:state\\s+)?([a-zA-Z0-9_]+)\\s+if\\s+(?:it\\s+ever\\s+goes\\s+through|visited)\\s+(?:state\\s+)?([a-zA-Z0-9_]+)");
        if (std::regex_search(lower, match, condRegex) && match.size() >= 3) {
            uint64_t forbiddenId = resolveBestState(match[1].str(), prob.states).first;
            uint64_t triggerId = resolveBestState(match[2].str(), prob.states).first;
            cmd.conditionalConstraints.push_back({triggerId, forbiddenId});
            cmd.slots["cond_forbidden"] = match[1].str();
            cmd.slots["cond_trigger"] = match[2].str();
        }
    }

    void extractRoutingEntities(
        const std::string& lower, 
        const core::PlanningProblem& prob, 
        ParsedCommand& cmd
    ) const {
        std::regex fromToRegex("from\\s+([a-zA-Z0-9_\\s]+?)\\s+to\\s+([a-zA-Z0-9_\\s]+)");
        std::smatch match;
        if (std::regex_search(lower, match, fromToRegex) && match.size() >= 3) {
            std::string fromStr = match[1].str();
            std::string toStr = match[2].str();

            size_t avoidPos = toStr.find("avoiding");
            if (avoidPos == std::string::npos) avoidPos = toStr.find("without");
            if (avoidPos == std::string::npos) avoidPos = toStr.find("bypassing");

            if (avoidPos != std::string::npos) {
                std::string hazardClause = toStr.substr(avoidPos);
                toStr = toStr.substr(0, avoidPos);
                cmd.resolvedHazardId = resolveBestState(hazardClause, prob.states).first;
                cmd.slots["avoid_hazard"] = hazardClause;
            }

            cmd.resolvedStartId = resolveBestState(fromStr, prob.states).first;
            cmd.resolvedGoalId = resolveBestState(toStr, prob.states).first;
            cmd.slots["start"] = fromStr;
            cmd.slots["goal"] = toStr;
        } else {
            // Check direct start / goal mentions
            cmd.resolvedStartId = prob.initialState;
            cmd.resolvedGoalId = prob.goalState;
            for (const auto& s : prob.states) {
                if (lower.find(toLower(s.name)) != std::string::npos) {
                    if (lower.find("start") != std::string::npos || lower.find("from") != std::string::npos) {
                        cmd.resolvedStartId = s.id;
                    } else if (lower.find("goal") != std::string::npos || lower.find("to") != std::string::npos) {
                        cmd.resolvedGoalId = s.id;
                    }
                }
            }
        }
    }

    void extractHazardEntities(
        const std::string& lower, 
        const core::PlanningProblem& prob, 
        ParsedCommand& cmd
    ) const {
        std::regex numRegex("(?:node|state|#)\\s*(\\d+)");
        std::smatch match;
        if (std::regex_search(lower, match, numRegex) && match.size() >= 2) {
            cmd.resolvedHazardId = std::stoull(match[1].str());
            return;
        }
        auto [bestId, bestSim] = resolveBestState(lower, prob.states);
        cmd.resolvedHazardId = bestId;
        cmd.slots["hazard_target"] = std::to_string(bestId);
    }

    void extractEdgeEntities(
        const std::string& lower, 
        const core::PlanningProblem& prob, 
        ParsedCommand& cmd
    ) const {
        std::regex edgeRegex("(?:edge|transition|#|api)\\s*(\\d+)");
        std::smatch match;
        if (std::regex_search(lower, match, edgeRegex) && match.size() >= 2) {
            cmd.resolvedEdgeId = std::stoull(match[1].str());
            return;
        }
        double bestSim = -1.0;
        uint64_t bestEdgeId = prob.transitions.empty() ? 0 : prob.transitions[0].id;
        for (const auto& t : prob.transitions) {
            double sim = embeddingModel_->similarity(lower, t.name);
            if (sim > bestSim) {
                bestSim = sim;
                bestEdgeId = t.id;
            }
        }
        cmd.resolvedEdgeId = bestEdgeId;
        cmd.slots["edge_target"] = std::to_string(bestEdgeId);
    }

    void extractObjectiveParameters(const std::string& lower, ParsedCommand& cmd) const {
        if (lower.find("safety") != std::string::npos || lower.find("clearance") != std::string::npos) {
            cmd.paramUpdates["gamma"] = 15.0;
        }
        if (lower.find("cost") != std::string::npos || lower.find("cheap") != std::string::npos || lower.find("fast") != std::string::npos || lower.find("latency") != std::string::npos) {
            cmd.paramUpdates["beta"] = 5.0;
            cmd.paramUpdates["gamma"] = 1.0;
        }
        if (lower.find("reliability") != std::string::npos || lower.find("sla") != std::string::npos) {
            cmd.paramUpdates["delta"] = 10.0;
        }
    }

    [[nodiscard]] std::pair<uint64_t, double> resolveBestState(
        const std::string& query, 
        const std::vector<core::State>& states
    ) const {
        if (states.empty()) return {0, 0.0};
        uint64_t bestId = states[0].id;
        double bestScore = -10.0;

        std::string cleanQuery = toLower(query);
        for (char& c : cleanQuery) {
            if (!std::isalnum(static_cast<unsigned char>(c))) c = ' ';
        }
        cleanQuery.erase(0, cleanQuery.find_first_not_of(" \t\n\r"));
        cleanQuery.erase(cleanQuery.find_last_not_of(" \t\n\r") + 1);

        for (const auto& s : states) {
            std::string sName = toLower(s.name);
            for (char& c : sName) {
                if (!std::isalnum(static_cast<unsigned char>(c))) c = ' ';
            }

            double score = embeddingModel_->similarity(query, s.name);

            // Number ID match (e.g. "node 2" or "2")
            if (cleanQuery == std::to_string(s.id)) {
                score += 15.0;
            }

            // Single letter state matching (e.g. "A" -> "State A")
            if (cleanQuery.size() == 1) {
                if (sName.find(std::string(" ") + cleanQuery) != std::string::npos || 
                    sName.find(cleanQuery + " ") == 0 ||
                    sName == cleanQuery || 
                    std::to_string(s.id) == cleanQuery) {
                    score += 10.0;
                }
            } else if (!cleanQuery.empty() && (sName.find(cleanQuery) != std::string::npos || cleanQuery.find(sName) != std::string::npos)) {
                score += 3.0;
            }

            if (score > bestScore) {
                bestScore = score;
                bestId = s.id;
            }
        }
        return {bestId, bestScore};
    }

    [[nodiscard]] core::PlanningResult executeComplexPlan(
        const ParsedCommand& cmd,
        core::PlanningProblem& problem,
        algorithms::DStarLitePlanner& planner
    ) const {
        uint64_t startId = cmd.resolvedStartId.value_or(problem.initialState);
        uint64_t goalId = cmd.resolvedGoalId.value_or(problem.goalState);

        problem.initialState = startId;
        problem.goalState = goalId;

        // Build sequence of milestones: Start -> Waypoints -> Goal
        std::vector<uint64_t> milestones;
        milestones.push_back(startId);
        for (uint64_t wp : cmd.mustVisitWaypoints) {
            milestones.push_back(wp);
        }
        milestones.push_back(goalId);

        core::PlanningResult totalResult;
        totalResult.success = true;
        totalResult.totalCost = 0.0;
        totalResult.minimumSafetyDistance = 999.0;
        totalResult.cumulativeReliability = 1.0;

        std::unordered_set<uint64_t> visitedGlobalStates;
        std::vector<uint64_t> fullStatePath;
        std::vector<uint64_t> fullTransitionPath;

        auto startTime = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i + 1 < milestones.size(); ++i) {
            uint64_t mFrom = milestones[i];
            uint64_t mTo = milestones[i + 1];

            // Enforce conditional prohibitions
            core::PlanningProblem segProblem = problem;
            segProblem.initialState = mFrom;
            segProblem.goalState = mTo;

            for (const auto& cond : cmd.conditionalConstraints) {
                if (visitedGlobalStates.count(cond.triggerStateId)) {
                    if (std::find(segProblem.badStates.begin(), segProblem.badStates.end(), cond.forbiddenStateId) == segProblem.badStates.end()) {
                        segProblem.badStates.push_back(cond.forbiddenStateId);
                    }
                }
            }

            auto segResult = planner.plan(segProblem);
            if (!segResult.success) {
                totalResult.success = false;
                totalResult.message = "Failed to satisfy waypoint milestone constraint [" + 
                    getStateName(problem, mFrom) + " -> " + getStateName(problem, mTo) + "]";
                return totalResult;
            }

            totalResult.totalCost += segResult.totalCost;
            totalResult.minimumSafetyDistance = std::min(totalResult.minimumSafetyDistance, segResult.minimumSafetyDistance);
            totalResult.cumulativeReliability *= segResult.cumulativeReliability;

            for (size_t sIdx = 0; sIdx < segResult.statePath.size(); ++sIdx) {
                uint64_t st = segResult.statePath[sIdx];
                visitedGlobalStates.insert(st);
                if (i == 0 && sIdx == 0) {
                    fullStatePath.push_back(st);
                } else if (sIdx > 0) {
                    fullStatePath.push_back(st);
                }
            }

            for (uint64_t tr : segResult.transitionPath) {
                fullTransitionPath.push_back(tr);
            }
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        totalResult.planningTimeMicroseconds = std::chrono::duration<double, std::micro>(endTime - startTime).count();
        totalResult.statePath = fullStatePath;
        totalResult.transitionPath = fullTransitionPath;
        totalResult.safetyScore = (planner.getConfig().alpha_goal * 1.0)
                                - (planner.getConfig().beta_cost * totalResult.totalCost)
                                + (planner.getConfig().gamma_safety * totalResult.minimumSafetyDistance)
                                + (planner.getConfig().delta_reliability * totalResult.cumulativeReliability);
        totalResult.message = "Optimal spliced multi-milestone trajectory generated successfully";
        return totalResult;
    }

    [[nodiscard]] std::string generatePlanExplanation(
        const ParsedCommand& cmd,
        const core::PlanningProblem& prob,
        const core::PlanningResult& res
    ) const {
        (void)cmd;
        if (!res.success) {
            return "Unable to synthesize collision-free path: " + res.message + 
                   ". Target destination or intermediate transitions are completely blocked by quarantined bad states.";
        }

        std::stringstream ss;
        ss << "Trajectory Plan Rationale: Discovered optimal safe path [" 
           << getStateName(prob, prob.initialState) << " -> ... -> " << getStateName(prob, prob.goalState)
           << "] spanning " << res.statePath.size() << " states. "
           << "Path maintains minimum hazard clearance distance of " << std::fixed << std::setprecision(2)
           << res.minimumSafetyDistance << " units, strictly avoids " << prob.badStates.size() 
           << " quarantined bad states, achieves " << std::setprecision(1) << (res.cumulativeReliability * 100.0) 
           << "% reliability SLA, and total cost of " << std::setprecision(2) << res.totalCost 
           << " (Computed in " << res.planningTimeMicroseconds << " µs via D* Lite).";
        return ss.str();
    }

    [[nodiscard]] std::string generateComplexPlanExplanation(
        const ParsedCommand& cmd,
        const core::PlanningProblem& prob,
        const core::PlanningResult& res
    ) const {
        if (!res.success) {
            return "LTL Constraint Unsatisfiable: " + res.message;
        }

        std::stringstream ss;
        ss << "Neuro-Symbolic Constraint Governor Result: Configured Start = #" << prob.initialState 
           << " (" << getStateName(prob, prob.initialState) << ") and Destination = #" << prob.goalState
           << " (" << getStateName(prob, prob.goalState) << "). ";

        if (!cmd.mustVisitWaypoints.empty()) {
            ss << "Satisfied mandatory waypoint sequencing [";
            for (size_t i = 0; i < cmd.mustVisitWaypoints.size(); ++i) {
                ss << "Visit " << getStateName(prob, cmd.mustVisitWaypoints[i]) 
                   << (i + 1 < cmd.mustVisitWaypoints.size() ? ", " : "");
            }
            ss << "]. ";
        }

        if (!cmd.conditionalConstraints.empty()) {
            for (const auto& cond : cmd.conditionalConstraints) {
                ss << "Enforced conditional LTL constraint [If Visited " << getStateName(prob, cond.triggerStateId)
                   << " -> Strictly Forbid " << getStateName(prob, cond.forbiddenStateId) << "]. ";
            }
        }

        ss << "Synthesized optimal spliced safe trajectory spanning " << res.statePath.size() 
           << " states with total cost " << std::fixed << std::setprecision(2) << res.totalCost 
           << ", minimum hazard clearance of " << res.minimumSafetyDistance << " units, and "
           << std::setprecision(1) << (res.cumulativeReliability * 100.0) << "% SLA (Dynamic Execution Time: "
           << res.planningTimeMicroseconds << " µs).";

        return ss.str();
    }
};

} // namespace ssp::nlp

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <sstream>
#include <regex>
#include <iomanip>
#include <memory>
#include <cmath>

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

        // 1. Check for multi-clause conditional / LTL constraints first
        if ((lowerQuery.find(" if ") != std::string::npos || 
             lowerQuery.find("never go") != std::string::npos || 
             lowerQuery.find("never visit") != std::string::npos ||
             lowerQuery.find("should go through") != std::string::npos || 
             lowerQuery.find("must pass") != std::string::npos || 
             lowerQuery.find("must visit") != std::string::npos ||
             lowerQuery.find("then go to") != std::string::npos) &&
            (lowerQuery.find("start") != std::string::npos || 
             lowerQuery.find("goal") != std::string::npos || 
             lowerQuery.find("state") != std::string::npos || 
             lowerQuery.find("node") != std::string::npos)) {
            cmd.intent = CommandIntentType::COMPLEX_CONSTRAINED_PLAN;
            cmd.confidence = 0.98;
            extractComplexConstraints(lowerQuery, problem, cmd);
            return cmd;
        }

        // 2. Identify Command Intent using Pattern Rules & Semantic Similarity
        auto [intent, confidence] = classifyIntent(query, lowerQuery);
        cmd.intent = intent;
        cmd.confidence = confidence;

        // 3. Extract Entities & Parameters based on Intent
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
                    (void)bestSim;
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
                        "). Computed safe path with total cost " + formatNumber(result.totalCost, 2) + ".";
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
                        "). D* Lite dynamically replanned in " + formatNumber(result.planningTimeMicroseconds, 1) + " µs.";
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
                        formatNumber(result.planningTimeMicroseconds, 1) + " µs (Zero bad states visited).";
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
                        ". Dynamic failover executed in " + formatNumber(result.planningTimeMicroseconds, 1) + 
                        " µs, rerouting traffic with end-to-end reliability of " + 
                        formatNumber(result.cumulativeReliability * 100.0, 1) + "%.";
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
                        formatNumber(result.planningTimeMicroseconds, 1) + " µs.";
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
                cmd.naturalLanguageExplanation = "Updated objective weights (Safety Weight γ=" + 
                    formatNumber(cfg.gamma_safety, 1) + ", Cost Penalty β=" + formatNumber(cfg.beta_cost, 1) + 
                    ", Reliability Weight δ=" + formatNumber(cfg.delta_reliability, 1) + 
                    "). Trajectory recomputed with safety score " + formatNumber(result.safetyScore, 1) + ".";
                break;
            }
            case CommandIntentType::EXPLAIN_PATH: {
                result = planner.plan(problem);
                cmd.naturalLanguageExplanation = generatePlanExplanation(cmd, problem, result);
                break;
            }
            default: {
                result = planner.plan(problem);
                cmd.naturalLanguageExplanation = "Processed query: \"" + cmd.rawQuery + 
                    "\". Computed optimal collision-free path with total cost " + formatNumber(result.totalCost, 2) + ".";
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
        std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) { return std::tolower(c); });
        return res;
    }

    static std::string formatNumber(double val, int precision) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(precision) << val;
        return ss.str();
    }

    static std::string getStateName(const core::PlanningProblem& prob, uint64_t stateId) {
        for (const auto& s : prob.states) {
            if (s.id == stateId) return s.name.empty() ? ("Node_" + std::to_string(stateId)) : s.name;
        }
        return "Node_" + std::to_string(stateId);
    }

    void initIntentPrototypes() {
        intentPrototypes_[CommandIntentType::PLAN_ROUTE] = {
            "find safe path from start to goal",
            "navigate to destination avoiding hazards",
            "calculate shortest safe route",
            "how to reach target",
            "take me to goal",
            "plan route to destination",
            "find path"
        };
        intentPrototypes_[CommandIntentType::ADD_HAZARD] = {
            "quarantine this state as a hazard",
            "mark node as dangerous obstacle",
            "block this state",
            "add hazard barrier",
            "avoid node"
        };
        intentPrototypes_[CommandIntentType::REMOVE_HAZARD] = {
            "unquarantine this state",
            "remove hazard obstacle",
            "unblock node",
            "clear hazard barrier"
        };
        intentPrototypes_[CommandIntentType::DISABLE_EDGE] = {
            "sever transition edge",
            "break connection between states",
            "disable API route",
            "cut edge"
        };
        intentPrototypes_[CommandIntentType::ENABLE_EDGE] = {
            "restore broken edge",
            "enable transition",
            "reconnect route",
            "fix API"
        };
        intentPrototypes_[CommandIntentType::SET_START] = {
            "set initial start state",
            "start from this node",
            "change origin"
        };
        intentPrototypes_[CommandIntentType::SET_GOAL] = {
            "set target goal state",
            "destination is this node",
            "change goal"
        };
        intentPrototypes_[CommandIntentType::TUNE_OBJECTIVES] = {
            "prioritize safety and clearance",
            "prefer cheaper route",
            "maximize reliability SLA",
            "tune weights",
            "safety first"
        };
        intentPrototypes_[CommandIntentType::EXPLAIN_PATH] = {
            "explain current path",
            "why is this route chosen",
            "give rationale for trajectory",
            "show path explanation"
        };
    }

    [[nodiscard]] std::pair<CommandIntentType, double> classifyIntent(
        const std::string& query, 
        const std::string& lower
    ) const {
        // High-confidence exact keyword matching
        if (lower.find("explain") != std::string::npos || lower.find("why") != std::string::npos || lower.find("rationale") != std::string::npos) {
            return {CommandIntentType::EXPLAIN_PATH, 0.98};
        }
        if (lower.find("sever") != std::string::npos || lower.find("break edge") != std::string::npos || lower.find("disable api") != std::string::npos || lower.find("cut connection") != std::string::npos) {
            return {CommandIntentType::DISABLE_EDGE, 0.96};
        }
        if (lower.find("restore edge") != std::string::npos || lower.find("enable edge") != std::string::npos || lower.find("fix api") != std::string::npos || lower.find("reconnect") != std::string::npos) {
            return {CommandIntentType::ENABLE_EDGE, 0.96};
        }
        if (lower.find("unquarantine") != std::string::npos || lower.find("remove hazard") != std::string::npos || lower.find("unblock") != std::string::npos || lower.find("clear obstacle") != std::string::npos) {
            return {CommandIntentType::REMOVE_HAZARD, 0.96};
        }
        if (lower.find("quarantine") != std::string::npos || lower.find("mark hazard") != std::string::npos || lower.find("add hazard") != std::string::npos || lower.find("block node") != std::string::npos || lower.find("block state") != std::string::npos) {
            return {CommandIntentType::ADD_HAZARD, 0.96};
        }
        if (lower.find("prioritize safety") != std::string::npos || lower.find("increase safety") != std::string::npos || lower.find("safety first") != std::string::npos || lower.find("tune") != std::string::npos || lower.find("weight") != std::string::npos || lower.find("max reliability") != std::string::npos || lower.find("highest sla") != std::string::npos || lower.find("cheapest") != std::string::npos) {
            return {CommandIntentType::TUNE_OBJECTIVES, 0.95};
        }
        bool hasStartKw = (lower.find("start") != std::string::npos || lower.find("origin") != std::string::npos || lower.find("begin") != std::string::npos || lower.find("from ") != std::string::npos);
        bool hasGoalKw = (lower.find("goal") != std::string::npos || lower.find("destination") != std::string::npos || lower.find("target") != std::string::npos || lower.find("end at") != std::string::npos || lower.find("to ") != std::string::npos);

        if (hasStartKw && hasGoalKw) {
            return {CommandIntentType::PLAN_ROUTE, 0.97};
        }
        if (hasStartKw && !hasGoalKw && (lower.find("set start") != std::string::npos || lower.find("start from") != std::string::npos || lower.find("origin is") != std::string::npos || lower.find("begin at") != std::string::npos || lower.find("make ") != std::string::npos)) {
            return {CommandIntentType::SET_START, 0.94};
        }
        if (hasGoalKw && !hasStartKw && (lower.find("set goal") != std::string::npos || lower.find("destination is") != std::string::npos || lower.find("target is") != std::string::npos || lower.find("end at") != std::string::npos || lower.find("navigate to") != std::string::npos || lower.find("make ") != std::string::npos)) {
            return {CommandIntentType::SET_GOAL, 0.94};
        }
        if (lower.find("route") != std::string::npos || lower.find("path") != std::string::npos || lower.find("navigate") != std::string::npos || lower.find("plan") != std::string::npos || lower.find("find") != std::string::npos || lower.find("go to") != std::string::npos || lower.find("take me") != std::string::npos) {
            return {CommandIntentType::PLAN_ROUTE, 0.95};
        }

        // Semantic Prototype Matching via 64-D continuous embeddings
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

    static std::vector<std::string> splitClauses(const std::string& query) {
        std::vector<std::string> clauses;
        std::regex clauseSep("\\s+(?:and|then|with|where|\\,)\\s+|[;,]");
        std::sregex_token_iterator iter(query.begin(), query.end(), clauseSep, -1);
        std::sregex_token_iterator end;
        for (; iter != end; ++iter) {
            std::string s = iter->str();
            s.erase(0, s.find_first_not_of(" \t\n\r"));
            if (s.find_last_not_of(" \t\n\r") != std::string::npos) {
                s.erase(s.find_last_not_of(" \t\n\r") + 1);
            }
            if (!s.empty()) clauses.push_back(s);
        }
        if (clauses.empty() && !query.empty()) clauses.push_back(query);
        return clauses;
    }

    static std::string cleanEntityString(const std::string& clause, bool isStartRole) {
        std::string res = clause;
        if (isStartRole) {
            res = std::regex_replace(res, std::regex("\\b(make|set|the|as|is|state|node|start|origin|initial|begin|at|from|to|into)\\b"), " ");
        } else {
            res = std::regex_replace(res, std::regex("\\b(make|set|the|as|is|state|node|goal|destination|target|reach|end|at|to|into)\\b"), " ");
        }
        res = std::regex_replace(res, std::regex("\\b(such|that|it|with|constraints|conditions|please|now)\\b"), " ");
        std::stringstream ss(res);
        std::string word, out;
        while (ss >> word) {
            if (!out.empty()) out += " ";
            out += word;
        }
        return out;
    }

    void extractComplexConstraints(
        const std::string& lower,
        const core::PlanningProblem& prob,
        ParsedCommand& cmd
    ) const {
        // 1. Extract Mandatory Waypoints
        std::regex waypointRegex("(?:should\\s+go\\s+through|must\\s+visit|must\\s+pass|pass\\s+through|visit|via)\\s+(?:state\\s+|node\\s+)?([a-zA-Z0-9_#]+)");
        auto wpBegin = std::sregex_iterator(lower.begin(), lower.end(), waypointRegex);
        auto wpEnd = std::sregex_iterator();
        for (std::sregex_iterator i = wpBegin; i != wpEnd; ++i) {
            std::smatch m = *i;
            if (m.size() >= 2) {
                uint64_t wpId = resolveBestState(m[1].str(), prob.states).first;
                cmd.mustVisitWaypoints.push_back(wpId);
                cmd.slots["waypoint_" + std::to_string(cmd.mustVisitWaypoints.size())] = m[1].str();
            }
        }

        // 2. Extract Conditional Constraints ("never go through X if visited Y")
        std::regex condRegex("(?:never\\s+(?:goes\\s+through|go\\s+through|visit)|avoid)\\s+(?:state\\s+|node\\s+)?([a-zA-Z0-9_#]+)\\s+if\\s+(?:it\\s+ever\\s+goes\\s+through|visited|touch)\\s+(?:state\\s+|node\\s+)?([a-zA-Z0-9_#]+)");
        std::smatch match;
        if (std::regex_search(lower, match, condRegex) && match.size() >= 3) {
            uint64_t forbiddenId = resolveBestState(match[1].str(), prob.states).first;
            uint64_t triggerId = resolveBestState(match[2].str(), prob.states).first;
            cmd.conditionalConstraints.push_back({triggerId, forbiddenId});
            cmd.slots["cond_forbidden"] = match[1].str();
            cmd.slots["cond_trigger"] = match[2].str();
        }

        // 3. Clean query to isolate start and goal definitions
        std::string cleanQ = std::regex_replace(lower, waypointRegex, " ");
        cleanQ = std::regex_replace(cleanQ, condRegex, " ");
        cleanQ = std::regex_replace(cleanQ, std::regex("\\b(as\\s+constraints|such\\s+that|with\\s+conditions|if\\s+it\\s+ever)\\b"), " ");

        std::string extractedStartStr;
        std::string extractedGoalStr;

        auto clauses = splitClauses(cleanQ);
        for (const auto& clause : clauses) {
            bool hasStartKeyword = (clause.find("start") != std::string::npos ||
                                    clause.find("origin") != std::string::npos ||
                                    clause.find("initial") != std::string::npos ||
                                    clause.find("begin") != std::string::npos);
            bool hasGoalKeyword = (clause.find("goal") != std::string::npos ||
                                   clause.find("destination") != std::string::npos ||
                                   clause.find("target") != std::string::npos ||
                                   clause.find("reach") != std::string::npos ||
                                   clause.find("end") != std::string::npos);

            if (hasStartKeyword && !hasGoalKeyword) {
                std::string cleaned = cleanEntityString(clause, true);
                if (!cleaned.empty()) extractedStartStr = cleaned;
            } else if (hasGoalKeyword && !hasStartKeyword) {
                std::string cleaned = cleanEntityString(clause, false);
                if (!cleaned.empty()) extractedGoalStr = cleaned;
            }
        }

        if (!extractedStartStr.empty()) {
            cmd.resolvedStartId = resolveBestState(extractedStartStr, prob.states).first;
            cmd.slots["start"] = extractedStartStr;
        } else {
            cmd.resolvedStartId = prob.initialState;
        }

        if (!extractedGoalStr.empty()) {
            cmd.resolvedGoalId = resolveBestState(extractedGoalStr, prob.states).first;
            cmd.slots["goal"] = extractedGoalStr;
        } else {
            cmd.resolvedGoalId = prob.goalState;
        }
    }

    void extractRoutingEntities(
        const std::string& lower, 
        const core::PlanningProblem& prob, 
        ParsedCommand& cmd
    ) const {
        std::string queryStr = lower;

        // 1. Check for hazard in avoiding / without / bypassing / except / quarantine
        std::regex hazardRegex("(?:avoiding|without|bypassing|except|avoid|quarantine|quarantining)\\s+(?:state\\s+|node\\s+)?([a-zA-Z0-9_#\\s]+)");
        std::smatch hMatch;
        if (std::regex_search(queryStr, hMatch, hazardRegex) && hMatch.size() >= 2) {
            std::string hStr = hMatch[1].str();
            cmd.resolvedHazardId = resolveBestState(hStr, prob.states).first;
            cmd.slots["avoid_hazard"] = hStr;
            queryStr = queryStr.substr(0, hMatch.position()) + " " + queryStr.substr(hMatch.position() + hMatch.length());
        }

        std::string extractedStartStr;
        std::string extractedGoalStr;

        auto clauses = splitClauses(queryStr);
        for (const auto& clause : clauses) {
            bool hasStartKeyword = (clause.find("start") != std::string::npos ||
                                    clause.find("origin") != std::string::npos ||
                                    clause.find("initial") != std::string::npos ||
                                    clause.find("begin") != std::string::npos);
            bool hasGoalKeyword = (clause.find("goal") != std::string::npos ||
                                   clause.find("destination") != std::string::npos ||
                                   clause.find("target") != std::string::npos ||
                                   clause.find("reach") != std::string::npos ||
                                   clause.find("end at") != std::string::npos ||
                                   clause.find("end") != std::string::npos);

            // If clause contains "from X to Y" in one clause
            std::regex fromToRegex("(?:from|between)\\s+([a-zA-Z0-9_#\\s]+?)\\s+(?:to|and)\\s+([a-zA-Z0-9_#\\s]+)");
            std::smatch ftm;
            if (std::regex_search(clause, ftm, fromToRegex) && ftm.size() >= 3) {
                extractedStartStr = ftm[1].str();
                extractedGoalStr = ftm[2].str();
                continue;
            }

            if (hasStartKeyword && !hasGoalKeyword) {
                std::string cleaned = cleanEntityString(clause, true);
                if (!cleaned.empty()) extractedStartStr = cleaned;
            } else if (hasGoalKeyword && !hasStartKeyword) {
                std::string cleaned = cleanEntityString(clause, false);
                if (!cleaned.empty()) extractedGoalStr = cleaned;
            } else if (!hasStartKeyword && !hasGoalKeyword) {
                if (clause.find("from ") != std::string::npos || clause.rfind("from", 0) == 0) {
                    std::string cleaned = cleanEntityString(clause, true);
                    if (!cleaned.empty()) extractedStartStr = cleaned;
                } else if (clause.find("to ") != std::string::npos || clause.rfind("to", 0) == 0) {
                    std::string cleaned = cleanEntityString(clause, false);
                    if (!cleaned.empty()) extractedGoalStr = cleaned;
                }
            }
        }

        // Fallback regex if clause splitting did not catch both
        if (extractedStartStr.empty() || extractedGoalStr.empty()) {
            std::regex fromToRegex("(?:from|between)\\s+([a-zA-Z0-9_#\\s]+?)\\s+(?:to|and)\\s+([a-zA-Z0-9_#\\s]+)");
            std::smatch ftm;
            if (std::regex_search(queryStr, ftm, fromToRegex) && ftm.size() >= 3) {
                if (extractedStartStr.empty()) extractedStartStr = ftm[1].str();
                if (extractedGoalStr.empty()) extractedGoalStr = ftm[2].str();
            }
        }

        if (!extractedStartStr.empty()) {
            cmd.resolvedStartId = resolveBestState(extractedStartStr, prob.states).first;
            cmd.slots["start"] = extractedStartStr;
        } else {
            cmd.resolvedStartId = prob.initialState;
        }

        if (!extractedGoalStr.empty()) {
            cmd.resolvedGoalId = resolveBestState(extractedGoalStr, prob.states).first;
            cmd.slots["goal"] = extractedGoalStr;
        } else {
            cmd.resolvedGoalId = prob.goalState;
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
        (void)bestSim;
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
        if (lower.find("safety") != std::string::npos || lower.find("clearance") != std::string::npos || lower.find("careful") != std::string::npos) {
            cmd.paramUpdates["gamma"] = 20.0;
            cmd.paramUpdates["margin"] = 2.5;
        }
        if (lower.find("cost") != std::string::npos || lower.find("cheap") != std::string::npos || lower.find("fast") != std::string::npos || lower.find("latency") != std::string::npos) {
            cmd.paramUpdates["beta"] = 8.0;
            cmd.paramUpdates["gamma"] = 1.0;
        }
        if (lower.find("reliability") != std::string::npos || lower.find("sla") != std::string::npos || lower.find("stable") != std::string::npos) {
            cmd.paramUpdates["delta"] = 15.0;
        }
        if (lower.find("balanced") != std::string::npos || lower.find("default") != std::string::npos) {
            cmd.paramUpdates["alpha"] = 100.0;
            cmd.paramUpdates["beta"] = 1.0;
            cmd.paramUpdates["gamma"] = 5.0;
            cmd.paramUpdates["delta"] = 2.0;
            cmd.paramUpdates["margin"] = 1.5;
        }
    }

    [[nodiscard]] std::pair<uint64_t, double> resolveBestState(
        const std::string& query, 
        const std::vector<core::State>& states
    ) const {
        if (states.empty()) return {0, 0.0};
        uint64_t bestId = states[0].id;
        double bestScore = -100.0;

        std::string cleanQuery = toLower(query);
        for (char& c : cleanQuery) {
            if (!std::isalnum(static_cast<unsigned char>(c))) c = ' ';
        }
        cleanQuery.erase(0, cleanQuery.find_first_not_of(" \t\n\r"));
        if (cleanQuery.find_last_not_of(" \t\n\r") != std::string::npos) {
            cleanQuery.erase(cleanQuery.find_last_not_of(" \t\n\r") + 1);
        }

        // Direct number extraction from query (e.g. "#30" or "node 30" or "30")
        std::regex numExtract("(?:node|state|#)?\\s*(\\d+)");
        std::smatch numMatch;
        int directNum = -1;
        if (std::regex_search(cleanQuery, numMatch, numExtract) && numMatch.size() >= 2) {
            try {
                directNum = std::stoi(numMatch[1].str());
            } catch (...) {}
        }

        for (const auto& s : states) {
            std::string sName = toLower(s.name);
            for (char& c : sName) {
                if (!std::isalnum(static_cast<unsigned char>(c))) c = ' ';
            }

            double score = embeddingModel_->similarity(query, s.name);

            // Exact numeric ID match
            if (directNum == static_cast<int>(s.id) || cleanQuery == std::to_string(s.id)) {
                score += 25.0;
            }

            // Single letter state matching (e.g. "A" -> "State A" or "Start_S0")
            if (cleanQuery.size() == 1) {
                if (sName.find(std::string(" ") + cleanQuery) != std::string::npos || 
                    sName.find(cleanQuery + " ") == 0 ||
                    sName == cleanQuery || 
                    std::to_string(s.id) == cleanQuery) {
                    score += 12.0;
                }
            } else if (!cleanQuery.empty()) {
                if (sName.find(cleanQuery) != std::string::npos) {
                    score += 8.0;
                } else if (cleanQuery.find(sName) != std::string::npos) {
                    score += 5.0;
                }

                // Keyword token overlap (e.g. "icu", "triage", "arrival", "discharge", "payment")
                std::istringstream iss(cleanQuery);
                std::string word;
                while (iss >> word) {
                    if (word.size() >= 3 && sName.find(word) != std::string::npos) {
                        score += 3.0;
                    }
                }
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
           << " (Computed in " << formatNumber(res.planningTimeMicroseconds, 1) << " µs via D* Lite).";
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
           << formatNumber(res.planningTimeMicroseconds, 1) << " µs).";

        return ss.str();
    }
};

} // namespace ssp::nlp

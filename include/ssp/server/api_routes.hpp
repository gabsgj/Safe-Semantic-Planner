#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <iostream>
#include <algorithm>

#include "ssp/core/problem.hpp"
#include "ssp/core/result.hpp"
#include "ssp/algorithms/dstar_lite.hpp"
#include "ssp/config/config_manager.hpp"
#include "ssp/domains/microservice_mesh.hpp"
#include "ssp/domains/hospital_triage.hpp"
#include "ssp/domains/banking_kyc.hpp"
#include "ssp/domains/warehouse_robotics.hpp"
#include "ssp/nlp/nlp_parser.hpp"
#include "third_party/httplib/httplib.h"
#include "third_party/nlohmann/json.hpp"

namespace ssp::server {

class ApiHandler {
private:
    std::mutex mutex_;
    core::PlanningProblem currentProblem_;
    config::PlannerConfig currentConfig_;
    std::unique_ptr<algorithms::DStarLitePlanner> planner_;
    nlp::NlpParser nlpParser_;

public:
    explicit ApiHandler(config::PlannerConfig config = config::PlannerConfig{})
        : currentConfig_(std::move(config)) {
        planner_ = std::make_unique<algorithms::DStarLitePlanner>(currentConfig_);
    }

    void setProblem(core::PlanningProblem problem) {
        std::lock_guard<std::mutex> lock(mutex_);
        currentProblem_ = std::move(problem);
        planner_->plan(currentProblem_);
    }

    [[nodiscard]] core::PlanningProblem getProblem() {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentProblem_;
    }

    [[nodiscard]] config::PlannerConfig getConfig() {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentConfig_;
    }

    void registerRoutes(httplib::Server& svr) {
        auto setCors = [](httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
        };

        svr.Options(".*", [setCors](const httplib::Request&, httplib::Response& res) {
            setCors(res);
            res.status = 200;
        });

        // GET /api/health
        svr.Get("/api/health", [setCors](const httplib::Request&, httplib::Response& res) {
            setCors(res);
            nlohmann::json j = {{"status", "ok"}, {"engine", "DStarLite"}, {"version", "1.0.0"}};
            res.set_content(j.dump(), "application/json");
        });

        // GET /api/problem
        svr.Get("/api/problem", [this, setCors](const httplib::Request&, httplib::Response& res) {
            setCors(res);
            std::lock_guard<std::mutex> lock(mutex_);
            nlohmann::json j = {
                {"problem", currentProblem_},
                {"config", currentConfig_}
            };
            res.set_content(j.dump(), "application/json");
        });

        // GET /api/export_path
        svr.Get("/api/export_path", [this, setCors](const httplib::Request&, httplib::Response& res) {
            setCors(res);
            std::lock_guard<std::mutex> lock(mutex_);
            auto result = planner_->plan(currentProblem_);

            std::unordered_map<uint64_t, core::State> statesMap;
            for (const auto& s : currentProblem_.states) statesMap[s.id] = s;

            std::unordered_map<uint64_t, core::Transition> transMap;
            for (const auto& t : currentProblem_.transitions) transMap[t.id] = t;

            std::unordered_set<uint64_t> badSet(currentProblem_.badStates.begin(), currentProblem_.badStates.end());

            nlohmann::json enrichedTrajectory = nlohmann::json::array();
            for (size_t i = 0; i < result.statePath.size(); ++i) {
                uint64_t sid = result.statePath[i];
                auto sIt = statesMap.find(sid);

                std::string role = "Waypoint";
                if (sid == currentProblem_.initialState) role = "Start State";
                else if (sid == currentProblem_.goalState) role = "Goal Destination";
                else if (badSet.count(sid)) role = "Quarantined Hazard";

                nlohmann::json stepObj = {
                    {"stepIndex", i + 1},
                    {"stateId", sid},
                    {"name", sIt != statesMap.end() ? sIt->second.name : "Node_" + std::to_string(sid)},
                    {"description", (sIt != statesMap.end() && !sIt->second.name.empty()) ? "State #" + std::to_string(sid) + ": " + sIt->second.name : "State #" + std::to_string(sid)},
                    {"role", role},
                    {"embedding", sIt != statesMap.end() ? sIt->second.embedding : std::vector<double>{}}
                };

                if (i < result.transitionPath.size()) {
                    uint64_t tid = result.transitionPath[i];
                    auto tIt = transMap.find(tid);
                    stepObj["nextTransition"] = {
                        {"transitionId", tid},
                        {"name", tIt != transMap.end() ? tIt->second.name : "Transition_" + std::to_string(tid)},
                        {"description", tIt != transMap.end() ? "Directed transition #" + std::to_string(tid) + " from #" + std::to_string(tIt->second.from) + " to #" + std::to_string(tIt->second.to) : "Transition #" + std::to_string(tid)},
                        {"fromStateId", tIt != transMap.end() ? tIt->second.from : sid},
                        {"toStateId", tIt != transMap.end() ? tIt->second.to : (i + 1 < result.statePath.size() ? result.statePath[i + 1] : 0)},
                        {"cost", tIt != transMap.end() ? tIt->second.cost : 1.0},
                        {"reliabilitySLA", tIt != transMap.end() ? tIt->second.reliability : 1.0},
                        {"safetyMargin", tIt != transMap.end() ? tIt->second.safety : 1.0},
                        {"available", tIt != transMap.end() ? tIt->second.available : true}
                    };
                }
                enrichedTrajectory.push_back(stepObj);
            }

            nlohmann::json exportPayload = {
                {"domainName", currentProblem_.domainName.empty() ? "Safe Semantic Domain" : currentProblem_.domainName},
                {"planningEngine", "D* Lite Incremental Replanning"},
                {"spatialIndex", "Spatial k-d Tree Euclidean Repulsive Potential Field"},
                {"summary", {
                    {"success", result.success},
                    {"totalCost", result.totalCost},
                    {"minimumSafetyClearance", result.minimumSafetyDistance},
                    {"cumulativeReliability", result.cumulativeReliability},
                    {"compositeSafetyScore", result.safetyScore},
                    {"planningTimeMicroseconds", result.planningTimeMicroseconds},
                    {"exploredStatesCount", result.exploredStatesCount},
                    {"totalTrajectoryStates", result.statePath.size()},
                    {"totalTransitions", result.transitionPath.size()},
                    {"initialState", {
                        {"id", currentProblem_.initialState},
                        {"name", statesMap.count(currentProblem_.initialState) ? statesMap[currentProblem_.initialState].name : "Unknown"}
                    }},
                    {"goalState", {
                        {"id", currentProblem_.goalState},
                        {"name", statesMap.count(currentProblem_.goalState) ? statesMap[currentProblem_.goalState].name : "Unknown"}
                    }}
                }},
                {"trajectoryPath", enrichedTrajectory},
                {"stateIdSequence", result.statePath},
                {"transitionIdSequence", result.transitionPath},
                {"rawResult", result}
            };

            res.set_content(exportPayload.dump(2), "application/json");
        });

        // POST /api/plan
        svr.Post("/api/plan", [this, setCors](const httplib::Request& req, httplib::Response& res) {
            setCors(res);
            try {
                auto body = nlohmann::json::parse(req.body);
                core::PlanningProblem prob = body.get<core::PlanningProblem>();

                std::lock_guard<std::mutex> lock(mutex_);
                currentProblem_ = prob;
                auto result = planner_->plan(currentProblem_);

                nlohmann::json responseJson = {
                    {"result", result},
                    {"problem", currentProblem_}
                };
                res.set_content(responseJson.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                nlohmann::json err = {{"error", e.what()}};
                res.set_content(err.dump(), "application/json");
            }
        });

        // POST /api/add_state
        svr.Post("/api/add_state", [this, setCors](const httplib::Request& req, httplib::Response& res) {
            setCors(res);
            try {
                auto body = nlohmann::json::parse(req.body);
                std::lock_guard<std::mutex> lock(mutex_);

                // Find max ID + 1
                uint64_t maxId = 0;
                for (const auto& s : currentProblem_.states) {
                    if (s.id >= maxId) maxId = s.id + 1;
                }
                uint64_t newId = body.value("id", maxId);
                std::string name = body.value("name", "Node_" + std::to_string(newId));
                std::vector<double> embedding = body.value("embedding", std::vector<double>{5.0, 5.0});

                currentProblem_.states.push_back({newId, embedding, name});
                auto result = planner_->plan(currentProblem_);

                nlohmann::json responseJson = {
                    {"result", result},
                    {"problem", currentProblem_},
                    {"newId", newId}
                };
                res.set_content(responseJson.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                nlohmann::json err = {{"error", e.what()}};
                res.set_content(err.dump(), "application/json");
            }
        });

        // POST /api/delete_state
        svr.Post("/api/delete_state", [this, setCors](const httplib::Request& req, httplib::Response& res) {
            setCors(res);
            try {
                auto body = nlohmann::json::parse(req.body);
                uint64_t stateId = body.at("stateId").get<uint64_t>();

                std::lock_guard<std::mutex> lock(mutex_);
                // Erase state
                currentProblem_.states.erase(
                    std::remove_if(currentProblem_.states.begin(), currentProblem_.states.end(),
                                   [stateId](const core::State& s) { return s.id == stateId; }),
                    currentProblem_.states.end()
                );
                // Erase associated transitions
                currentProblem_.transitions.erase(
                    std::remove_if(currentProblem_.transitions.begin(), currentProblem_.transitions.end(),
                                   [stateId](const core::Transition& t) { return t.from == stateId || t.to == stateId; }),
                    currentProblem_.transitions.end()
                );
                // Erase from bad states
                currentProblem_.badStates.erase(
                    std::remove(currentProblem_.badStates.begin(), currentProblem_.badStates.end(), stateId),
                    currentProblem_.badStates.end()
                );

                auto result = planner_->plan(currentProblem_);
                nlohmann::json responseJson = {
                    {"result", result},
                    {"problem", currentProblem_}
                };
                res.set_content(responseJson.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                nlohmann::json err = {{"error", e.what()}};
                res.set_content(err.dump(), "application/json");
            }
        });

        // POST /api/add_transition
        svr.Post("/api/add_transition", [this, setCors](const httplib::Request& req, httplib::Response& res) {
            setCors(res);
            try {
                auto body = nlohmann::json::parse(req.body);
                std::lock_guard<std::mutex> lock(mutex_);

                uint64_t maxId = 100;
                for (const auto& t : currentProblem_.transitions) {
                    if (t.id >= maxId) maxId = t.id + 1;
                }
                uint64_t newId = body.value("id", maxId);
                uint64_t from = body.at("from").get<uint64_t>();
                uint64_t to = body.at("to").get<uint64_t>();
                double cost = body.value("cost", 2.0);
                double reliability = body.value("reliability", 0.99);
                double safety = body.value("safety", 1.0);
                bool available = body.value("available", true);
                std::string name = body.value("name", "API_" + std::to_string(from) + "_to_" + std::to_string(to));

                core::Transition newTrans(newId, from, to, cost, safety, reliability, available, name);
                currentProblem_.transitions.push_back(newTrans);
                auto result = planner_->addTransition(newTrans);

                nlohmann::json responseJson = {
                    {"result", result},
                    {"problem", currentProblem_},
                    {"newId", newId}
                };
                res.set_content(responseJson.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                nlohmann::json err = {{"error", e.what()}};
                res.set_content(err.dump(), "application/json");
            }
        });

        // POST /api/delete_transition
        svr.Post("/api/delete_transition", [this, setCors](const httplib::Request& req, httplib::Response& res) {
            setCors(res);
            try {
                auto body = nlohmann::json::parse(req.body);
                uint64_t edgeId = body.at("edgeId").get<uint64_t>();

                std::lock_guard<std::mutex> lock(mutex_);
                currentProblem_.transitions.erase(
                    std::remove_if(currentProblem_.transitions.begin(), currentProblem_.transitions.end(),
                                   [edgeId](const core::Transition& t) { return t.id == edgeId; }),
                    currentProblem_.transitions.end()
                );

                auto result = planner_->plan(currentProblem_);
                nlohmann::json responseJson = {
                    {"result", result},
                    {"problem", currentProblem_}
                };
                res.set_content(responseJson.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                nlohmann::json err = {{"error", e.what()}};
                res.set_content(err.dump(), "application/json");
            }
        });

        // POST /api/edit_state
        svr.Post("/api/edit_state", [this, setCors](const httplib::Request& req, httplib::Response& res) {
            setCors(res);
            try {
                auto body = nlohmann::json::parse(req.body);
                uint64_t stateId = body.at("id").get<uint64_t>();
                std::string name = body.value("name", "");
                std::vector<double> embedding = body.at("embedding").get<std::vector<double>>();

                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& s : currentProblem_.states) {
                    if (s.id == stateId) {
                        s.name = name;
                        s.embedding = embedding;
                        break;
                    }
                }
                auto result = planner_->plan(currentProblem_);
                nlohmann::json responseJson = {
                    {"result", result},
                    {"problem", currentProblem_}
                };
                res.set_content(responseJson.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                nlohmann::json err = {{"error", e.what()}};
                res.set_content(err.dump(), "application/json");
            }
        });

        // POST /api/edit_transition
        svr.Post("/api/edit_transition", [this, setCors](const httplib::Request& req, httplib::Response& res) {
            setCors(res);
            try {
                auto body = nlohmann::json::parse(req.body);
                uint64_t edgeId = body.at("id").get<uint64_t>();

                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& t : currentProblem_.transitions) {
                    if (t.id == edgeId) {
                        if (body.contains("name")) t.name = body["name"];
                        if (body.contains("cost")) t.cost = body["cost"];
                        if (body.contains("reliability")) t.reliability = body["reliability"];
                        if (body.contains("safety")) t.safety = body["safety"];
                        if (body.contains("available")) t.available = body["available"];
                        break;
                    }
                }
                auto result = planner_->plan(currentProblem_);
                nlohmann::json responseJson = {
                    {"result", result},
                    {"problem", currentProblem_}
                };
                res.set_content(responseJson.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                nlohmann::json err = {{"error", e.what()}};
                res.set_content(err.dump(), "application/json");
            }
        });

        // POST /api/toggle_edge
        svr.Post("/api/toggle_edge", [this, setCors](const httplib::Request& req, httplib::Response& res) {
            setCors(res);
            try {
                auto body = nlohmann::json::parse(req.body);
                uint64_t edgeId = body.at("edgeId").get<uint64_t>();
                bool available = body.at("available").get<bool>();

                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& t : currentProblem_.transitions) {
                    if (t.id == edgeId) {
                        t.available = available;
                        break;
                    }
                }
                auto result = planner_->setEdgeAvailability(edgeId, available);

                nlohmann::json responseJson = {
                    {"result", result},
                    {"problem", currentProblem_}
                };
                res.set_content(responseJson.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                nlohmann::json err = {{"error", e.what()}};
                res.set_content(err.dump(), "application/json");
            }
        });

        // POST /api/toggle_bad_state
        svr.Post("/api/toggle_bad_state", [this, setCors](const httplib::Request& req, httplib::Response& res) {
            setCors(res);
            try {
                auto body = nlohmann::json::parse(req.body);
                uint64_t stateId = body.at("stateId").get<uint64_t>();

                std::lock_guard<std::mutex> lock(mutex_);
                bool exists = false;
                auto it = currentProblem_.badStates.begin();
                while (it != currentProblem_.badStates.end()) {
                    if (*it == stateId) {
                        it = currentProblem_.badStates.erase(it);
                        exists = true;
                    } else {
                        ++it;
                    }
                }

                core::PlanningResult result;
                if (exists) {
                    result = planner_->removeBadState(stateId);
                } else {
                    currentProblem_.badStates.push_back(stateId);
                    result = planner_->addBadState(stateId);
                }

                nlohmann::json responseJson = {
                    {"result", result},
                    {"problem", currentProblem_},
                    {"isBadState", !exists}
                };
                res.set_content(responseJson.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                nlohmann::json err = {{"error", e.what()}};
                res.set_content(err.dump(), "application/json");
            }
        });

        // POST /api/update_goal
        svr.Post("/api/update_goal", [this, setCors](const httplib::Request& req, httplib::Response& res) {
            setCors(res);
            try {
                auto body = nlohmann::json::parse(req.body);
                uint64_t newGoalId = body.at("goalState").get<uint64_t>();

                std::lock_guard<std::mutex> lock(mutex_);
                currentProblem_.goalState = newGoalId;
                auto result = planner_->updateGoal(newGoalId);

                nlohmann::json responseJson = {
                    {"result", result},
                    {"problem", currentProblem_}
                };
                res.set_content(responseJson.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                nlohmann::json err = {{"error", e.what()}};
                res.set_content(err.dump(), "application/json");
            }
        });

        // POST /api/update_start
        svr.Post("/api/update_start", [this, setCors](const httplib::Request& req, httplib::Response& res) {
            setCors(res);
            try {
                auto body = nlohmann::json::parse(req.body);
                uint64_t newStartId = body.at("initialState").get<uint64_t>();

                std::lock_guard<std::mutex> lock(mutex_);
                currentProblem_.initialState = newStartId;
                auto result = planner_->replan(newStartId);

                nlohmann::json responseJson = {
                    {"result", result},
                    {"problem", currentProblem_}
                };
                res.set_content(responseJson.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                nlohmann::json err = {{"error", e.what()}};
                res.set_content(err.dump(), "application/json");
            }
        });

        // POST /api/update_state_pos
        svr.Post("/api/update_state_pos", [this, setCors](const httplib::Request& req, httplib::Response& res) {
            setCors(res);
            try {
                auto body = nlohmann::json::parse(req.body);
                uint64_t stateId = body.at("stateId").get<uint64_t>();
                std::vector<double> embedding = body.at("embedding").get<std::vector<double>>();

                std::lock_guard<std::mutex> lock(mutex_);
                for (auto& s : currentProblem_.states) {
                    if (s.id == stateId) {
                        s.embedding = embedding;
                        break;
                    }
                }
                auto result = planner_->plan(currentProblem_);

                nlohmann::json responseJson = {
                    {"result", result},
                    {"problem", currentProblem_}
                };
                res.set_content(responseJson.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                nlohmann::json err = {{"error", e.what()}};
                res.set_content(err.dump(), "application/json");
            }
        });

        // POST /api/update_weights
        svr.Post("/api/update_weights", [this, setCors](const httplib::Request& req, httplib::Response& res) {
            setCors(res);
            try {
                auto body = nlohmann::json::parse(req.body);
                std::lock_guard<std::mutex> lock(mutex_);
                if (body.contains("alpha_goal")) currentConfig_.alpha_goal = body["alpha_goal"];
                if (body.contains("beta_cost")) currentConfig_.beta_cost = body["beta_cost"];
                if (body.contains("gamma_safety")) currentConfig_.gamma_safety = body["gamma_safety"];
                if (body.contains("delta_reliability")) currentConfig_.delta_reliability = body["delta_reliability"];
                if (body.contains("safety_clearance_margin")) currentConfig_.safety_clearance_margin = body["safety_clearance_margin"];

                planner_->setConfig(currentConfig_);
                auto result = planner_->plan(currentProblem_);

                nlohmann::json responseJson = {
                    {"result", result},
                    {"config", currentConfig_}
                };
                res.set_content(responseJson.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                nlohmann::json err = {{"error", e.what()}};
                res.set_content(err.dump(), "application/json");
            }
        });

        // POST /api/nlp_command
        svr.Post("/api/nlp_command", [this, setCors](const httplib::Request& req, httplib::Response& res) {
            setCors(res);
            try {
                auto body = nlohmann::json::parse(req.body);
                std::string query = body.at("query").get<std::string>();

                std::lock_guard<std::mutex> lock(mutex_);
                auto parsedCmd = nlpParser_.parse(query, currentProblem_);
                auto [result, executedCmd] = nlpParser_.execute(parsedCmd, currentProblem_, *planner_);

                nlohmann::json responseJson = {
                    {"success", result.success},
                    {"intent", nlp::intentToString(executedCmd.intent)},
                    {"confidence", executedCmd.confidence},
                    {"rawQuery", executedCmd.rawQuery},
                    {"slots", executedCmd.slots},
                    {"resolvedStartId", executedCmd.resolvedStartId},
                    {"resolvedGoalId", executedCmd.resolvedGoalId},
                    {"resolvedHazardId", executedCmd.resolvedHazardId},
                    {"resolvedEdgeId", executedCmd.resolvedEdgeId},
                    {"paramUpdates", executedCmd.paramUpdates},
                    {"embedding", executedCmd.embedding},
                    {"explanation", executedCmd.naturalLanguageExplanation},
                    {"result", result},
                    {"problem", currentProblem_},
                    {"config", currentConfig_}
                };
                res.set_content(responseJson.dump(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                nlohmann::json err = {{"error", e.what()}};
                res.set_content(err.dump(), "application/json");
            }
        });

        // GET /api/templates
        svr.Get("/api/templates", [setCors](const httplib::Request&, httplib::Response& res) {
            setCors(res);
            nlohmann::json templates = nlohmann::json::array();

            // Template 0: Cartesian Safety Corridor (TC Benchmark)
            {
                core::PlanningProblem p0;
                p0.domainName = "Cartesian Safety Corridor (TC Benchmark)";
                p0.initialState = 0;
                p0.goalState = 6;
                p0.badStates = {90};
                p0.states = {
                    {0, {1.0, 4.0}, "Start_S0"},
                    {1, {3.0, 2.0}, "Main_Corridor_1"},
                    {2, {5.0, 2.0}, "Main_Corridor_2"},
                    {3, {3.0, 6.0}, "Safe_Highland_1"},
                    {4, {5.0, 6.0}, "Safe_Highland_2"},
                    {5, {7.0, 3.0}, "Pre_Goal_Junction"},
                    {6, {9.0, 4.0}, "Goal_Terminal"},
                    {90, {4.0, 3.5}, "Hazard_Core"}
                };
                p0.transitions = {
                    {101, 0, 1, 2.82, 0.6, 0.95, true, "Edge_S0_to_M1"},
                    {102, 1, 2, 2.00, 0.5, 0.95, true, "Edge_M1_to_M2"},
                    {103, 2, 5, 2.23, 0.6, 0.95, true, "Edge_M2_to_J5"},
                    {104, 5, 6, 2.23, 0.9, 0.99, true, "Edge_J5_to_Goal"},
                    {201, 0, 3, 2.82, 1.0, 0.99, true, "Edge_S0_to_H3"},
                    {202, 3, 4, 2.00, 1.0, 0.99, true, "Edge_H3_to_H4"},
                    {203, 4, 6, 4.47, 1.0, 0.99, true, "Edge_H4_to_Goal"},
                    {301, 1, 3, 4.00, 0.8, 0.95, true, "Bypass_1_to_3"},
                    {302, 2, 4, 4.00, 0.8, 0.95, true, "Bypass_2_to_4"}
                };
                templates.push_back(p0);
            }

            // Template 1: Microservice Resilience Mesh (E-Commerce)
            {
                domains::MicroserviceMeshDomain microDomain;
                templates.push_back(microDomain.createProblem());
            }

            // Template 2: Hospital Emergency Triage Pipeline
            {
                domains::HospitalTriageDomain hospDomain;
                templates.push_back(hospDomain.createProblem());
            }

            // Template 3: Banking KYC & Loan Underwriting
            {
                domains::BankingKycDomain bankDomain;
                templates.push_back(bankDomain.createProblem());
            }

            // Template 4: Warehouse Robotics & AMR Logistics
            {
                domains::WarehouseRoboticsDomain amrDomain;
                templates.push_back(amrDomain.createProblem());
            }

            // Template 5: SWE-bench Autonomous Coding Agent (Neuro-Symbolic Governor)
            {
                core::PlanningProblem p5;
                p5.domainName = "SWE-bench Autonomous Coding Agent Governor";
                p5.initialState = 0;
                p5.goalState = 5;
                p5.badStates = {2}; // Quarantined regression snapshot
                p5.states = {
                    {0, {1.0, 4.0, 0.50, 0.10, 0.0}, "Issue_Ingest_FailingTest [a1b2c3d]"},
                    {1, {3.0, 4.0, 0.70, 0.25, 0.0}, "AST_Fault_Localized [b2c3d4e]"},
                    {2, {5.5, 6.0, 0.60, 0.50, 3.0}, "Naive_Patch_Regressions_Trap [c3d4e5f]"},
                    {3, {7.5, 6.0, 0.30, 0.75, 5.0}, "Hotfix_Syntax_Error_Break [d4e5f6a]"},
                    {4, {5.5, 2.0, 0.95, 0.40, 0.0}, "Refactored_Threadsafe_Patch [e5f6a7b]"},
                    {5, {8.5, 4.0, 1.00, 0.45, 0.0}, "Clean_Verified_Commit_Goal [f6a7b8c]"}
                };
                p5.transitions = {
                    {101, 0, 1, 5.0, 1.0, 0.98, true, "tool: locate_fault()"},
                    {102, 1, 2, 4.0, 1.0, 0.70, true, "tool: apply_naive_patch()"},
                    {103, 2, 3, 6.0, 1.0, 0.40, true, "tool: hasty_hotfix()"},
                    {104, 1, 4, 8.0, 1.0, 0.99, true, "tool: refactor_threadsafe()"},
                    {105, 4, 5, 3.0, 1.0, 1.00, true, "tool: run_full_test_suite()"}
                };
                templates.push_back(p5);
            }

            res.set_content(templates.dump(), "application/json");
        });
    }
};

} // namespace ssp::server

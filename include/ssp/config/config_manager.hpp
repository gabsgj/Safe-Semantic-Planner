#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include "third_party/nlohmann/json.hpp"

namespace ssp::config {

struct PlannerConfig {
    std::string algorithm{"dstar_lite"};
    double alpha_goal{100.0};
    double beta_cost{1.0};
    double gamma_safety{5.0};
    double delta_reliability{2.0};
    double safety_clearance_margin{1.5};
    double hazard_barrier_decay_sigma{1.0};
    double hazard_critical_radius{0.0};
    double max_velocity_heuristic{1.0};
    size_t max_expansions{100000};
    bool allow_replan{true};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        PlannerConfig,
        algorithm,
        alpha_goal,
        beta_cost,
        gamma_safety,
        delta_reliability,
        safety_clearance_margin,
        hazard_barrier_decay_sigma,
        hazard_critical_radius,
        max_velocity_heuristic,
        max_expansions,
        allow_replan
    )
};

struct SpatialConfig {
    std::string index_type{"kdtree"};
    std::string distance_metric{"euclidean"};
    double influence_radius{3.0};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SpatialConfig, index_type, distance_metric, influence_radius)
};

struct ServerConfig {
    std::string host{"0.0.0.0"};
    int port{8080};
    std::string web_root{"./web"};
    bool enable_cors{true};
    bool log_requests{false};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ServerConfig, host, port, web_root, enable_cors, log_requests)
};

struct DomainsConfig {
    std::string default_template{"cartesian_benchmark"};
    std::vector<std::string> available_templates{
        "cartesian_benchmark", "microservice_mesh", "hospital_triage",
        "banking_workflow", "cloud_scheduler", "knowledge_graph"
    };

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(DomainsConfig, default_template, available_templates)
};

struct NlpConfig {
    std::string mode{"hybrid_slot_parser"};
    double confidence_threshold{0.75};
    bool enable_alias_matching{true};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(NlpConfig, mode, confidence_threshold, enable_alias_matching)
};

struct AgentConfig {
    size_t max_backtrack_depth{20};
    bool auto_rollback_on_bad_state{true};
    size_t execution_timeout_ms{5000};
    bool deduplicate_visited_states{true};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(
        AgentConfig,
        max_backtrack_depth,
        auto_rollback_on_bad_state,
        execution_timeout_ms,
        deduplicate_visited_states
    )
};

struct AppConfig {
    PlannerConfig planner;
    SpatialConfig spatial;
    ServerConfig server;
    DomainsConfig domains;
    NlpConfig nlp;
    AgentConfig agent;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AppConfig, planner, spatial, server, domains, nlp, agent)
};

class ConfigManager {
private:
    AppConfig config_;
    std::string configPath_{"config.json"};

public:
    explicit ConfigManager(std::string configPath = "config.json")
        : configPath_(std::move(configPath)) {
        load();
    }

    bool load(const std::string& path = "") {
        std::string target = path.empty() ? configPath_ : path;
        std::ifstream file(target);
        if (!file.is_open()) {
            std::cerr << "[ConfigManager] Warning: Could not open config file at '" << target
                      << "'. Using embedded defaults.\n";
            return false;
        }
        try {
            nlohmann::json j;
            file >> j;
            config_ = j.get<AppConfig>();
            configPath_ = target;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[ConfigManager] Error parsing config JSON: " << e.what() << "\n";
            return false;
        }
    }

    [[nodiscard]] const AppConfig& get() const { return config_; }
    [[nodiscard]] AppConfig& getMutable() { return config_; }
    [[nodiscard]] const PlannerConfig& planner() const { return config_.planner; }
    [[nodiscard]] const SpatialConfig& spatial() const { return config_.spatial; }
    [[nodiscard]] const ServerConfig& server() const { return config_.server; }
    [[nodiscard]] const DomainsConfig& domains() const { return config_.domains; }
    [[nodiscard]] const NlpConfig& nlp() const { return config_.nlp; }
    [[nodiscard]] const AgentConfig& agent() const { return config_.agent; }
};

} // namespace ssp::config

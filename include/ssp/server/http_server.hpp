#pragma once

#include <string>
#include <memory>
#include <thread>
#include <iostream>
#include <atomic>

#include "ssp/server/api_routes.hpp"
#include "ssp/config/config_manager.hpp"
#include "third_party/httplib/httplib.h"

namespace ssp::server {

class HttpServer {
private:
    config::ServerConfig config_;
    httplib::Server server_;
    std::unique_ptr<ApiHandler> apiHandler_;
    std::thread serverThread_;
    std::atomic<bool> isRunning_{false};

public:
    explicit HttpServer(config::ServerConfig serverCfg, config::PlannerConfig plannerCfg = config::PlannerConfig{})
        : config_(std::move(serverCfg)) {
        apiHandler_ = std::make_unique<ApiHandler>(std::move(plannerCfg));

        // 1. Set static files mount point for Web UI
        server_.set_mount_point("/", config_.web_root);

        // 2. Register API REST routes
        apiHandler_->registerRoutes(server_);

        // 3. Set default initial problem template
        core::PlanningProblem initialProb;
        initialProb.domainName = "Cartesian Safety Corridor (TC Benchmark)";
        initialProb.initialState = 0;
        initialProb.goalState = 6;
        initialProb.badStates = {90};
        initialProb.states = {
            {0, {1.0, 4.0}, "Start_S0"},
            {1, {3.0, 2.0}, "Main_Corridor_1"},
            {2, {5.0, 2.0}, "Main_Corridor_2"},
            {3, {3.0, 6.0}, "Safe_Highland_1"},
            {4, {5.0, 6.0}, "Safe_Highland_2"},
            {5, {7.0, 3.0}, "Pre_Goal_Junction"},
            {6, {9.0, 4.0}, "Goal_Terminal"},
            {90, {4.0, 3.5}, "Hazard_Core"}
        };
        initialProb.transitions = {
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
        apiHandler_->setProblem(std::move(initialProb));
    }

    ~HttpServer() {
        stop();
    }

    void startBlocking() {
        std::cout << "\n======================================================================\n";
        std::cout << "  SAFE SEMANTIC PLANNER (SSP) - EMBEDDED HTTP SERVER ACTIVE           \n";
        std::cout << "======================================================================\n";
        std::cout << "  • Listening on:       http://" << config_.host << ":" << config_.port << "\n";
        std::cout << "  • Web UI Dashboard:   http://localhost:" << config_.port << "\n";
        std::cout << "  • Static Files Path:  " << config_.web_root << "\n";
        std::cout << "  • Engine:             D* Lite Dynamic Replanner with KD-Tree\n";
        std::cout << "======================================================================\n\n";

        isRunning_ = true;
        server_.listen(config_.host.c_str(), config_.port);
        isRunning_ = false;
    }

    void startAsync() {
        if (isRunning_) return;
        isRunning_ = true;
        serverThread_ = std::thread([this]() {
            server_.listen(config_.host.c_str(), config_.port);
            isRunning_ = false;
        });
    }

    void stop() {
        if (isRunning_) {
            server_.stop();
            if (serverThread_.joinable()) {
                serverThread_.join();
            }
            isRunning_ = false;
        }
    }

    [[nodiscard]] bool isRunning() const { return isRunning_; }
    ApiHandler& getApiHandler() { return *apiHandler_; }
};

} // namespace ssp::server

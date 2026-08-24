#include <iostream>
#include <csignal>
#include <cstdlib>
#include "ssp/server/http_server.hpp"
#include "ssp/config/config_manager.hpp"

std::unique_ptr<ssp::server::HttpServer> g_server = nullptr;

void signalHandler(int signum) {
    std::cout << "\n[Server] Intercepted signal " << signum << ". Gracefully shutting down...\n";
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    std::string configPath = "config.json";
    if (argc > 1) {
        configPath = argv[1];
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    ssp::config::ConfigManager cfgMgr(configPath);
    auto appConfig = cfgMgr.get();

    // Render & Cloud Environment Variable Overrides
    const char* envPort = std::getenv("PORT");
    if (envPort) {
        try {
            appConfig.server.port = std::stoi(envPort);
            std::cout << "[Server] Overriding port from $PORT env: " << appConfig.server.port << "\n";
        } catch (...) {}
    }

    const char* envHost = std::getenv("HOST");
    if (envHost) {
        appConfig.server.host = envHost;
        std::cout << "[Server] Overriding host from $HOST env: " << appConfig.server.host << "\n";
    } else if (envPort) {
        // When running in cloud containers (Render / Fly.io / Heroku), default host to 0.0.0.0
        appConfig.server.host = "0.0.0.0";
    }

    g_server = std::make_unique<ssp::server::HttpServer>(appConfig.server, appConfig.planner);
    g_server->startBlocking();

    return 0;
}

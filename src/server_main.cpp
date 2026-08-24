#include <iostream>
#include <csignal>
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
    const auto& appConfig = cfgMgr.get();

    g_server = std::make_unique<ssp::server::HttpServer>(appConfig.server, appConfig.planner);
    g_server->startBlocking();

    return 0;
}

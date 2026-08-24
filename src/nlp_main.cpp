#include <iostream>
#include <string>
#include <iomanip>

#include "ssp/nlp/nlp_parser.hpp"
#include "ssp/domains/microservice_mesh.hpp"
#include "ssp/domains/hospital_triage.hpp"

using namespace ssp;

void runNlpCommand(const std::string& input, nlp::NlpParser& parser, core::PlanningProblem& prob, algorithms::DStarLitePlanner& planner) {
    std::cout << "\n" << std::string(75, '=') << "\n";
    std::cout << " NLP QUERY: \"" << input << "\"\n";
    std::cout << std::string(75, '=') << "\n";

    auto parsed = parser.parse(input, prob);

    std::cout << ">>> PARSED INTENT:      " << nlp::intentToString(parsed.intent) 
              << " (Confidence: " << std::fixed << std::setprecision(1) << (parsed.confidence * 100.0) << "%)\n";

    if (parsed.resolvedStartId) {
        std::cout << ">>> RESOLVED START:     Node #" << *parsed.resolvedStartId << "\n";
    }
    if (parsed.resolvedGoalId) {
        std::cout << ">>> RESOLVED GOAL:      Node #" << *parsed.resolvedGoalId << "\n";
    }
    if (parsed.resolvedHazardId) {
        std::cout << ">>> RESOLVED HAZARD:    Node #" << *parsed.resolvedHazardId << "\n";
    }
    if (parsed.resolvedEdgeId) {
        std::cout << ">>> RESOLVED EDGE:      Transition #" << *parsed.resolvedEdgeId << "\n";
    }

    std::cout << ">>> 64-D EMBEDDING:     [ ";
    for (size_t i = 0; i < 8; ++i) {
        std::cout << std::setprecision(2) << parsed.embedding[i] << " ";
    }
    std::cout << "... (" << parsed.embedding.size() << " dims) ]\n";

    // Execute
    auto [result, executedCmd] = parser.execute(parsed, prob, planner);

    std::cout << "\n>>> PLANNER EXECUTION:  " << (result.success ? "SUCCESS" : "FAILED") << "\n";
    std::cout << ">>> REPLAN TIME:        " << result.planningTimeMicroseconds << " µs\n";
    std::cout << ">>> TOTAL COST:         " << result.totalCost << "\n";
    std::cout << ">>> MIN CLEARANCE:      " << result.minimumSafetyDistance << "\n";
    std::cout << ">>> TRAJECTORY PATH:    ";
    for (size_t i = 0; i < result.statePath.size(); ++i) {
        std::cout << result.statePath[i] << (i + 1 < result.statePath.size() ? " -> " : "");
    }
    std::cout << "\n\n>>> NEURO-SYMBOLIC EXPLANATION:\n" << executedCmd.naturalLanguageExplanation << "\n";
}

int main(int argc, char* argv[]) {
    std::cout << "\n***************************************************************************\n";
    std::cout << "       SAFE SEMANTIC PLANNER - PHASE 6 ADVANCED NLP ENGINE                  \n";
    std::cout << "***************************************************************************\n";

    domains::MicroserviceMeshDomain domain;
    auto prob = domain.createProblem();
    config::PlannerConfig cfg;
    algorithms::DStarLitePlanner planner(cfg);
    planner.plan(prob);

    nlp::NlpParser parser;

    if (argc > 1) {
        std::string query;
        for (int i = 1; i < argc; ++i) {
            query += argv[i];
            if (i + 1 < argc) query += " ";
        }
        runNlpCommand(query, parser, prob, planner);
        return 0;
    }

    // Default Demo Script
    std::vector<std::string> demoQueries = {
        "Find a safe route from API Gateway to Order Confirmed avoiding vulnerable scanners",
        "Sever the Stripe payment API edge 102 due to gateway outage",
        "Quarantine node 2 as an active security hazard",
        "Prioritize safety clearance over cost and latency",
        "Explain why this trajectory was chosen"
    };

    for (const auto& q : demoQueries) {
        runNlpCommand(q, parser, prob, planner);
    }

    std::cout << "\n***************************************************************************\n";
    std::cout << "       PHASE 6 ADVANCED NLP DEMO RUN COMPLETED SUCCESSFULLY                \n";
    std::cout << "***************************************************************************\n\n";

    return 0;
}

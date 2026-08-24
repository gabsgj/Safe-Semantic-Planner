#pragma once

#include "ssp/domains/domain_template.hpp"

namespace ssp::domains {

/**
 * @brief Microservice Resilience Mesh Domain
 * 
 * Models distributed microservices with latency, error rate, CPU load, and circuit-breaker states.
 * Vector Space: [ Latency (ms), ErrorRate (%), CPULoad (%), Memory (%), SecurityTier (1-5) ]
 */
class MicroserviceMeshDomain : public DomainTemplate {
public:
    [[nodiscard]] std::string getName() const override {
        return "Microservice Resilience Mesh";
    }

    [[nodiscard]] std::string getDescription() const override {
        return "High-availability financial checkout mesh with automated circuit-breakers, fraud quarantine, and multi-cloud fallback.";
    }

    [[nodiscard]] std::vector<std::string> getDimensionNames() const override {
        return {"Latency_ms", "ErrorRate_pct", "CPULoad_pct", "Memory_pct", "SecurityTier"};
    }

    [[nodiscard]] core::PlanningProblem createProblem() const override {
        core::PlanningProblem prob;
        prob.domainName = "Microservice Resilience Mesh";
        prob.initialState = 0; // API Gateway
        prob.goalState = 7;    // Order Finalized
        prob.badStates = {4};  // Vulnerable Fraud Engine (Compromised / Quarantined)

        // States in 5D Vector Space: [Latency, ErrorRate, CPU, Memory, Security]
        prob.states = {
            {0, {1.0, 1.0, 15.0, 20.0, 5.0}, "API_Gateway_Ingress"},
            {1, {2.5, 1.5, 30.0, 35.0, 5.0}, "Auth_OAuth2_TokenService"},
            {2, {4.0, 2.0, 45.0, 50.0, 4.0}, "Stripe_Fast_Payment"},
            {3, {4.0, 4.0, 60.0, 55.0, 5.0}, "Escrow_Banking_Backup"},
            {4, {5.5, 1.5, 85.0, 90.0, 1.0}, "Legacy_Fraud_Scanner_Vulnerable"},
            {5, {5.5, 3.5, 25.0, 30.0, 4.0}, "Modern_AI_Fraud_Detector"},
            {6, {7.0, 2.5, 40.0, 45.0, 5.0}, "Inventory_Ledger_Commit"},
            {7, {8.5, 2.0, 20.0, 25.0, 5.0}, "Order_Confirmed_Terminal"}
        };

        // Transitions: (id, from, to, baseCost, safety, reliability, available, name)
        prob.transitions = {
            {101, 0, 1, 12.0, 1.0, 0.999, true, "POST /oauth/token"},
            {102, 1, 2, 45.0, 0.9, 0.995, true, "POST /stripe/charge"},
            {103, 1, 3, 95.0, 1.0, 0.999, true, "POST /escrow/wire"},
            {104, 2, 4, 30.0, 0.1, 0.700, true, "POST /fraud/legacy_inspect"},
            {105, 2, 5, 25.0, 1.0, 0.998, true, "POST /fraud/ai_verify"},
            {106, 3, 5, 20.0, 1.0, 0.999, true, "POST /fraud/ai_verify_escrow"},
            {107, 4, 6, 40.0, 0.1, 0.750, true, "POST /inventory/reserve_risky"},
            {108, 5, 6, 15.0, 1.0, 0.999, true, "POST /inventory/reserve_secure"},
            {109, 6, 7, 10.0, 1.0, 0.999, true, "POST /order/finalize"}
        };

        return prob;
    }
};

} // namespace ssp::domains

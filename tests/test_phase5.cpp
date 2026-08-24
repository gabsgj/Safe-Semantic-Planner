#include <iostream>
#include <cassert>
#include <cmath>
#include <iomanip>

#include "ssp/core/problem.hpp"
#include "ssp/core/result.hpp"
#include "ssp/algorithms/dstar_lite.hpp"
#include "ssp/config/config_manager.hpp"
#include "ssp/domains/microservice_mesh.hpp"
#include "ssp/domains/hospital_triage.hpp"
#include "ssp/domains/banking_kyc.hpp"
#include "ssp/domains/warehouse_robotics.hpp"

using namespace ssp;

void testMicroserviceMeshDomain() {
    std::cout << ">>> [TEST 1] Microservice Resilience Mesh Domain\n";
    domains::MicroserviceMeshDomain domain;
    assert(domain.getName() == "Microservice Resilience Mesh");
    assert(domain.getDimensionNames().size() == 5);

    auto prob = domain.createProblem();
    assert(prob.states.size() == 8);
    assert(prob.transitions.size() == 9);
    assert(prob.badStates.size() == 1);
    assert(prob.badStates[0] == 4); // Vulnerable legacy fraud scanner

    config::PlannerConfig cfg;
    cfg.gamma_safety = 10.0;
    algorithms::DStarLitePlanner planner(cfg);

    auto res = planner.plan(prob);
    std::cout << "  - Initial Plan Success: " << (res.success ? "YES" : "NO") << "\n";
    std::cout << "  - Path: ";
    for (size_t i = 0; i < res.statePath.size(); ++i) {
        std::cout << res.statePath[i] << (i + 1 < res.statePath.size() ? " -> " : "");
    }
    std::cout << "\n  - Cost: " << res.totalCost << ", Clearance: " << res.minimumSafetyDistance << "\n";

    assert(res.success);
    // Ensure bad state 4 is strictly avoided
    for (uint64_t s : res.statePath) {
        assert(s != 4);
    }
    // Path should go: 0 (API Gateway) -> 1 (Auth) -> 2 (Stripe) -> 5 (AI Fraud) -> 6 (Inventory) -> 7 (Order Confirmed)
    assert(res.statePath.front() == 0);
    assert(res.statePath.back() == 7);

    // Test Dynamic Failover: Break Stripe (transition 102)
    std::cout << "  - Triggering Dynamic Event: Stripe API Outage (Sever Edge 102)...\n";
    auto replanRes = planner.setEdgeAvailability(102, false);
    assert(replanRes.success);
    std::cout << "  - Replan Path: ";
    for (size_t i = 0; i < replanRes.statePath.size(); ++i) {
        std::cout << replanRes.statePath[i] << (i + 1 < replanRes.statePath.size() ? " -> " : "");
    }
    std::cout << "\n  - Replan Time: " << replanRes.planningTimeMicroseconds << " us, New Cost: " << replanRes.totalCost << "\n";

    // Replan path must route through Escrow (State 3) instead of Stripe (State 2)
    assert(replanRes.statePath[1] == 1);
    assert(replanRes.statePath[2] == 3); // Escrow backup
    std::cout << "  [PASSED] Microservice Mesh Failover Verified.\n\n";
}

void testHospitalTriageDomain() {
    std::cout << ">>> [TEST 2] Hospital Emergency Triage Pipeline\n";
    domains::HospitalTriageDomain domain;
    assert(domain.getName() == "Hospital Emergency Triage Pipeline");
    assert(domain.getDimensionNames().size() == 5);

    auto prob = domain.createProblem();
    assert(prob.states.size() == 7);
    assert(prob.badStates.size() == 1);
    assert(prob.badStates[0] == 3); // Overcrowded ICU

    config::PlannerConfig cfg;
    cfg.gamma_safety = 8.0;
    algorithms::DStarLitePlanner planner(cfg);

    auto res = planner.plan(prob);
    std::cout << "  - Clinical Triage Path: ";
    for (size_t i = 0; i < res.statePath.size(); ++i) {
        std::cout << res.statePath[i] << (i + 1 < res.statePath.size() ? " -> " : "");
    }
    std::cout << "\n  - Cost: " << res.totalCost << ", Reliability: " << res.cumulativeReliability << "\n";

    assert(res.success);
    // Strict zero-visit constraint on bad state 3
    for (uint64_t s : res.statePath) {
        assert(s != 3);
    }
    // Path: 0 (Ambulance) -> 2 (Trauma OT1) -> 4 (Surgery OT2) -> 5 (StepDown) -> 6 (Discharge)
    assert(res.statePath.front() == 0);
    assert(res.statePath.back() == 6);
    assert(res.statePath[1] == 2);

    std::cout << "  [PASSED] Hospital Triage Safe Pathway Verified.\n\n";
}

void testBankingKycDomain() {
    std::cout << ">>> [TEST 3] Banking KYC & Loan Underwriting Domain\n";
    domains::BankingKycDomain domain;
    assert(domain.getName() == "Banking KYC & Loan Underwriting");
    assert(domain.getDimensionNames().size() == 5);

    auto prob = domain.createProblem();
    assert(prob.badStates.size() == 1);
    assert(prob.badStates[0] == 3); // Sanctioned Entity / AML Hit

    config::PlannerConfig cfg;
    cfg.gamma_safety = 12.0;
    algorithms::DStarLitePlanner planner(cfg);

    auto res = planner.plan(prob);
    std::cout << "  - Compliant Loan Path: ";
    for (size_t i = 0; i < res.statePath.size(); ++i) {
        std::cout << res.statePath[i] << (i + 1 < res.statePath.size() ? " -> " : "");
    }
    std::cout << "\n  - Total Cost: " << res.totalCost << ", Safety Score: " << res.safetyScore << "\n";

    assert(res.success);
    for (uint64_t s : res.statePath) {
        assert(s != 3); // Must NEVER route through sanctioned entity
    }
    assert(res.statePath.front() == 0);
    assert(res.statePath.back() == 6);

    std::cout << "  [PASSED] Banking KYC Compliance Verified.\n\n";
}

void testWarehouseRoboticsDomain() {
    std::cout << ">>> [TEST 4] Warehouse Robotics & AMR Logistics Domain\n";
    domains::WarehouseRoboticsDomain domain;
    assert(domain.getName() == "Warehouse Robotics & AMR Logistics");
    assert(domain.getDimensionNames().size() == 5);

    auto prob = domain.createProblem();
    assert(prob.badStates.size() == 1);
    assert(prob.badStates[0] == 3); // Forklift collision zone

    config::PlannerConfig cfg;
    cfg.gamma_safety = 6.0;
    algorithms::DStarLitePlanner planner(cfg);

    auto res = planner.plan(prob);
    std::cout << "  - AMR Collision-Free Path: ";
    for (size_t i = 0; i < res.statePath.size(); ++i) {
        std::cout << res.statePath[i] << (i + 1 < res.statePath.size() ? " -> " : "");
    }
    std::cout << "\n  - Total Cost: " << res.totalCost << ", Clearance: " << res.minimumSafetyDistance << "\n";

    assert(res.success);
    for (uint64_t s : res.statePath) {
        assert(s != 3); // Avoid forklift crossing
    }
    assert(res.statePath.front() == 0);
    assert(res.statePath.back() == 6);

    std::cout << "  [PASSED] Warehouse AMR Robotics Navigation Verified.\n\n";
}

int main() {
    std::cout << "==========================================================\n";
    std::cout << "      SAFE SEMANTIC PLANNER - PHASE 5 TEST SUITE          \n";
    std::cout << "==========================================================\n\n";

    testMicroserviceMeshDomain();
    testHospitalTriageDomain();
    testBankingKycDomain();
    testWarehouseRoboticsDomain();

    std::cout << "==========================================================\n";
    std::cout << "  ALL 4 ENTERPRISE DOMAIN TEST SUITES PASSED (100% SUCCESS) \n";
    std::cout << "==========================================================\n";
    return 0;
}

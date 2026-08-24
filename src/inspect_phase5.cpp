#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

#include "ssp/core/problem.hpp"
#include "ssp/core/result.hpp"
#include "ssp/algorithms/dstar_lite.hpp"
#include "ssp/domains/microservice_mesh.hpp"
#include "ssp/domains/hospital_triage.hpp"
#include "ssp/domains/banking_kyc.hpp"
#include "ssp/domains/warehouse_robotics.hpp"

using namespace ssp;

void inspectDomain(const domains::DomainTemplate& domain) {
    std::cout << "\n" << std::string(75, '=') << "\n";
    std::cout << " DOMAIN: " << domain.getName() << "\n";
    std::cout << std::string(75, '=') << "\n";
    std::cout << "Description: " << domain.getDescription() << "\n";
    std::cout << "Dimensions:  ";
    auto dims = domain.getDimensionNames();
    for (size_t i = 0; i < dims.size(); ++i) {
        std::cout << "[" << i << "] " << dims[i] << (i + 1 < dims.size() ? ", " : "");
    }
    std::cout << "\n\n";

    auto prob = domain.createProblem();
    std::cout << "State Space Topology (" << prob.states.size() << " States, " << prob.transitions.size() << " Transitions):\n";
    std::cout << std::left << std::setw(6) << "ID"
              << std::setw(36) << "State Name"
              << std::setw(12) << "Type"
              << "Embedding Vector\n";
    std::cout << std::string(75, '-') << "\n";

    for (const auto& s : prob.states) {
        std::string type = "Normal";
        if (s.id == prob.initialState) type = "START (s_I)";
        else if (s.id == prob.goalState) type = "GOAL (s_G)";
        for (uint64_t b : prob.badStates) {
            if (s.id == b) type = "HAZARD (B)";
        }

        std::cout << std::left << std::setw(6) << s.id
                  << std::setw(36) << s.name
                  << std::setw(12) << type
                  << "[ ";
        for (double v : s.embedding) {
            std::cout << std::fixed << std::setprecision(1) << v << " ";
        }
        std::cout << "]\n";
    }

    // Run D* Lite Search
    config::PlannerConfig cfg;
    algorithms::DStarLitePlanner planner(cfg);
    auto res = planner.plan(prob);

    std::cout << "\nOptimal Execution Trajectory:\n";
    std::cout << std::string(75, '-') << "\n";
    std::cout << "  Success:               " << (res.success ? "YES (Safe Path Found)" : "NO (Blocked)") << "\n";
    std::cout << "  Planning Time:         " << res.planningTimeMicroseconds << " microseconds\n";
    std::cout << "  Total Cost:            " << res.totalCost << "\n";
    std::cout << "  Min Safety Clearance:  " << res.minimumSafetyDistance << "\n";
    std::cout << "  End-to-End SLA:        " << (res.cumulativeReliability * 100.0) << "%\n";
    std::cout << "  Trajectory Path:       ";
    for (size_t i = 0; i < res.statePath.size(); ++i) {
        auto it = std::find_if(prob.states.begin(), prob.states.end(), [&](const core::State& s){ return s.id == res.statePath[i]; });
        std::string sName = (it != prob.states.end()) ? it->name : std::to_string(res.statePath[i]);
        std::cout << sName << (i + 1 < res.statePath.size() ? "  ==>  \n                         " : "\n");
    }
}

int main() {
    std::cout << "\n***************************************************************************\n";
    std::cout << "       SAFE SEMANTIC PLANNER - PHASE 5 DOMAIN INSPECTOR                     \n";
    std::cout << "***************************************************************************\n";

    domains::MicroserviceMeshDomain microDomain;
    inspectDomain(microDomain);

    domains::HospitalTriageDomain hospDomain;
    inspectDomain(hospDomain);

    domains::BankingKycDomain bankDomain;
    inspectDomain(bankDomain);

    domains::WarehouseRoboticsDomain amrDomain;
    inspectDomain(amrDomain);

    std::cout << "\n***************************************************************************\n";
    std::cout << "       ALL 4 ENTERPRISE DOMAIN INSPECTIONS COMPLETE                        \n";
    std::cout << "***************************************************************************\n\n";
    return 0;
}

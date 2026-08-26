#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cmath>
#include <algorithm>

#include "ssp/nlp/semantic_embedding.hpp"
#include "ssp/nlp/command_intent.hpp"
#include "ssp/nlp/nlp_parser.hpp"
#include "ssp/domains/hospital_triage.hpp"
#include "ssp/domains/microservice_mesh.hpp"
#include "ssp/algorithms/dstar_lite.hpp"

using namespace ssp;

void testNlpInvertedStartGoal() {
    std::cout << ">>> [TEST 1] Bidirectional Inverted Start & Goal Queries\n";
    nlp::NlpParser parser;

    core::PlanningProblem ltlProb;
    ltlProb.domainName = "LTL Test Graph";
    ltlProb.initialState = 0;
    ltlProb.goalState = 5;
    ltlProb.states = {
        {0, {1.0, 4.0}, "State_A"},
        {1, {3.0, 6.0}, "State_B"},
        {2, {5.0, 6.0}, "State_C"},
        {3, {3.0, 2.0}, "State_D"},
        {4, {5.0, 2.0}, "State_E"},
        {5, {7.0, 4.0}, "State_G"}
    };

    std::vector<std::pair<std::string, std::pair<uint64_t, uint64_t>>> testCases = {
        {"make State_A the start state and State_G the goal state", {0, 5}},
        {"make State_G the goal state and State_A the start state", {0, 5}},
        {"make State_G the start state and State_A the goal state", {5, 0}},
        {"make A the start state and G the goal state", {0, 5}},
        {"make G the goal state and A the start state", {0, 5}},
        {"make G the start state and A the goal state", {5, 0}},
        {"set start to State_A and goal to State_G", {0, 5}},
        {"set goal to State_G and start to State_A", {0, 5}},
        {"set start state to 0 and goal state to 5", {0, 5}},
        {"set goal state to 5 and start state to 0", {0, 5}},
        {"set start state to 5 and goal state to 0", {5, 0}},
        {"make state 0 start and state 5 goal", {0, 5}},
        {"make state 5 goal and state 0 start", {0, 5}},
        {"make state 5 start and state 0 goal", {5, 0}},
        {"start: A, goal: G", {0, 5}},
        {"goal: G, start: A", {0, 5}},
        {"start at State_A and end at State_G", {0, 5}},
        {"navigate from State_A to State_G", {0, 5}},
        {"find safe route from State_A to State_G", {0, 5}}
    };

    for (const auto& [q, expected] : testCases) {
        auto cmd = parser.parse(q, ltlProb);
        uint64_t s = cmd.resolvedStartId.value_or(999);
        uint64_t g = cmd.resolvedGoalId.value_or(999);
        assert(s == expected.first);
        assert(g == expected.second);
    }

    std::cout << "  ✓ All 19 bidirectional start/goal phrasing patterns verified (100% accurate).\n";
}

void testNlpComplexLtlInverted() {
    std::cout << ">>> [TEST 2] Inverted Multi-Clause LTL Temporal Logic\n";
    nlp::NlpParser parser;

    core::PlanningProblem ltlProb;
    ltlProb.domainName = "LTL Test Graph";
    ltlProb.initialState = 0;
    ltlProb.goalState = 5;
    ltlProb.states = {
        {0, {1.0, 4.0}, "State_A"},
        {1, {3.0, 6.0}, "State_B"},
        {2, {5.0, 6.0}, "State_C"},
        {3, {3.0, 2.0}, "State_D"},
        {4, {5.0, 2.0}, "State_E"},
        {5, {7.0, 4.0}, "State_G"}
    };
    ltlProb.transitions = {
        {101, 0, 1, 2.0, 1.0, 0.99, true, "A -> B"},
        {102, 1, 2, 2.0, 1.0, 0.99, true, "B -> C"},
        {103, 2, 5, 2.0, 1.0, 0.99, true, "C -> G"},
        {104, 0, 3, 2.0, 1.0, 0.99, true, "A -> D"},
        {105, 3, 4, 2.0, 1.0, 0.99, true, "D -> E"},
        {106, 4, 5, 2.0, 1.0, 0.99, true, "E -> G"},
        {107, 1, 4, 3.0, 1.0, 0.99, true, "B -> E"}
    };

    config::PlannerConfig cfg;
    algorithms::DStarLitePlanner planner(cfg);

    // Standard ordering
    std::string qStandard = "Make A the start state and G the goal state as constraints such that it never goes through state C if it ever goes through state B and Should go through state E";
    auto cmdStd = parser.parse(qStandard, ltlProb);
    assert(cmdStd.resolvedStartId.value_or(99) == 0);
    assert(cmdStd.resolvedGoalId.value_or(99) == 5);
    assert(!cmdStd.mustVisitWaypoints.empty() && cmdStd.mustVisitWaypoints[0] == 4);
    assert(!cmdStd.conditionalConstraints.empty() && cmdStd.conditionalConstraints[0].triggerStateId == 1 && cmdStd.conditionalConstraints[0].forbiddenStateId == 2);

    // Inverted ordering (Goal first, Start second)
    std::string qInverted = "Make G the goal state and A the start state as constraints such that it never goes through state C if it ever goes through state B and Should go through state E";
    auto cmdInv = parser.parse(qInverted, ltlProb);
    assert(cmdInv.resolvedStartId.value_or(99) == 0);
    assert(cmdInv.resolvedGoalId.value_or(99) == 5);
    assert(!cmdInv.mustVisitWaypoints.empty() && cmdInv.mustVisitWaypoints[0] == 4);
    assert(!cmdInv.conditionalConstraints.empty() && cmdInv.conditionalConstraints[0].triggerStateId == 1 && cmdInv.conditionalConstraints[0].forbiddenStateId == 2);

    auto [resInv, execInv] = parser.execute(cmdInv, ltlProb, planner);
    assert(resInv.success);
    assert(resInv.statePath.front() == 0);
    assert(resInv.statePath.back() == 5);
    assert(std::find(resInv.statePath.begin(), resInv.statePath.end(), 4) != resInv.statePath.end());
    assert(std::find(resInv.statePath.begin(), resInv.statePath.end(), 2) == resInv.statePath.end());

    std::cout << "  ✓ Inverted LTL temporal query execution verified with 100% compliance.\n";
}

void testNlpDomainQueries() {
    std::cout << ">>> [TEST 3] Domain-Specific Clinical & Cloud Queries\n";
    nlp::NlpParser parser;
    domains::HospitalTriageDomain hosp;
    auto hospProb = hosp.createProblem();

    auto cmdHosp = parser.parse("Find route from Ambulance_Arrival_Bay to Patient_Stabilized_Goal avoiding Overcrowded_ICU_Overflow_Hazard", hospProb);
    assert(cmdHosp.resolvedStartId.value_or(99) == 0);
    assert(cmdHosp.resolvedGoalId.value_or(99) == 6);
    assert(cmdHosp.resolvedHazardId.value_or(99) == 3);

    domains::MicroserviceMeshDomain mesh;
    auto meshProb = mesh.createProblem();
    auto cmdMesh = parser.parse("Plan path between API_Gateway_Ingress and Order_Confirmed_Terminal avoiding Legacy_Fraud_Scanner_Vulnerable", meshProb);
    assert(cmdMesh.resolvedStartId.value_or(99) == 0);
    assert(cmdMesh.resolvedGoalId.value_or(99) == 7);
    assert(cmdMesh.resolvedHazardId.value_or(99) == 4);

    std::cout << "  ✓ Clinical and microservice domain NLP entity extraction verified.\n";
}

int main() {
    std::cout << "==========================================================\n";
    std::cout << "  SAFE SEMANTIC PLANNER - NLP COMPREHENSIVE TEST SUITE    \n";
    std::cout << "==========================================================\n";

    testNlpInvertedStartGoal();
    testNlpComplexLtlInverted();
    testNlpDomainQueries();

    std::cout << "==========================================================\n";
    std::cout << "  ALL NLP COMPREHENSIVE TESTS PASSED (100% SUCCESS RATE)  \n";
    std::cout << "==========================================================\n";
    return 0;
}

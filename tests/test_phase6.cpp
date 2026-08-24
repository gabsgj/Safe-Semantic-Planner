#include <iostream>
#include <cassert>
#include <cmath>

#include "ssp/nlp/semantic_embedding.hpp"
#include "ssp/nlp/command_intent.hpp"
#include "ssp/nlp/nlp_parser.hpp"
#include "ssp/domains/microservice_mesh.hpp"
#include "ssp/domains/hospital_triage.hpp"

using namespace ssp;

void testSemanticEmbeddings() {
    std::cout << ">>> [TEST 1] 64-D Semantic Vector Embedding Engine\n";
    nlp::SemanticEmbeddingModel model;

    auto emb1 = model.encode("navigate safe path to goal");
    auto emb2 = model.encode("find secure route to destination");
    auto emb3 = model.encode("chocolate peanut butter cake");

    assert(emb1.size() == 64);
    assert(emb2.size() == 64);
    assert(emb3.size() == 64);

    // Verify L2 Unit Norm
    double norm1 = 0.0, norm2 = 0.0;
    for (double v : emb1) norm1 += v * v;
    for (double v : emb2) norm2 += v * v;
    assert(std::abs(std::sqrt(norm1) - 1.0) < 1e-4);
    assert(std::abs(std::sqrt(norm2) - 1.0) < 1e-4);

    double sim12 = model.cosineSimilarity(emb1, emb2);
    double sim13 = model.cosineSimilarity(emb1, emb3);

    std::cout << "  - CosineSim(Planning, Planning): " << sim12 << "\n";
    std::cout << "  - CosineSim(Planning, Cake):     " << sim13 << "\n";

    assert(sim12 > 0.60);
    assert(sim12 > sim13);
    std::cout << "  [PASSED] Semantic Embedding & Cosine Similarity Verified.\n\n";
}

void testIntentClassification() {
    std::cout << ">>> [TEST 2] NLP Intent Classification & Confidence Scoring\n";
    nlp::NlpParser parser;
    domains::MicroserviceMeshDomain domain;
    auto prob = domain.createProblem();

    auto cmd1 = parser.parse("Find safe route from API Gateway to Order Confirmed", prob);
    assert(cmd1.intent == nlp::CommandIntentType::PLAN_ROUTE);
    assert(cmd1.confidence > 0.70);

    auto cmd2 = parser.parse("Quarantine node 4 as a high risk hazard", prob);
    assert(cmd2.intent == nlp::CommandIntentType::ADD_HAZARD);
    assert(cmd2.resolvedHazardId.has_value());
    assert(*cmd2.resolvedHazardId == 4);

    auto cmd3 = parser.parse("Sever the Stripe payment API edge 102", prob);
    assert(cmd3.intent == nlp::CommandIntentType::DISABLE_EDGE);
    assert(cmd3.resolvedEdgeId.has_value());
    assert(*cmd3.resolvedEdgeId == 102);

    auto cmd4 = parser.parse("Prioritize safety clearance over latency and cost", prob);
    assert(cmd4.intent == nlp::CommandIntentType::TUNE_OBJECTIVES);
    assert(cmd4.paramUpdates.count("gamma") > 0);

    auto cmd5 = parser.parse("Why did you choose this trajectory?", prob);
    assert(cmd5.intent == nlp::CommandIntentType::EXPLAIN_PATH);

    std::cout << "  [PASSED] Intent Classification for All Commands Verified.\n\n";
}

void testZeroShotEntityResolution() {
    std::cout << ">>> [TEST 3] Zero-Shot Entity Resolution Across Domain Graph\n";
    nlp::NlpParser parser;
    domains::HospitalTriageDomain hospDomain;
    auto hospProb = hospDomain.createProblem();

    auto cmd = parser.parse("Route patient from Ambulance Arrival to Patient Stabilized", hospProb);
    assert(cmd.resolvedStartId.has_value());
    assert(*cmd.resolvedStartId == 0); // Ambulance Arrival Bay
    assert(cmd.resolvedGoalId.has_value());
    assert(*cmd.resolvedGoalId == 6);  // Patient Stabilized Goal

    std::cout << "  - Resolved Start Mention -> Node #" << *cmd.resolvedStartId << "\n";
    std::cout << "  - Resolved Goal Mention  -> Node #" << *cmd.resolvedGoalId << "\n";
    std::cout << "  [PASSED] Zero-Shot Entity Resolution Verified.\n\n";
}

void testNlpExecutionAndExplanation() {
    std::cout << ">>> [TEST 4] End-to-End NLP Execution & Neuro-Symbolic Explanation\n";
    nlp::NlpParser parser;
    domains::MicroserviceMeshDomain domain;
    auto prob = domain.createProblem();
    config::PlannerConfig cfg;
    algorithms::DStarLitePlanner planner(cfg);

    // 1. Initial Plan via NLP
    auto cmdPlan = parser.parse("Plan safe trajectory from API Gateway to Order Confirmed", prob);
    auto [resPlan, execPlan] = parser.execute(cmdPlan, prob, planner);
    assert(resPlan.success);
    assert(!execPlan.naturalLanguageExplanation.empty());
    std::cout << "  - Generated Explanation: " << execPlan.naturalLanguageExplanation << "\n";

    // 2. Dynamic Failover via NLP Command
    auto cmdSever = parser.parse("Sever edge 102 due to Stripe payment outage", prob);
    auto [resSever, execSever] = parser.execute(cmdSever, prob, planner);
    assert(resSever.success);
    assert(resSever.statePath[2] == 3);
    std::cout << "  - Dynamic Failover Explanation: " << execSever.naturalLanguageExplanation << "\n";

    std::cout << "  [PASSED] End-to-End NLP Execution & Neuro-Symbolic Explanation Verified.\n\n";
}

void testComplexTemporalLTLConstraints() {
    std::cout << ">>> [TEST 5] Complex Multi-Clause LTL Temporal Logic & Conditional Constraints\n";
    nlp::NlpParser parser;

    // Create a 6-node test graph: A(0), B(1), C(2), D(3), E(4), G(5)
    core::PlanningProblem ltlProb;
    ltlProb.domainName = "LTL Test Graph";
    ltlProb.initialState = 0;
    ltlProb.goalState = 5;
    ltlProb.badStates = {};
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

    // Exact User Query:
    std::string complexQuery = "Make A the start state and G the goal state as constraints such that it never goes through state C if it ever goes through state B and Should go through state E";
    
    auto parsed = parser.parse(complexQuery, ltlProb);
    std::cout << "  - Parsed Intent: " << nlp::intentToString(parsed.intent) << " (" << (parsed.confidence * 100.0) << "%)\n";
    assert(parsed.intent == nlp::CommandIntentType::COMPLEX_CONSTRAINED_PLAN);
    assert(parsed.resolvedStartId.has_value() && *parsed.resolvedStartId == 0); // State A
    assert(parsed.resolvedGoalId.has_value() && *parsed.resolvedGoalId == 5);   // State G
    assert(!parsed.mustVisitWaypoints.empty() && parsed.mustVisitWaypoints[0] == 4); // State E
    assert(!parsed.conditionalConstraints.empty());
    assert(parsed.conditionalConstraints[0].triggerStateId == 1);   // If Visited B
    assert(parsed.conditionalConstraints[0].forbiddenStateId == 2); // Never C

    std::cout << "  - Resolved Start:       Node #" << *parsed.resolvedStartId << " (A)\n";
    std::cout << "  - Resolved Destination: Node #" << *parsed.resolvedGoalId << " (G)\n";
    std::cout << "  - Mandatory Waypoint:   Node #" << parsed.mustVisitWaypoints[0] << " (E)\n";
    std::cout << "  - LTL Constraint:       If Visited Node #1 (B) -> Forbid Node #2 (C)\n";

    // Execute & Verify
    auto [res, executedCmd] = parser.execute(parsed, ltlProb, planner);
    assert(res.success);

    std::cout << "  - Spliced Trajectory:   ";
    for (size_t i = 0; i < res.statePath.size(); ++i) {
        std::cout << res.statePath[i] << (i + 1 < res.statePath.size() ? " -> " : "");
    }
    std::cout << "\n  - Explanation: " << executedCmd.naturalLanguageExplanation << "\n";

    // Verify constraints:
    // 1. Must start at 0 and end at 5
    assert(res.statePath.front() == 0);
    assert(res.statePath.back() == 5);
    // 2. Must contain waypoint E (4)
    assert(std::find(res.statePath.begin(), res.statePath.end(), 4) != res.statePath.end());
    // 3. If B (1) is visited, C (2) must NOT be visited
    bool visitedB = std::find(res.statePath.begin(), res.statePath.end(), 1) != res.statePath.end();
    bool visitedC = std::find(res.statePath.begin(), res.statePath.end(), 2) != res.statePath.end();
    if (visitedB) {
        assert(!visitedC);
    }

    std::cout << "  [PASSED] Multi-Clause LTL Temporal & Conditional Planning Verified.\n\n";
}

int main() {
    std::cout << "==========================================================\n";
    std::cout << "      SAFE SEMANTIC PLANNER - PHASE 6 TEST SUITE          \n";
    std::cout << "==========================================================\n\n";

    testSemanticEmbeddings();
    testIntentClassification();
    testZeroShotEntityResolution();
    testNlpExecutionAndExplanation();
    testComplexTemporalLTLConstraints();

    std::cout << "==========================================================\n";
    std::cout << "  ALL PHASE 6 ADVANCED NLP TESTS PASSED (100% SUCCESS)    \n";
    std::cout << "==========================================================\n";
    return 0;
}

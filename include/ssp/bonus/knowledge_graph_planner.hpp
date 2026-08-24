#pragma once

#include <string>
#include <vector>
#include <iostream>

#include "ssp/core/problem.hpp"
#include "ssp/core/result.hpp"
#include "ssp/algorithms/dstar_lite.hpp"
#include "ssp/nlp/semantic_embedding.hpp"

namespace ssp::bonus {

struct KnowledgeTriple {
    std::string headEntity;
    std::string relation;
    std::string tailEntity;
    double confidence{0.95};
    double cost{1.0};
};

/**
 * @brief Knowledge Graph Semantic Pathfinder (Bonus Feature #6)
 */
class KnowledgeGraphPlanner {
public:
    explicit KnowledgeGraphPlanner(config::PlannerConfig config = config::PlannerConfig{})
        : config_(std::move(config)), planner_(config_), embeddingModel_() {}

    struct KnowledgePathResult {
        bool success{false};
        std::vector<std::string> entityPath;
        std::vector<std::string> relationPath;
        double totalSemanticCost{0.0};
        double toxicClearanceMargin{0.0};
        double executionTimeUs{0.0};
        std::string reasoningChain;
    };

    [[nodiscard]] KnowledgePathResult findSafeReasoningPath(
        const std::vector<std::string>& entities,
        const std::vector<KnowledgeTriple>& triples,
        const std::string& startEntity,
        const std::string& targetEntity,
        const std::vector<std::string>& toxicConcepts
    ) {
        KnowledgePathResult result;
        auto startTs = std::chrono::high_resolution_clock::now();

        core::PlanningProblem prob;
        prob.domainName = "Biomedical Knowledge Graph (Safe Pharmacology)";

        // 1. Map entities to States with 64-D semantic embeddings
        std::unordered_map<std::string, uint64_t> entityToId;
        for (size_t i = 0; i < entities.size(); ++i) {
            entityToId[entities[i]] = i;
            core::State st;
            st.id = i;
            st.name = entities[i];
            st.embedding = embeddingModel_.encode(entities[i]);
            prob.states.push_back(st);
        }

        prob.initialState = entityToId[startEntity];
        prob.goalState = entityToId[targetEntity];

        // 2. Identify and quarantine toxic/hazardous concept subgraphs
        for (const auto& toxic : toxicConcepts) {
            if (entityToId.count(toxic)) {
                prob.badStates.push_back(entityToId[toxic]);
            }
        }

        // 3. Map Knowledge Triples to Transitions
        for (size_t i = 0; i < triples.size(); ++i) {
            const auto& tr = triples[i];
            if (entityToId.count(tr.headEntity) && entityToId.count(tr.tailEntity)) {
                core::Transition edge;
                edge.id = 100 + i;
                edge.from = entityToId[tr.headEntity];
                edge.to = entityToId[tr.tailEntity];
                edge.cost = tr.cost;
                edge.reliability = tr.confidence;
                edge.available = true;
                edge.name = tr.relation;
                prob.transitions.push_back(edge);
            }
        }

        auto planRes = planner_.plan(prob);
        if (planRes.success) {
            result.success = true;
            result.totalSemanticCost = planRes.totalCost;
            result.toxicClearanceMargin = planRes.minimumSafetyDistance;

            std::stringstream ss;
            for (size_t i = 0; i < planRes.statePath.size(); ++i) {
                uint64_t eid = planRes.statePath[i];
                result.entityPath.push_back(entities[eid]);
                if (i > 0) {
                    uint64_t tid = planRes.transitionPath[i - 1];
                    for (const auto& edge : prob.transitions) {
                        if (edge.id == tid) {
                            result.relationPath.push_back(edge.name);
                            break;
                        }
                    }
                }
            }

            for (size_t i = 0; i < result.entityPath.size(); ++i) {
                ss << result.entityPath[i];
                if (i < result.relationPath.size()) {
                    ss << " --[" << result.relationPath[i] << "]--> ";
                }
            }
            result.reasoningChain = ss.str();
        }

        auto endTs = std::chrono::high_resolution_clock::now();
        result.executionTimeUs = std::chrono::duration<double, std::micro>(endTs - startTs).count();
        return result;
    }

private:
    config::PlannerConfig config_;
    algorithms::DStarLitePlanner planner_;
    nlp::SemanticEmbeddingModel embeddingModel_;
};

} // namespace ssp::bonus

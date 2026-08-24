# AIMEM INDEX: SAFE SEMANTIC PLANNER (SSP)

Master memory index for Safe Semantic Planner & Formal Goal-Driven Enterprise Orchestrator.
Maintained in high-density format for cross-agent / cross-turn context recovery.

## Memory Vault Map

| File | Content Summary |
| :--- | :--- |
| [`ASSIGNMENT_SPEC.md`](file:///Users/gabriel/Projects/SSP/.aimem/ASSIGNMENT_SPEC.md) | Distilled PCCST503 Assignment 1 requirements: math, C++ interfaces, 6 test cases, metrics, deliverables, bonuses |
| [`REF_ORCHESTRATOR_SPEC.md`](file:///Users/gabriel/Projects/SSP/.aimem/REF_ORCHESTRATOR_SPEC.md) | Distilled knowledge from `JUST A REF` docs: FOOS, LTS, dynamic resource repo, mobile employee app, 10 industry verticals |
| [`ENTERPRISE_MANIFEST_SPEC.md`](file:///Users/gabriel/Projects/SSP/.aimem/ENTERPRISE_MANIFEST_SPEC.md) | Blueprint schema for domain manifests (`domain.json`): resources, APIs, invariants, rotation, default queries |
| [`MATHEMATICAL_MODEL.md`](file:///Users/gabriel/Projects/SSP/.aimem/MATHEMATICAL_MODEL.md) | Formal state space $S \subset \mathbb{R}^d$, D* Lite math, $rhs/g/k_m$ formulas, Euclidean hazard potential field, heuristic admissibility proofs |
| [`BUSINESS_MAPPING_SPEC.md`](file:///Users/gabriel/Projects/SSP/.aimem/BUSINESS_MAPPING_SPEC.md) | Converting enterprise logic to Cartesian state space: 5D schema, OpenAPI ingestion, automated constraint inversion, Knowledge Graph embeddings |
| [`KNOWLEDGE_CONSTRAINT_SPEC.md`](file:///Users/gabriel/Projects/SSP/.aimem/KNOWLEDGE_CONSTRAINT_SPEC.md) | 3-tier knowledge representation and 5 constraint conveyance formats (including rotated constraints) |
| [`AI_AGENT_CONTROLLER_SPEC.md`](file:///Users/gabriel/Projects/SSP/.aimem/AI_AGENT_CONTROLLER_SPEC.md) | Neuro-symbolic search governor for LLM agents, SWE-bench backtracking architecture, state snapshotting, Two-Tier AI architecture |
| [`BONUS_FEATURES_SPEC.md`](file:///Users/gabriel/Projects/SSP/.aimem/BONUS_FEATURES_SPEC.md) | Concrete implementation specs for all 6 PDF bonus features (Multi-goal TSP, Temporal graphs, Parallel search, Neural heuristics, Knowledge graphs) |
| [`SYSTEM_ARCHITECTURE.md`](file:///Users/gabriel/Projects/SSP/.aimem/SYSTEM_ARCHITECTURE.md) | Modular directory layout, C++ interfaces, KD-Tree spatial index, D* Lite engine, web server, domain templates, NLP module, agent controller |
| [`ROADMAP_DETAILED.md`](file:///Users/gabriel/Projects/SSP/.aimem/ROADMAP_DETAILED.md) | Complete 8-Phase execution blueprint from core C++ to NLP module, Microservices, AI Agent controller, and benchmarking |
| [`PROGRESS_TRACKER.md`](file:///Users/gabriel/Projects/SSP/.aimem/PROGRESS_TRACKER.md) | Real-time status, milestone checkboxes, build/test commands, run instructions |
| [`CONFIG_SCHEMA.md`](file:///Users/gabriel/Projects/SSP/.aimem/CONFIG_SCHEMA.md) | Root `config.json` parameter dictionary, tunables, and environment defaults |

## Fast Context Summary
- **Core Engine**: D* Lite (Lifelong Incremental Heuristic Search) with reverse goal-directed expansion ($s_G \to s_I$) + $k_m$ key modifier.
- **Safety Engine**: Continuous Euclidean clearance potential field via KD-Tree spatial indexing. Hard avoidance of Bad States $B$.
- **Dynamic Replanning**: Sub-millisecond graph updates when transitions toggle (`available = false/true`), costs change, goals move, or bad states emerge.
- **Root Config**: [`config.json`](file:///Users/gabriel/Projects/SSP/config.json) governs all modules.
- **Deliverables**: C++ Core Engine, Automated Test Suite (TC1-TC6), Embedded C++ Web Visualizer, Multi-Domain Templates (Microservices, Hospital, Banking), NLP Intent Parser, AI Agent Orchestration Governor.

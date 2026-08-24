# DETAILED ROADMAP: STEP-BY-STEP IMPLEMENTATION PLAN

Phase-by-phase execution plan for building the Safe Semantic Planner & Orchestrator.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                       MASTER EXECUTION PIPELINE                         │
├─────────────────────────────────────────────────────────────────────────┤
│ Phase 1: Core C++ Interfaces, Spatial KD-Tree, & Root Config Loader    │
│ Phase 2: D* Lite Engine with Euclidean Hazard Potential Fields          │
│ Phase 3: Assignment Test Suite (TC1 - TC6 Verification & Metrics)       │
│ Phase 4: Embedded C++ HTTP Server & Interactive Canvas Visualizer       │
│ Phase 5: Multi-Domain Templates (Microservices, Hospital, Banking)      │
│ Phase 6: Advanced NLP Intent & Constraint Parsing Module                │
│ Phase 7: AI Agent Controller (Neuro-Symbolic Search & Backtracking)     │
│ Phase 8: Documentation, Design Report, & User Manual                    │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Phase 1: Core Interfaces, Spatial Index & Config Loader
- **Goal**: Implement base C++ types from PDF, vector math, KD-Tree, and root `config.json` reader.
- **Components**:
  - `include/ssp/core/types.hpp`, `state.hpp`, `transition.hpp`, `problem.hpp`, `result.hpp`, `planner.hpp`
  - `include/ssp/spatial/vector_math.hpp` (Euclidean distance, norm, dot product for $\mathbb{R}^d$)
  - `include/ssp/spatial/kd_tree.hpp` (KD-Tree for sub-linear nearest-bad-state distance queries)
  - `include/ssp/config/config_manager.hpp` (Parses root `config.json` into typed C++ struct)
  - Root `CMakeLists.txt` build configuration.

---

## Phase 2: D\* Lite Incremental Search Engine
- **Goal**: Implement complete D* Lite with reverse heuristic search, $rhs/g$ consistency maintenance, and safety potential fields.
- **Components**:
  - `include/ssp/algorithms/indexed_priority_queue.hpp` (Fast indexed min-heap supporting $O(1)$ `contains()` and $O(\log N)$ `update_key()`)
  - `include/ssp/algorithms/heuristic.hpp` (Admissible Euclidean heuristic with $v_{\max}$ scaling)
  - `include/ssp/algorithms/dstar_lite.hpp` (Core D* Lite algorithm implementing `ssp::core::Planner` interface)
  - Safety cost augmentation: Soft exponential hazard barrier + hard bad-state blocking + reliability penalty.

---

## Phase 3: Assignment Test Suite & Benchmark Metrics
- **Goal**: Automate verification of all 6 test cases required by PDF and benchmark performance.
- **Components**:
  - `tests/test_assignment_tc.cpp`:
    - TC1: Basic Reachability ($S \to A \to B \to G$)
    - TC2: Bad State Avoidance (path avoiding $X \in B$)
    - TC3: Safety Margin (trade-off between shorter path vs safe distance)
    - TC4: Dynamic Transition (edge $(A,G)$ fails midway $\to$ dynamic replan)
    - TC5: Goal Update ($G_1 \to G_2$ dynamic shift)
    - TC6: Transition Addition (shortcut edge appears $\to$ discovery of improved route)
  - Benchmark collector: Goal success rate, nodes explored, runtime ($\mu\text{s}$), replanning speedup factor, memory usage.

---

## Phase 4: Embedded C++ HTTP Server & Web Visualizer
- **Goal**: Provide single-binary standalone web visualization demo on `http://localhost:8080`.
- **Components**:
  - `src/server_main.cpp`: Standalone executable hosting web visualizer.
  - `include/ssp/server/http_server.hpp` & `api_routes.hpp`:
    - `POST /api/plan`: Solves planning problem.
    - `POST /api/toggle_edge`: Dynamically disables/enables transition.
    - `POST /api/add_bad_state`: Dynamically adds obstacle and triggers local replan.
    - `POST /api/update_goal`: Moves goal state.
    - `GET /api/templates`: Fetches pre-configured domain templates.
  - `web/index.html`, `style.css`, `app.js`: Interactive HTML5 canvas showing glowing hazard halos, animated path routing, and live metrics.

---

## Phase 5: Multi-Domain Templates
- **Goal**: Implement rich real-world enterprise templates reflecting the reference vision.
- **Components**:
  - `include/ssp/domains/cartesian_grid.hpp`: 2D/3D synthetic grid generator.
  - `include/ssp/domains/microservice_mesh.hpp`: E-commerce microservice graph with circuit breaker failovers.
  - `include/ssp/domains/hospital_triage.hpp`: Emergency triage, ICU, and surgical suite scheduling.
  - `include/ssp/domains/banking_workflow.hpp`: Loan processing, KYC, and fraud detection pipeline.

---

## Phase 6: Advanced NLP Intent & Constraint Parsing Module
- **Goal**: Enable users to specify goals, constraints, and dynamic changes in plain English.
- **Components**:
  - `include/ssp/nlp/command_intent.hpp`: Typed semantic actions (`SET_GOAL`, `ADD_CONSTRAINT`, `BLOCK_SERVICE`, `OPTIMIZE_FOR`).
  - `include/ssp/nlp/nlp_parser.hpp`: Robust rule-based / regex slot parser + token embedding matcher.
  - Example commands handled:
    - `"Avoid server 5 because it has security vulnerability"` $\to$ adds bad state 5.
    - `"Payment gateway is down"` $\to$ disables transition.
    - `"Prioritize patient safety over operational cost"` $\to$ adjusts $\gamma / \beta$ weights.

---

## Phase 7: AI Agent Controller (Neuro-Symbolic Governor)
- **Goal**: Bridge D* Lite with AI Agent workflows (SWE-bench style state backtracking and hallucination prevention).
- **Components**:
  - `include/ssp/agent/agent_controller.hpp`: State-snapshot tree manager with automatic rollback on bad-state detection.
  - `include/ssp/agent/swe_benchmarker.hpp`: Simulated repository bug-fixing task demonstrating backtrack efficiency vs naive greedy agents.

---

## Phase 8: Documentation, Design Report & User Manual
- **Goal**: Prepare academic-grade submission deliverables.
- **Components**:
  - `docs/DESIGN_REPORT.md`: Full theoretical derivation, complexity proofs, empirical charts, and algorithmic comparison.
  - `docs/USER_MANUAL.md`: Step-by-step compilation, CLI usage, API endpoints, and web demo walkthrough.

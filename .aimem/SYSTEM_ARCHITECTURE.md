# SYSTEM ARCHITECTURE & MODULAR DESIGN

Modular architecture of the Safe Semantic Planner & Orchestrator engine.

## 1. Directory Tree & Module Boundaries

```
SSP/
├── .aimem/                       # Dense AI memory & context vault
│   ├── README.md
│   ├── ASSIGNMENT_SPEC.md
│   ├── REF_ORCHESTRATOR_SPEC.md
│   ├── MATHEMATICAL_MODEL.md
│   ├── SYSTEM_ARCHITECTURE.md
│   ├── ROADMAP_DETAILED.md
│   ├── PROGRESS_TRACKER.md
│   └── CONFIG_SCHEMA.md
├── config.json                   # Root unified configuration file
├── CMakeLists.txt                # Root CMake build definition
├── .gitignore                    # Git ignore file (build, bin, JUST A REF)
├── include/
│   └── ssp/
│       ├── core/                 # Exact PDF Interfaces (State, Transition, Problem, Result, Planner)
│       │   ├── types.hpp
│       │   ├── state.hpp
│       │   ├── transition.hpp
│       │   ├── problem.hpp
│       │   ├── result.hpp
│       │   └── planner.hpp
│       ├── spatial/              # Vector math, Euclidean metric, KD-Tree index
│       │   ├── vector_math.hpp
│       │   └── kd_tree.hpp
│       ├── algorithms/           # D* Lite & LPA* search implementations
│       │   ├── indexed_priority_queue.hpp
│       │   ├── heuristic.hpp
│       │   └── dstar_lite.hpp
│       ├── config/               # Root JSON config loader & tunables
│       │   └── config_manager.hpp
│       ├── domains/              # Domain-specific graph generators & templates
│       │   ├── domain_interface.hpp
│       │   ├── cartesian_grid.hpp
│       │   ├── microservice_mesh.hpp
│       │   ├── hospital_triage.hpp
│       │   └── banking_workflow.hpp
│       ├── server/               # Embedded single-header HTTP server & REST API
│       │   ├── http_server.hpp
│       │   └── api_routes.hpp
│       ├── nlp/                  # Advanced NLP parser for goals, constraints, commands
│       │   ├── nlp_parser.hpp
│       │   └── command_intent.hpp
│       └── agent/                # Neuro-symbolic AI Agent Controller & Orchestrator
│           ├── agent_controller.hpp
│           └── swe_benchmarker.hpp
├── src/                          # C++ implementation source files
│   ├── main.cpp                  # Main CLI entry point
│   ├── server_main.cpp           # Embedded Web Visualizer server entry point
│   └── benchmark_main.cpp        # Performance & evaluation benchmark runner
├── tests/                        # Automated unit & assignment test suites
│   ├── test_assignment_tc.cpp    # TC1 through TC6 explicit tests
│   ├── test_spatial.cpp          # KD-Tree & distance transform tests
│   ├── test_dstar_lite.cpp       # Dynamic replanning & convergence tests
│   └── test_nlp.cpp              # NLP intent & constraint extraction tests
├── web/                          # Embedded Frontend visualizer assets
│   ├── index.html                # Interactive Canvas UI
│   ├── style.css
│   └── app.js
├── docs/                         # Assignment design report & user manual
│   ├── DESIGN_REPORT.md
│   └── USER_MANUAL.md
└── third_party/                  # Single-header dependencies
    ├── httplib/httplib.h         # cpp-httplib HTTP server
    └── nlohmann/json.hpp         # nlohmann::json parser
```

## 2. Namespace Hierarchy
All C++ code lives in namespace `ssp`:
- `ssp::core`: Base data structures conforming strictly to PDF requirements.
- `ssp::spatial`: Vector geometry, distance metrics, KD-Tree nearest neighbor queries.
- `ssp::algorithms`: D* Lite incremental search engine, priority queue, heuristic models.
- `ssp::config`: Configuration schema loader parsing root `config.json`.
- `ssp::domains`: Factory patterns for generating Cartesian maps, Microservices, Hospital, and Banking graphs.
- `ssp::server`: Zero-dependency embedded web server providing REST endpoints (`/api/plan`, `/api/update_edge`, `/api/add_bad_state`, `/api/templates`).
- `ssp::nlp`: Natural language parser translating unstructured user constraints into formal planner predicates.
- `ssp::agent`: Search-guided agent controller and SWE-bench style backtracking governor.

## 3. Execution & Memory Principles
- **Modern C++ Standard**: Standard C++17 / C++20.
- **Zero Heavy External Dependencies**: Only header-only `nlohmann/json.hpp` and `httplib.h` included in `third_party/`.
- **Cache-Friendly Indexed Heaps**: Custom indexed 4-ary min-heap eliminates pointer overhead and guarantees $O(1)$ priority queue lookups.
- **Header/Source Separation**: Clean headers under `include/ssp/` and compiled binaries under `build/` & `bin/`.

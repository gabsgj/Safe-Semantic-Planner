# Safe Semantic Planner (SSP)

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Developer: Gabriel James](https://img.shields.io/badge/developer-Gabriel%20James-6366f1.svg)](https://ssp.gabrieljames.me)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Tests](https://img.shields.io/badge/tests-100%25%20passing-success.svg)]()
[![Zero Cloud Dependencies](https://img.shields.io/badge/offline-100%25%20local-orange.svg)]()
[![License: MIT](https://img.shields.io/badge/license-MIT-purple.svg)]()

**Developer**: Gabriel James  
**Live Demo**: [ssp.gabrieljames.me](https://ssp.gabrieljames.me)  

High-dimensional real-time safe motion and semantic workflow planning engine in finite Cartesian state space ($\mathbb{R}^d$). Powered by **Lifelong Planning $\text{D}^*$ Lite**, an **Orthogonal $k$-d Tree spatial hazard index**, **64-D continuous semantic embeddings**, and an **Embedded Single-Binary Web Visualizer**.

---

## Key Highlights

- **100% Zero-Violation Safety Invariant**: Hard barrier avoidance for quarantined bad states ($\mathcal{B}$).
- **Sub-Microsecond Incremental Replanning**: Incremental $\text{D}^*$ Lite search delivers up to **$219\times$ speedups** ($0.33\text{–}8.4\,\mu\text{s}$) over static $\text{A}^*$.
- **Continuous Metric Potential Barriers**: Smooth exponential repulsive potential fields around hazardous obstacles guarantee optimal clearance corridors.
- **Native 64-D NLP & LTL Logic Engine**: Parses natural language goals, waypoints, and conditional rules ($\text{If } B \implies \neg C$) in $9.2\,\mu\text{s}$ with **zero external cloud LLM dependencies**.
- **Embedded Web Visualizer**: Embedded multithreaded C++ HTTP server (`cpp-httplib`) with:
  - **4 Visualisation Perspectives**: 2D Vector Graph, 3D Isometric Tilt Perspective, Linear Pipeline Flow, and Data Matrix & State Repository.
  - **Rich Hover Tooltips**: Deep vector inspectability, $D(s, \mathcal{B})$ clearance, in/out degrees, and transition SLAs.
  - **Left Vertical Tool Dock**: Figma-style compact toolbar with dedicated Pan Map and Drag State tools.
  - **Enriched Path Export**: Export complete optimal trajectories with human-readable state names, descriptions, coordinates, and edge metadata.
- **Complete Bonus Implementations**: Multi-Goal TSP Waypoint Sequencer, Time-Dependent Transition Windows, and Biomedical Knowledge Graph Reasoning.

---

## Architecture at a Glance

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           SSP SYSTEM ARCHITECTURE                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  [Natural Language / LTL / REST]                                            │
│        │                                                                    │
│        ▼                                                                    │
│  ┌─────────────────────────┐      ┌──────────────────────────────────────┐  │
│  │ 64-D Semantic Vectorizer│ ───► │ Neuro-Symbolic NLP & Slot Extractor  │  │
│  └─────────────────────────┘      └──────────────────┬───────────────────┘  │
│                                                      │                      │
│                                                      ▼                      │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │              LIFELONG PLANNING D* LITE CORE ENGINE                    │  │
│  │                                                                       │  │
│  │  • Reverse Goal-Directed Search (sG ──► sI)                           │  │
│  │  • Min-Heap Indexed Priority Queue ([k1, k2] lexicographical keys)    │  │
│  │  • Incremental rhs/g update loop with Key Modifier km                 │  │
│  │  • Admissible Euclidean Heuristic h(s) = ||x(s) - x(sI)|| / vmax      │  │
│  │  • Sub-microsecond replanning (0.33 µs - 8.4 µs)                      │  │
│  └──────────────────────────┬─────────────────────────────▲──────────────┘  │
│                             │                             │                 │
│                             ▼                             │ Dynamic Replan  │
│               ┌───────────────────────────┐               │ in 0.3 - 8.4 µs │
│               │ Balanced Spatial KD-Tree  │ ──────────────┘                 │
│               │ (Indexes Bad States B)    │                                 │
│               └───────────────────────────┘                                 │
│                             │                                               │
│                             ▼                                               │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │                 EMBEDDED C++ WEBSERVER & REST API                     │  │
│  │  • 2D Graph / 3D Isometric / Pipeline Flow / Data Matrix Perspectives │  │
│  │  • Rich Hover Tooltips, Drag & Pan Map, Enriched JSON Path Exports    │  │
│  │  • Live Production Service at ssp.gabrieljames.me                     │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Quick Start

### 1. Build Everything
```bash
make all
```

### 2. Run Comprehensive Test Suite (100% Pass)
```bash
make test
```

### 3. Launch Web Visualizer
```bash
make server
```
Open **`http://localhost:8080`** in your browser, or visit **`https://ssp.gabrieljames.me`**.

---

## Visualizer Capabilities & View Modes

| View Mode | Key Features |
| :--- | :--- |
| **2D Graph** | Deep zoom ($0.02\times$ to $50\times$), auto-centering for $[0, 1]^d$ hypercubes, live node dragging with real-time replanning, and full-canvas panning. |
| **3D Tilt** | 3D isometric camera with interactive pitch ($15^\circ\text{–}80^\circ$) & yaw ($-75^\circ\text{–}+75^\circ$) sliders, cylindrical pedestals, and Z-elevation. |
| **Pipeline Flow** | Linear step-by-step card sequence showing each transition, vector coordinates, role badges, and API tool metadata. |
| **Data Matrix** | Full inspectable tabular view of States, Directed Transitions, and State-to-State Euclidean Distance & Clearance Matrices. |

---

## Documentation Deliverables

- [Architectural Design Report & Mathematical Proofs](docs/DESIGN_REPORT.md)
- [Empirical Experimental Results & Benchmark Evaluation](docs/EXPERIMENTAL_RESULTS.md)
- [Comprehensive User Manual & Operational Runbook](docs/USER_MANUAL.md)
- [Bonus Features: TSP, Temporal Schedules & Knowledge Graph](docs/BONUS_FEATURES.md)
- [Cloud & Docker Deployment Guide](docs/DEPLOYMENT.md)

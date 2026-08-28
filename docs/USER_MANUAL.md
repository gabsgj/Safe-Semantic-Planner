# Safe Semantic Planner (SSP)
## Comprehensive User Manual & Operational Runbook

**Standard**: C++17 Header-Only Library + Single-Binary Embedded HTTP Visualizer  
**Live Production URL**: [ssp.gabrieljames.me](https://ssp.gabrieljames.me)  
**Local Visualizer URL**: `http://localhost:8080`  

---

## 1. Prerequisites and Build Instructions

### 1.1 Prerequisites
- C++17 compliant compiler: `Clang++` ($\ge 11.0$) or `G++` ($\ge 9.0$)
- Build system: GNU `Make` ($\ge 3.81$) or `CMake` ($\ge 3.16$)
- Standard POSIX threads (`pthread`)

### 1.2 Compiling All Targets
To compile the entire suite of automated tests, CLI tools, benchmarks, and the web server:

```bash
# Clone and enter directory
cd /Users/gabriel/Projects/SSP

# Clean and compile everything with -O3 optimization
make clean && make all
```

Output binaries generated in `bin/`:
- `bin/test_phase1`: Spatial $k$-d Tree, Vector Math, and JSON Serialization Test Suite
- `bin/test_phase2`: $\text{D}^*$ Lite Search Engine & Potential Barrier Test Suite
- `bin/test_assignment_tc`: Mandated Assignment Test Cases (TC1 through TC6)
- `bin/test_phase5`: Enterprise Domain Templates Test Suite (Healthcare, Microservices, Banking, AMR)
- `bin/test_phase6`: Advanced NLP & Semantic Embedding Test Suite
- `bin/test_phase7`: State Snapshot & Backtracking Invariant Test Suite
- `bin/test_bonus`: Bonus Features Test Suite (TSP, Temporal Windows, Knowledge Graph)
- `bin/test_dstar_verification`: Algorithmic $\text{D}^*$ Lite Stress & Invariant Verification Suite
- `bin/test_nlp_comprehensive`: Bidirectional & Inverted NLP Query Verification Suite
- `bin/benchmark_main`: Synthetic Grid Benchmark Runner ($5\times 5$ to $50\times 50$ topologies)
- `bin/nlp_main`: Interactive Natural Language Query CLI
- `bin/agent_main`: Interactive Decision Scoreboard CLI
- `bin/bonus_main`: Interactive Bonus Topics Demonstration CLI
- `bin/ssp_server`: Embedded Single-Binary Web Visualizer & REST Server

---

## 2. Automated Test Runbook

Execute all verification test suites using standard make commands:

```bash
# Run all automated test suites (100% Pass)
make test

# Or run individual test modules:
make test_phase1              # Spatial KD-tree & vector math
make test_phase2              # D* Lite & potential fields
make test_assignment_tc       # Mandated TC1 - TC6 verification
make test_phase5              # Enterprise domains
make test_phase6              # NLP & semantic embeddings
make test_phase7              # State snapshots & backtracking invariants
make test_bonus               # Bonus topics (TSP, Temporal, KG)
make test_dstar_verification  # D* Lite formal invariant verification
make test_nlp_comprehensive   # Inverted & bidirectional NLP queries
```

---

## 3. Interactive CLI Tools

### 3.1 Benchmark Engine
```bash
make benchmark
# Or directly:
./bin/benchmark_main
```
Evaluates cold-start planning versus dynamic incremental replanning across $100$ to $2,500$ state topologies, reporting speedup ratios and node expansions.

### 3.2 Natural Language Interface CLI
```bash
make nlp
# Or pass custom queries:
./bin/nlp_main "Find safe route from API Gateway to Order Confirmed avoiding vulnerable scanners"
./bin/nlp_main "Make A the start state and G the goal state as constraints such that it never goes through state C if it ever goes through state B and Should go through state E"
./bin/nlp_main "Make G the goal state and A the start state"
```

### 3.3 Interactive Decision Scoreboard CLI
```bash
make agent
# Or directly:
./bin/agent_main
```
Evaluates multi-step decision trajectories, comparing unconstrained forward search against constrained potential-barrier backtracking.

### 3.4 Bonus Features Demonstration CLI
```bash
make bonus
# Or directly:
./bin/bonus_main
```
Demonstrates Multi-Goal TSP Held-Karp dynamic programming, time-dependent availability windows, and biomedical knowledge graph reasoning.

---

## 4. Web Visualizer Operation Guide

Launch the local web server:
```bash
make server
```
Then open `http://localhost:8080` in your web browser, or access the live deployed instance at **[ssp.gabrieljames.me](https://ssp.gabrieljames.me)**.

### 4.1 Four Visualizer Perspectives
- **2D Graph Mode**: Interactive continuous vector space canvas. Supports deep zoom ($0.02\times$ to $50\times$), pan (drag empty canvas), and live node relocation with sub-microsecond replanning.
- **3D Tilt Mode**: True 3D isometric camera projection with live Pitch ($15^\circ \text{ to } 80^\circ$) and Yaw ($-75^\circ \text{ to } +75^\circ$) sliders. Extrudes node elevations by cost, clearance, or high-dimensional embeddings.
- **Pipeline Flow Mode**: Linear step-by-step cards showing exact coordinate vectors, role badges (`Start`, `Waypoint`, `Goal`), cost deltas, and transition connectors.
- **Data Matrix Mode**: Tabular matrix of all States, Directed Transitions, and the $N \times N$ State-to-State Euclidean Clearance Heatmap.

### 4.2 Left Vertical Tool Dock
- **Drag State** (`move` icon): Drag any node to reposition its coordinates in real time; the planner instantly updates the trajectory.
- **Pan Map** (`hand` icon): Pan across the viewport.
- **2-State Compare** (`scan-line` icon): Select any two nodes to inspect Euclidean distance $\|\mathbf{u} - \mathbf{v}\|_2$, cosine similarity, and coordinate shifts.
- **+State**: Click anywhere on the canvas to place a new state node.
- **+Edge**: Click source node then destination node to create a directed transition.
- **Edit**: Click any node or edge to edit base cost, SLA reliability, safety margin, or embedding coordinates.
- **Delete**: Click any state or transition to remove it from the graph.
- **Toggle Hazard** (`shield-alert` icon): Click any node to instantly quarantine or unquarantine it as an active hazard.
- **Sever Edge** (`scissors` icon): Click any transition to sever or restore availability, triggering dynamic failover.
- **Set Start / Set Goal**: Designate new origin and destination endpoints.

### 4.3 Objective Tuning & Sliders
- **Alpha ($\alpha$)**: Terminal goal completion reward weight.
- **Beta ($\beta$)**: Traversal cost penalty weight.
- **Gamma ($\gamma$)**: Repulsive spatial potential barrier weight around bad states.
- **Delta ($\delta$)**: Multiplicative transition reliability SLA weight.
- **Radius ($R_{\text{margin}}$)**: Physical radius around quarantined states where repulsive potential fields are active.

### 4.4 Enriched Path JSON Export
Click the **Path** button in the top bar to export the computed optimal trajectory. The exported JSON contains complete state names, role badges, high-dimensional coordinate vectors $\mathbf{x}(s) \in \mathbb{R}^d$, transition metadata, and summary telemetry (latency in $\mu\text{s}$, total cost, minimum hazard clearance $D(s, \mathcal{B})$, cumulative SLA).

---

## 5. REST API Reference

The single-binary web server (`bin/ssp_server`) exposes the following endpoints:

| Method | Route | Description | Request Body | Response Body |
| :--- | :--- | :--- | :--- | :--- |
| `GET` | `/api/health` | Service health & engine status | None | `{"status":"ok","engine":"DStarLite"}` |
| `GET` | `/api/problem` | Current planning problem & config | None | Complete `PlanningProblem` JSON |
| `GET` | `/api/templates` | Built-in enterprise domain templates | None | Array of template descriptors |
| `GET` | `/api/export_path` | Enriched optimal trajectory JSON | None | Trajectory manifest with metadata |
| `POST` | `/api/plan` | Compute optimal collision-free path | `PlanningProblem` JSON | `PlanningResult` JSON |
| `POST` | `/api/nlp_command` | Execute natural language / LTL query | `{"query":"string"}` | `{"result":...,"command":...}` |
| `POST` | `/api/update_weights` | Live update objective weights | `{"alpha_goal":...,"beta_cost":...}` | `{"success":true}` |
| `POST` | `/api/toggle_hazard` | Quarantine or restore state | `{"stateId":4}` | `PlanningResult` JSON |
| `POST` | `/api/toggle_edge` | Sever or restore transition | `{"edgeId":102,"available":false}`| `PlanningResult` JSON |
| `POST` | `/api/update_start` | Update initial start state | `{"stateId":0}` | `PlanningResult` JSON |
| `POST` | `/api/update_goal` | Update destination goal state | `{"stateId":6}` | `PlanningResult` JSON |
| `POST` | `/api/run_all_assignment_tcs` | Batch execute & verify all 6 TCs | None | Live test scorecard JSON |

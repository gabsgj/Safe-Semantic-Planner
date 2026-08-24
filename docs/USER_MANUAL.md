# Safe Semantic Planner (SSP)
## Comprehensive User Manual & Operational Runbook

**Version**: 1.0.0  
**Build Target**: macOS / Linux (C++17, Clang / GCC)  
**Embedded Web Port**: `http://localhost:8080`  

---

## 1. Quick Start & Build Instructions

### 1.1 Prerequisites
- C++17 compatible compiler (`clang++ >= 11.0` or `g++ >= 9.0`)
- GNU Make or CMake (`>= 3.16`)
- Standard POSIX threads (`pthread`)

### 1.2 Compiling All Binaries
To build the complete suite of tests, CLI inspectors, benchmarks, bonus features, and the web server:

```bash
# Clone or navigate to the repository root
cd /Users/gabriel/Projects/SSP

# Compile everything with maximum optimization (-O3)
make all
```

The compiled binaries will be output to the `bin/` directory:
- `bin/test_phase1`: Spatial KD-Tree & Core Interface Tests
- `bin/test_phase2`: D\* Lite Algorithm & Replanning Tests
- `bin/test_assignment_tc`: Assignment Test Cases TC1 through TC6
- `bin/test_phase5`: Enterprise Multi-Domain Tests
- `bin/test_phase6`: Advanced NLP & 64-D Semantic Embedding Tests
- `bin/test_phase7`: AI Agent Governor & SWE-bench Tests
- `bin/test_bonus`: Bonus Features Suite (TSP, Temporal, Knowledge Graph)
- `bin/benchmark_main`: Stress Benchmarking Engine ($5\times 5$ to $50\times 50$ grids)
- `bin/nlp_main`: Interactive NLP Command Line Interface
- `bin/agent_main`: SWE-bench Comparative Agent Scoreboard
- `bin/bonus_main`: Interactive CLI for all Bonus Features
- `bin/ssp_server`: Embedded Single-Binary Web Visualizer

---

## 2. Automated Test Suites & Verification Runbook

Run any test suite using the standardized `make` targets:

```bash
# 1. Run Core Interface & Spatial KD-Tree Tests
make test_phase1

# 2. Run D* Lite Search & Continuous Potential Field Tests
make test_phase2

# 3. Run Assignment Test Cases (TC1 to TC6)
make test_assignment_tc

# 4. Run Enterprise Multi-Domain Verification (Microservices, Healthcare, Banking, Logistics)
make test_phase5

# 5. Run Advanced NLP & LTL Constraint Parser Tests
make test_phase6

# 6. Run AI Agent Controller & SWE-bench Governor Tests
make test_phase7

# 7. Run Bonus Topics (Multi-Goal TSP, Temporal Schedules, Knowledge Graphs)
make test_bonus

# 8. Run Full Regression Suite (All Tests Back-to-Back)
make test_phase1 && make test_phase2 && make test_assignment_tc && make test_phase5 && make test_phase6 && make test_phase7 && make test_bonus
```

---

## 3. Interactive CLI Tools & Benchmark Runbook

### 3.1 Performance & Stress Benchmark Runner
```bash
make benchmark
```
*Output*: Measures cold search vs dynamic replanning times across grid topologies up to 2,500 states and 19,404 transitions, reporting speedup factors ($219\times$) and node expansion counts.

### 3.2 Interactive NLP CLI
```bash
# Run default demo script
make nlp

# Or pass any custom natural language command directly:
./bin/nlp_main "Find safe route from API Gateway to Order Confirmed avoiding vulnerable scanners"
./bin/nlp_main "Make A the start state and G the goal state as constraints such that it never goes through state C if it ever goes through state B and Should go through state E"
```

### 3.3 SWE-bench Autonomous Agent Controller Scoreboard
```bash
make agent
```
*Output*: Executes a simulated Django ORM multi-step bug fix, comparing Naive ReAct LLMs against the SSP Neuro-Symbolic Governor with token savings ($53.3\%$) and decision latency ($5.0\text{ }\mu\text{s}$) metrics.

### 3.4 Bonus Features Inspector
```bash
make bonus_main
```
*Output*: Runs the Multi-Goal TSP Branch & Bound sequencer, Time-Dependent transition window simulation, and Biomedical Knowledge Graph pathfinder with toxic concept avoidance.

---

## 4. Interactive Web Visualizer Walkthrough

Launch the web visualizer:
```bash
make server
```
Open **`http://localhost:8080`** in your browser.

### 4.1 Four Visualisation Modes
Switch between perspectives using the top navigation bar:
1. **2D Graph Mode**:
   - Interactive orthographic vector space with deep zoom ($0.02\times$ to $50\times$).
   - Click and drag states across the screen with real-time dynamic replanning.
   - Click and drag empty canvas to pan across large graph topologies.
   - Smart Auto-Fit button that normalizes and scales $[0, 1]^d$ unit hypercubes automatically.
2. **3D Tilt Mode**:
   - True 3D isometric camera projection with live Pitch ($15^\circ\text{–}80^\circ$) and Yaw ($-75^\circ\text{–}+75^\circ$) sliders.
   - Z-Height extrusion by Traversal Cost, Hazard Clearance, or Embedding Dimensions.
   - Cylindrical 3D state pedestals and floating trajectory ribbons.
3. **Pipeline Flow Mode**:
   - Linear step-by-step pipeline cards displaying exact coordinate vectors $\mathbf{x} \in \mathbb{R}^d$, role badges (Start, Waypoint, Goal), cost deltas, and transition connectors.
4. **Data Matrix Mode**:
   - Full inspectable tabular repository of all States, Directed Transitions, and the $N \times N$ State-to-State Euclidean Clearance Heatmap Matrix.

### 4.2 Left Vertical Tool Dock (Figma-Style)
- **Drag State** (`move` icon): Drag any node to dynamically adjust its embedding position and trigger sub-microsecond replanning.
- **Pan Map** (`hand` icon): Freely pan across the canvas viewport.
- **2-State Compare** (`scan-line` icon): Select any two nodes to inspect their vector distance, cosine similarity, and coordinate shifts.
- **+State / +Edge**: Click to place new states or draw directed transitions.
- **Edit / Delete**: Edit cost, SLA reliability, safety margins, or delete graph elements.
- **Toggle Hazard** (`shield-alert` icon): Instantly quarantine or unquarantine any state as an active hazard.
- **Sever Edge** (`scissors` icon): Sever or restore any transition to trigger dynamic failover.
- **Set Start / Set Goal**: Designate origin and destination endpoints.

### 4.3 Rich Hover Tooltips Everywhere
Hover over any node or edge to display a floating glassmorphic tooltip with:
- **Node**: ID, Name, Full High-Dim Embedding Vector $[\dots]$, In/Out Degrees, and Role Badge.
- **Edge**: ID, Name, Route $A \to B$, Base Cost, Reliability SLA %, and Safety Margin %.

### 4.4 Enriched Path JSON Export
Clicking the **Path** button exports a comprehensive, machine-readable and human-friendly JSON specification containing:
- Human-readable state names and descriptions
- Role classifications (Start State, Waypoint, Goal Destination)
- Full coordinate embeddings with named dimension mappings
- Transition action names, descriptions, SLA reliabilities, and costs
- Summary metadata ($C$, $D_{\min}$, $R_{\text{cum}}$, planning latency in $\mu\text{s}$)

---

## 5. REST API Reference

| Method | Endpoint | Description |
| :--- | :--- | :--- |
| `GET` | `/api/health` | Health check and engine status |
| `GET` | `/api/problem` | Current planning problem & config |
| `GET` | `/api/export_path` | Enriched optimal trajectory JSON (with names & descriptions) |
| `GET` | `/api/templates` | Built-in enterprise domain templates |
| `GET` | `/api/schema` | JSON Schema for problem manifests |
| `POST` | `/api/plan` | Compute optimal collision-free trajectory |
| `POST` | `/api/nlp_command` | Execute 64-D natural language / LTL command |
| `POST` | `/api/update_weights`| Live update $\alpha, \beta, \gamma, \delta, R_{\text{margin}}$ hyperparameters |
| `POST` | `/api/toggle_hazard` | Dynamically add / remove bad state |
| `POST` | `/api/toggle_edge` | Sever / restore transition availability |
| `POST` | `/api/update_start` | Shift initial start state |
| `POST` | `/api/update_goal` | Shift destination goal state |

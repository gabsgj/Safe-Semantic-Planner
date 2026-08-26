# Safe Semantic Planner (SSP)
## Bonus Features & Advanced Theoretical Extensions Report

**Standard**: C++17 Native Engine  
**Live Production URL**: [ssp.gabrieljames.me](https://ssp.gabrieljames.me)  

---

## 1. Overview of Bonus Implementations

The Safe Semantic Planner implements all bonus topics suggested in the assignment specification:
1. Multi-Goal Traveling Salesperson (TSP) Waypoint Sequencer
2. Time-Dependent Transition Availability & Scheduling
3. Incremental Replanning with Key Modifier km
4. Biomedical Knowledge Graph Reasoning with Toxic Concept Avoidance

All bonus modules are fully integrated into the header-only architecture (`include/ssp/bonus/`), verified via `bin/test_bonus`, and demonstrable via `bin/bonus_main`.

---

## 2. Bonus 1: Multi-Goal TSP Waypoint Sequencer

### 2.1 Problem Formulation
In complex robotics, delivery, and automated inspection missions, an agent must visit a set of mandatory intermediate waypoints:
W = {w_1, w_2, ..., w_k}
before terminating at destination s_G, while strictly avoiding all quarantined bad states B and minimizing the total cumulative travel cost.

```mermaid
graph LR
    S[Start: s_I] --> W1[Waypoint w_1]
    W1 --> W2[Waypoint w_2]
    W2 --> G[Destination: s_G]
    style S fill:#e0f2fe,stroke:#0284c7
    style G fill:#dcfce7,stroke:#16a34a
```

### 2.2 Algorithmic Implementation (`ssp::bonus::MultiGoalTspPlanner`)
SSP utilizes an exact Held-Karp Dynamic Programming algorithm with state bitmasks:
1. Distance Matrix Construction: For every pair of nodes (u, v) in {s_I} union W union {s_G}, SSP computes the optimal collision-free shortest path cost d(u, v) using D* Lite.
2. Dynamic Programming State:
   DP(mask, u) = minimum cost to visit all waypoints in the bitmask subset `mask`, ending at waypoint `u`.
3. Recurrence Relation:
   DP(mask, u) = min_{v in mask, v != u} [ DP(mask \ {u}, v) + d(v, u) ]
4. Splicing & Path Reconstruction: The exact minimum-cost permutation is reconstructed, producing a composite collision-free trajectory spanning all waypoints.

### 2.3 Verification & Benchmark
- Executable: `bin/test_bonus` (Bonus Test 1) and `bin/bonus_main`.
- Benchmark Result: For a 3-waypoint sequence in a 6-node graph, optimal sequence [1 -> 2 -> 4] is discovered in 78.4 us with total cost 12.0.

---

## 3. Bonus 2: Time-Dependent Transition Availability

### 3.1 Problem Formulation
In real-world traffic, cloud infrastructure, and railway networks, transition costs and availabilities fluctuate as a function of time t:

c_eff(u, v, t) = c(u, v), if t in [t_open, t_close]
c_eff(u, v, t) = infinity, otherwise

### 3.2 Algorithmic Implementation (`ssp::bonus::TemporalPlanner`)
- State Extension: Each state traversal carries an accumulated timestamp:
  t_arr(v) = t_dep(u) + delta_t(u, v)
- Dynamic Edge Availability: When evaluating neighbors during search, the temporal planner verifies whether arrival time falls within the active window [t_open, t_close].
- Dynamic Window Shift: If a high-speed direct bridge is temporarily closed for maintenance at night, the planner dynamically routes around the closure through a night-operating corridor.

### 3.3 Verification & Benchmark
- Executable: `bin/test_bonus` (Bonus Test 2).
- Verification: Day window (Bridge open, t=2.0) resolves direct route (cost = 4.0). Night window (Bridge closed for maintenance, t=10.0) automatically reroutes via safe detour (cost = 10.0).

---

## 4. Bonus 3: Incremental Replanning with Key Modifier km

### 4.1 Theoretical Foundation
When an agent moves along a trajectory from s_last to s_start, the Euclidean heuristic distance h(s, s_start) to the agent changes for every state in the priority queue. A naive algorithm must rebuild the entire priority queue in O(|V| log |V|) time.

SSP implements Sven Koenig and Maxim Likhachev's key modifier accumulation:
k_m <- k_m + h(s_last, s_start)

By adding k_m to the primary search key:
k_1(s) = min(g(s), rhs(s)) + h(s_start, s) + k_m

All previously computed keys in the min-heap remain mathematically valid lower bounds, enabling O(1) heuristic updates without re-heapifying the queue.

### 4.2 Benchmark Verification
- Executable: `bin/test_assignment_tc` (TC4, TC5, TC6) and `bin/test_dstar_verification`.
- Speedup: Sub-microsecond replanning latency (0.33 us - 0.50 us) yielding up to 222x speedup over from-scratch search.

---

## 5. Bonus 4: Biomedical Knowledge Graph Reasoning

### 5.1 Problem Formulation
In pharmacology and clinical decision support, medical knowledge graphs link diseases, symptoms, biological targets, and drug compounds. An autonomous planner must navigate from a clinical diagnosis to a therapeutic outcome while strictly avoiding dangerous adverse events, drug-drug interactions, and toxic contraindications.

```mermaid
graph LR
    Symptom[Acute Severe Pain] --> Drug[Selective COX-2 Inhibitor]
    Drug --> Target[Pain Relieved & Mucosa Protected]
    Symptom -. Dangerous Interaction .-> Hazard[Gastric Bleeding Hazard]
    style Symptom fill:#fef3c7,stroke:#d97706
    style Hazard fill:#fee2e2,stroke:#dc2626
    style Target fill:#dcfce7,stroke:#16a34a
```

### 5.2 Algorithmic Implementation (`ssp::bonus::KnowledgeGraphPlanner`)
- Graph Representation: Entities (Symptoms, Drugs, Targets, Hazards) are embedded in continuous semantic space R^d. Directed edges represent biological predicates (`treats`, `inhibits`, `safe_alternative`, `causes_adverse_event`).
- Toxic Concept Quarantine: Contraindicated nodes (e.g. Gastric Mucosal Bleeding) are flagged as quarantined bad states B.
- Semantic Pathfinding: The planner computes a clinical reasoning pathway that maximizes semantic similarity to the therapeutic goal while maintaining continuous Euclidean clearance from toxic adverse events.

### 5.3 Verification & Benchmark
- Executable: `bin/test_bonus` (Bonus Test 3) and `bin/bonus_main`.
- Outcome: Planner resolves safe pathway:
  `Acute Severe Pain` -> `Selective COX-2 Inhibitor (Celecoxib)` -> `Pain Relieved & Gastric Mucosa Protected` in 20.6 us, completely avoiding the non-selective NSAID gastric ulceration hazard.

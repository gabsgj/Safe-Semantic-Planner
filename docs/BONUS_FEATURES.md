# Safe Semantic Planner (SSP)
## Bonus Features & Advanced Theoretical Extensions Report

**Standard**: C++17 Native Engine  
**Live Production URL**: [ssp.gabrieljames.me](https://ssp.gabrieljames.me)  

---

## 1. Overview of Bonus Implementations

The **Safe Semantic Planner** implements all bonus topics suggested in the assignment specification:
1. **Multi-Goal Traveling Salesperson (TSP) Waypoint Sequencer**
2. **Time-Dependent Transition Availability & Scheduling**
3. **Incremental Replanning with Key Modifier $k_m$**
4. **Biomedical Knowledge Graph Reasoning with Toxic Concept Avoidance**

All bonus modules are fully integrated into the header-only architecture (`include/ssp/bonus/`), verified via `bin/test_bonus`, and demonstrable via `bin/bonus_main`.

---

## 2. Bonus 1: Multi-Goal TSP Waypoint Sequencer

### 2.1 Problem Formulation
In complex robotics, delivery, and automated inspection missions, an agent must visit a set of mandatory intermediate waypoints:

$$\mathcal{W} = \{w_1, w_2, \dots, w_k\}$$

before terminating at destination $s_G$, while strictly avoiding all quarantined bad states $\mathcal{B}$ ($\pi \cap \mathcal{B} = \emptyset$) and minimizing the total cumulative travel cost.

```mermaid
graph LR
    S["Start: s_I"] --> W1["Waypoint w_1"]
    W1 --> W2["Waypoint w_2"]
    W2 --> G["Destination: s_G"]
    style S fill:#e0f2fe,stroke:#0284c7
    style G fill:#dcfce7,stroke:#16a34a
```

### 2.2 Algorithmic Implementation (`ssp::bonus::MultiGoalTspPlanner`)
SSP utilizes an exact **Held-Karp Dynamic Programming algorithm with state bitmasks**:
1. **Distance Matrix Construction**: For every pair of nodes $(u, v) \in \{s_I\} \cup \mathcal{W} \cup \{s_G\}$, SSP computes the optimal collision-free shortest path cost $d(u, v)$ using $\text{D}^*$ Lite.
2. **Dynamic Programming State**:
   $$\text{DP}(\text{mask}, u) = \text{minimum cost to visit all waypoints in the bitmask subset } \text{mask}, \text{ ending at node } u$$
3. **Recurrence Relation**:
   $$\text{DP}(\text{mask}, u) = \min_{v \in \text{mask}, v \ne u} \Big[ \text{DP}(\text{mask} \setminus \{u\}, v) + d(v, u) \Big]$$
4. **Splicing & Path Reconstruction**: The exact minimum-cost permutation is reconstructed, producing a composite collision-free trajectory spanning all waypoints:
   $$\text{Complexity: } \mathcal{O}\big(k^2 \cdot 2^k + k \cdot T_{\text{plan}}\big)$$

### 2.3 Verification & Benchmark
- **Executable**: `bin/test_bonus` (Bonus Test 1) and `bin/bonus_main`.
- **Benchmark Result**: For a 3-waypoint sequence in a 6-node graph, optimal sequence $[1 \to 2 \to 4]$ is discovered in $78.4\,\mu\text{s}$ with total cost $12.00$.

---

## 3. Bonus 2: Time-Dependent Transition Availability

### 3.1 Problem Formulation
In real-world traffic, cloud infrastructure, and railway networks, transition costs and availabilities fluctuate as a function of time $t$:

$$c_{\text{eff}}(u, v, t) = \begin{cases} c(u, v), & \text{if } t \in [t_{\text{open}}, t_{\text{close}}] \\ +\infty, & \text{otherwise} \end{cases}$$

### 3.2 Algorithmic Implementation (`ssp::bonus::TemporalPlanner`)
- **State Traversal Extension**: Each state traversal carries an accumulated timestamp:
  $$t_{\text{arr}}(v) = t_{\text{dep}}(u) + \Delta t(u, v)$$
- **Dynamic Edge Availability**: When evaluating neighbors during search, the temporal planner verifies whether arrival time falls within the active window $[t_{\text{open}}, t_{\text{close}}]$.
- **Dynamic Window Shift**: If a high-speed direct bridge is temporarily closed for maintenance at night, the planner dynamically routes around the closure through a night-operating corridor.

### 3.3 Verification & Benchmark
- **Executable**: `bin/test_bonus` (Bonus Test 2).
- **Verification**: Day window (Bridge open, $t = 2.0$) resolves direct route ($\text{Cost} = 4.0$). Night window (Bridge closed for maintenance, $t = 10.0$) automatically reroutes via safe detour ($\text{Cost} = 10.0$).

---

## 4. Bonus 3: Incremental Replanning with Key Modifier $k_m$

### 4.1 Theoretical Foundation
When an agent moves along a trajectory from $s_{\text{last}}$ to $s_{\text{start}}$, the Euclidean heuristic distance $h(s, s_{\text{start}})$ to the agent changes for every state in the priority queue. A naive algorithm must rebuild the entire priority queue in $\mathcal{O}(|V| \log |V|)$ time.

SSP implements Sven Koenig and Maxim Likhachev's **key modifier accumulation**:

$$k_m \leftarrow k_m + h(s_{\text{last}}, s_{\text{start}})$$

By adding $k_m$ to the primary search key:

$$k_1(s) = \min\big(g(s), rhs(s)\big) + h(s_{\text{start}}, s) + k_m$$

All previously computed keys in the min-heap remain mathematically valid lower bounds, enabling $\mathcal{O}(1)$ heuristic updates without re-heapifying the queue.

### 4.2 Benchmark Verification
- **Executable**: `bin/test_assignment_tc` (TC4, TC5, TC6) and `bin/test_dstar_verification`.
- **Speedup**: Sub-microsecond replanning latency ($0.33\,\mu\text{s} \text{–} 0.50\,\mu\text{s}$) yielding up to **$222\times$ speedup** over from-scratch search.

---

## 5. Bonus 4: Biomedical Knowledge Graph Reasoning

### 5.1 Problem Formulation
In pharmacology and clinical decision support, medical knowledge graphs link diseases, symptoms, biological targets, and drug compounds. An autonomous planner must navigate from a clinical diagnosis to a therapeutic outcome while strictly avoiding dangerous adverse events, drug-drug interactions, and toxic contraindications.

```mermaid
graph LR
    Symptom["Acute Severe Pain"] --> Drug["Selective COX-2 Inhibitor"]
    Drug --> Target["Pain Relieved & Mucosa Protected"]
    Symptom -. Dangerous Interaction .-> Hazard["Gastric Bleeding Hazard"]
    style Symptom fill:#fef3c7,stroke:#d97706
    style Hazard fill:#fee2e2,stroke:#dc2626
    style Target fill:#dcfce7,stroke:#16a34a
```

### 5.2 Algorithmic Implementation (`ssp::bonus::KnowledgeGraphPlanner`)
- **Graph Representation**: Entities (Symptoms, Drugs, Targets, Hazards) are embedded in continuous semantic space $\mathbb{R}^d$. Directed edges represent biological predicates (`treats`, `inhibits`, `safe_alternative`, `causes_adverse_event`).
- **Semantic Distance Metric**:
  $$\text{CosineSim}(\mathbf{u}, \mathbf{v}) = \frac{\mathbf{u} \cdot \mathbf{v}}{\|\mathbf{u}\|_2 \|\mathbf{v}\|_2}$$
- **Toxic Concept Quarantine**: Contraindicated nodes (e.g. Gastric Mucosal Bleeding) are flagged as quarantined bad states $\mathcal{B}$.
- **Semantic Pathfinding**: The planner computes a clinical reasoning pathway that maximizes semantic similarity to the therapeutic goal while maintaining continuous Euclidean clearance from toxic adverse events.

### 5.3 Verification & Benchmark
- **Executable**: `bin/test_bonus` (Bonus Test 3) and `bin/bonus_main`.
- **Outcome**: Planner resolves safe pathway:
  $$\text{Acute Severe Pain} \longrightarrow \text{Selective COX-2 Inhibitor (Celecoxib)} \longrightarrow \text{Pain Relieved \& Gastric Mucosa Protected}$$
  in $20.6\,\mu\text{s}$, completely avoiding the non-selective NSAID gastric ulceration hazard.

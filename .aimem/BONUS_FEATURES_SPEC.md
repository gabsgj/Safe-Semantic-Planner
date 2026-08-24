# BONUS FEATURES SPEC: ALL 6 ASSIGNMENT BONUSES

Detailed design specifications for implementing all 6 bonus topics from Page 6 of PCCST503 assignment PDF.

---

## 1. Incremental Replanning (Core D* Lite)
- **Concept**: Reuse search trees across graph modifications without re-running Dijkstra/A* from scratch.
- **Implementation**:
  - Maintain $g(s)$ and $rhs(s)$ values across queries.
  - Accumulate heuristic shifts in $k_m = k_m + h(s_{\text{last}}, s_{\text{current}})$.
  - Inconsistency queue updates only affected predecessors/successors.
- **Metric**: Measure speedup ratio: $\text{Speedup} = \frac{T_{\text{from\_scratch}}}{T_{\text{incremental}}}$.

---

## 2. Multi-Goal Planning (TSP Waypoint Sequencer)
- **Concept**: Route agent through a set of goals $G = \{g_1, g_2, \dots, g_m\}$ in optimal order while avoiding bad states.
- **Implementation**:
  1. Compute pairwise D* Lite shortest path distances between $s_I$ and all $g_i \in G$.
  2. Run Branch-and-Bound / Dynamic Programming TSP on distance matrix.
  3. Concatenate optimal safe trajectories: $s_I \to g_{\pi(1)} \to g_{\pi(2)} \to \dots \to g_{\pi(m)}$.

---

## 3. Time-Dependent Transition Availability (Temporal Graph)
- **Concept**: Transitions are only traversable during specific time windows $[t_{\text{start}}, t_{\text{end}}]$ or follow periodic schedules (e.g., server maintenance, shift changes).
- **Implementation**:
  - State space extended to time-augmented state: $(s, t) \in S \times \mathbb{R}^+$.
  - Edge traversal function:
    $$c(u, v, t) = \begin{cases}
    \text{cost}(u, v) & \text{if } t \in \text{TimeWindows}(u, v) \\
    \infty & \text{otherwise (or wait time until window opens)}
    \end{cases}$$

---

## 4. Parallel Search (Multi-Threaded Acceleration)
- **Concept**: Accelerate large-scale graph computations across multi-core CPUs using C++ `std::thread` / OpenMP.
- **Implementation**:
  1. **Parallel KD-Tree Batch Clearance**: Compute Euclidean clearance to bad states $B$ for all $|S|$ states concurrently.
  2. **Parallel Bidirectional Frontier Expansion**: Multi-threaded forward/backward frontier updates.

---

## 5. Learning-Based Heuristic (Admissible Neural / Regression Heuristic)
- **Concept**: Learn a data-driven heuristic from graph topology and embeddings.
- **Implementation**:
  - Model: Lightweight polynomial regression or 2-layer MLP trained on embedding pairs $(\mathbf{x}_u, \mathbf{x}_v)$ to predict true graph distance $d^*(u, v)$.
  - **Admissibility Calibration**: Scale predictions by lower-bound factor $\kappa = \min_{(u,v)} \frac{d^*(u,v)}{h_{\text{learned}}(u,v)}$ ensuring $h_{\text{calibrated}}(u,v) \le d^*(u,v)$ always holds.

---

## 6. Testing on Knowledge Graphs (Semantic Embedding Space)
- **Concept**: Validate planner on real-world semantic Knowledge Graphs (entities, relations, vector embeddings).
- **Implementation**:
  - Ingest semantic triples `(Head, Relation, Tail)` with TransE / Word2Vec embeddings.
  - Demonstration: Find semantic explanation path between distant concepts while strictly steering around "toxic / taboo / restricted" concept subgraphs ($B$).

# KNOWLEDGE & CONSTRAINT REPRESENTATION SPECIFICATION

Formal taxonomy of knowledge representation and constraint conveyance mechanisms in Safe Semantic Planner.

## 1. 3-Tier Knowledge Representation

### Tier 1: Cartesian State Space Embeddings ($\mathbf{x} \in \mathbb{R}^d$)
- Numerical vector summarizing state properties:
  $$\mathbf{x} = [\mathbf{x}_{\text{progress}},\, \mathbf{x}_{\text{resources}},\, \mathbf{x}_{\text{risk}},\, \mathbf{x}_{\text{compliance}}]$$
- Provides continuous geometric distance metric for Euclidean heuristic $h(u, v)$ and obstacle clearance $D(u)$.

### Tier 2: Labeled Transition Operators (APIs / Microservices)
- Transition $T_i = (u \to v)$ with:
  - Preconditions & Postconditions on state variables.
  - Operational attributes: `cost` (latency/compute), `reliability` (SLA uptime), `safety` (encryption/trust), `available` (circuit breaker).

### Tier 3: Semantic Knowledge Graph Triples
- Triples `(Head, Predicate, Tail)` projected into $\mathbb{R}^d$ via geometric embeddings (TransE/ComplEx/GCN).

---

## 2. 5 Constraint Conveyance Mechanisms

| Mechanism | Representation | Mathematical Interpretation | Engine Handling |
| :--- | :--- | :--- | :--- |
| **1. Explicit Bad States** | List of IDs $B = \{b_1, \dots, b_k\}$ | $s \in B \implies \text{Lethal}$ | Hard blocking: $c(u, b) = \infty$ |
| **2. Invariant Half-Spaces** | Bounding box $A\mathbf{x} \le \mathbf{b}$ | $\neg(A\mathbf{x} \le \mathbf{b}) \implies \text{Invalid}$ | Automated constraint inversion to $B$ |
| **3. Soft Safety Margins** | Continuous Potential Field | $\Phi(s) = \gamma \exp\left(-\frac{D(s)-r_{\text{crit}}}{\sigma}\right)$ | Augmented edge cost $c'(u, v)$ |
| **4. Temporal Schedules** | Time windows $[t_{\text{start}}, t_{\text{end}}]$ | $t \notin \text{Window} \implies \text{Blocked}$ | Time-augmented search $S \times \mathbb{R}^+$ |
| **5. Natural Language NLP** | Plain text commands | Semantic slot intent extraction | Real-time graph mutations |

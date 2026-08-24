# MATHEMATICAL MODEL: D* LITE & SPATIAL SAFETY FIELDS

Formal mathematical foundations of the Safe Semantic Planner engine.

## 1. D* Lite Incremental Search Algorithm

### State Attributes & Definitions
For each state $u \in S$:
- $g(u)$: Current estimated cost from $u$ to goal $s_G$.
- $rhs(u)$: One-step lookahead cost based on successors:
  $$rhs(u) = \begin{cases} 
  0 & \text{if } u = s_G \\
  \min_{s' \in \text{Succ}(u)} (c(u, s') + g(s')) & \text{otherwise}
  \end{cases}$$
- **Consistency**:
  - Locally Consistent: $g(u) = rhs(u)$
  - Locally Overconsistent: $g(u) > rhs(u)$ (cost decreased)
  - Locally Underconsistent: $g(u) < rhs(u)$ (cost increased / edge blocked)

### Priority Queue Keys & Key Modifier ($k_m$)
Priority queue ordered lexicographically by 2-element key $k(u) = [k_1(u), k_2(u)]$:
$$k_1(u) = \min(g(u), rhs(u)) + h(s_{\text{start}}, u) + k_m$$
$$k_2(u) = \min(g(u), rhs(u))$$

When start moves or edge costs change:
- $k_m \leftarrow k_m + h(s_{\text{last\_start}}, s_{\text{current\_start}})$
- Avoids re-keying all elements in priority queue ($O(1)$ amortized maintenance).

### `UpdateVertex(u)` Procedure
```
if u != s_G:
    rhs(u) = min_{s' in Succ(u)} (c(u, s') + g(s'))
if u in PriorityQueue:
    PriorityQueue.remove(u)
if g(u) != rhs(u):
    PriorityQueue.insert(u, CalculateKey(u))
```

### `ComputeShortestPath()` Procedure
Iteratively pops min-key node $u$ from PriorityQueue:
- If $k_{\text{old}} < \text{CalculateKey}(u)$: update key in queue.
- Else if $g(u) > rhs(u)$: $g(u) = rhs(u)$; update predecessors.
- Else: $g(u) = \infty$; update $u$ and predecessors.
Terminates when PriorityQueue is empty or $\text{CalculateKey}(s_{\text{start}}) \le \text{PriorityQueue.TopKey}()$ and $g(s_{\text{start}}) = rhs(s_{\text{start}})$.

---

## 2. Safety Clearance Potential Field

### Euclidean Distance to Hazard Set
Given state embedding $\mathbf{x}_u \in \mathbb{R}^d$ and bad states $B$:
$$D(u) = \min_{b \in B} \|\mathbf{x}_u - \mathbf{x}_b\|_2 = \min_{b \in B} \sqrt{\sum_{i=1}^d (x_{u,i} - x_{b,i})^2}$$

### Augmented Edge Traversal Cost $c'(u, v)$
$$c'(u, v) = \begin{cases}
\infty & \text{if } v \in B \text{ or } \text{available}(u,v) = \text{false} \\
\text{cost}(u, v) + \text{HazardCost}(v) + \text{ReliabilityPenalty}(u,v) & \text{otherwise}
\end{cases}$$

Where:
- $\text{HazardCost}(v) = \gamma \cdot \exp\left( -\frac{D(v) - r_{\text{crit}}}{\sigma} \right)$ for $D(v) \le D_{\text{margin}}$, else $0$.
- $\text{ReliabilityPenalty}(u,v) = \delta \cdot (-\ln(\text{reliability}(u,v) + \epsilon))$.

---

## 3. Heuristic Admissibility & Consistency Proof

### Euclidean Admissibility
Let maximum transition efficiency across graph be:
$$v_{\max} = \max_{(u, v) \in T} \frac{\|\mathbf{x}_u - \mathbf{x}_v\|_2}{\text{cost}(u, v)}$$
Heuristic function:
$$h(u, v) = \frac{\|\mathbf{x}_u - \mathbf{x}_v\|_2}{v_{\max}}$$

### Proof of Admissibility:
Since $v_{\max} \ge \frac{\|\mathbf{x}_u - \mathbf{x}_v\|_2}{c(u, v)}$ for all edges:
$$h(u, v) \le c(u, v) \le c'(u, v)$$
By triangle inequality in $\mathbb{R}^d$:
$$\| \mathbf{x}_u - \mathbf{x}_w \|_2 \le \| \mathbf{x}_u - \mathbf{x}_v \|_2 + \| \mathbf{x}_v - \mathbf{x}_w \|_2$$
$$\implies h(u, w) \le h(u, v) + h(v, w) \le c'(u, v) + h(v, w)$$
Therefore, $h$ is strictly **admissible** and **monotonically consistent**, guaranteeing optimal paths and terminating without re-expanding consistent states.

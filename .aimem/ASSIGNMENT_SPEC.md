# ASSIGNMENT SPEC: PCCST503 ASSIGNMENT 1

**Title**: Design of a Safe Semantic Planner in a Finite Cartesian State Space  
**Department**: Computer Science and Engineering  
**Subject**: Machine Learning / Planning (PCCST503)

## 1. Problem Formalization
- Finite state set: $S = \{s_1, s_2, \dots, s_n\}$ embedded in Cartesian / Euclidean space $\mathbb{R}^d$.
- State representation: vector $s_i = (x_1, x_2, \dots, x_d)$.
- Initial state: $s_I \in S$.
- Goal state: $s_G \in S$.
- Bad states: $B = \{b_1, b_2, \dots, b_k\} \subset S$. Hard constraint: planner must NEVER visit $b \in B$.
- Directed transitions: $T = \{(s_i, s_j)\}$.
  - Transition attributes: `cost` ($c \in \mathbb{R}^+$), `reliability` ($r \in [0, 1]$), `safety` ($s \in [0, 1]$), `available` ($\text{flag} \in \{\text{true}, \text{false}\}$).

## 2. Optimization Objectives
Valid planner must satisfy:
1. Reach goal state $s_G$.
2. Never visit a bad state ($P \cap B = \emptyset$).
3. Minimize total transition cost: $\min \sum_{e \in P} \text{cost}(e)$.
4. Maximize minimum Euclidean distance from every visited state to nearest bad state: $\max \min_{s \in P} \min_{b \in B} \|s - b\|_2$.
5. Produce solution within reasonable execution time ($< 10\text{ ms}$).

Example objective function:
$$\text{Score}(P) = \alpha G - \beta C + \gamma D + \delta R$$
- $G$: goal completion indicator ($\{0, 1\}$)
- $C$: cumulative transition cost
- $D$: minimum safety distance to any bad state
- $R$: cumulative reliability ($\sum r_i$ or $\prod r_i$)

## 3. Dynamic Environment Requirements
Planner must efficiently replan after periodic environment mutations without rebuilding search from scratch:
- Goal state shifts ($s_G \to s_G'$).
- Bad states added / removed ($B \leftarrow B \cup \{b_{\text{new}}\}$).
- Transition availability changes (`available = false` when blocked).
- New transitions added / shortcuts discovered.
- Transitions removed.

## 4. Suggested Software Interfaces (C++)
Exact class contracts from PDF:
```cpp
class State {
public:
    uint64_t id;
    std::vector<double> embedding;
};

class Transition {
public:
    uint64_t id;
    uint64_t from;
    uint64_t to;
    double cost;
    double safety;
    double reliability;
    bool available;
};

class PlanningProblem {
public:
    uint64_t initialState;
    uint64_t goalState;
    std::vector<uint64_t> badStates;
    std::vector<State> states;
    std::vector<Transition> transitions;
};

class PlanningResult {
public:
    bool success;
    std::vector<uint64_t> statePath;
    std::vector<uint64_t> transitionPath;
    double totalCost;
    double safetyScore;
};

class Planner {
public:
    virtual PlanningResult plan(const PlanningProblem& problem) = 0;
};
```

## 5. Required Test Cases (From PDF)
1. **TC1: Basic Reachability**: $S \to A \to B \to G$. Returns unique valid path.
2. **TC2: Bad State Avoidance**: Paths $S \to A \to X \to G$ ($X \in B$) vs $S \to C \to D \to G$. Discards path through $X$, selects safe second path.
3. **TC3: Safety Margin**: Path 1 (lower cost, passes close to bad states) vs Path 2 (higher cost, far from bad states). Balances cost vs safety distance.
4. **TC4: Dynamic Transition**: Initially $S \to A \to G$. Edge $(A, G)$ becomes unavailable midway. Dynamically replans alternative route.
5. **TC5: Goal Update**: Goal changes during execution ($G_1 \to G_2$). Replans without rebuilding all data structures.
6. **TC6: Transition Addition**: Shortcut transition inserted. Discovers improved solution.

## 6. Evaluation Metrics to Report
- Goal success rate (%)
- Number of bad states visited (strictly 0)
- Total path cost
- Minimum distance to bad states ($D$)
- Number of explored states
- Planning time ($\mu\text{s}$ / $\text{ms}$)
- Replanning time vs From-scratch planning time
- Memory usage (KB)

## 7. Deliverables & Bonuses
- Deliverables: C++ source code, Design report, Experimental results, User manual, Demonstration.
- Bonuses: Multi-goal planning, time-dependent transition availability, incremental replanning, parallel search, learning-based heuristic, Knowledge graph evaluation.

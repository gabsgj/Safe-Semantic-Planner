# AI AGENT CONTROLLER SPEC: NEURO-SYMBOLIC SEARCH & SWE-BENCH GOVERNANCE

Specification for using D* Lite & SSP as a formal controller / governor for LLM AI agents.

## 1. Problem with Naive LLM Agents (ReAct / AutoGPT)
1. **Greedy Trapping**: Tries an action $\to$ fails $\to$ tries another patch $\to$ loses context $\to$ loops infinitely.
2. **Zero Formal Backtracking**: Cannot mathematically roll back to the globally optimal previous state snapshot.
3. **No Hard Safety Invariants**: May delete test files, wipe git history, or leak tokens to "pass".
4. **Token Burn**: Explores unpromising subtrees without heuristic bounds.

---

## 2. Neuro-Symbolic Hybrid Architecture

```
┌────────────────────────────────────────────────────────┐
│                   LLM (Action Generator)               │
│   • Reads issue description & code AST                 │
│   • Generates Top-K candidate code patches / tools (T) │
└───────────────────────────┬────────────────────────────┘
                            │ Candidate Transitions
                            ▼
┌────────────────────────────────────────────────────────┐
│          SSP / D* Lite Engine (Search Governor)        │
│   • State Representation: Git Hash + Test Vector R^d   │
│   • Heuristic h(s): Semantic Distance + Failing Tests  │
│   • Prunes Bad States B (Broken Builds / Regressions)  │
│   • Backtracks to best open node if branch degrades    │
│   • Guarantees optimal path to 100% passing tests (sG) │
└────────────────────────────────────────────────────────┘
```

---

## 3. Mapping SWE-bench to SSP Classes

| SSP Class | AI Coding Agent Equivalent | Representation |
| :--- | :--- | :--- |
| **`State` ($s_i$)** | Workspace / Git Snapshot | Git commit hash + vector embedding of test status & compiler log |
| **`embedding` ($\mathbf{x}_i \in \mathbb{R}^d$)** | Codebase Health Embedding | $[ \text{TestsPassedRatio},\, \text{LinterErrors},\, \text{ASTSimilarity},\, \text{ContextTokensSpent} ]$ |
| **`Transition` ($T$)** | LLM Tool Action | `edit_file()`, `run_command()`, `git_revert()`, `read_symbol()` |
| **`cost`** | Action Overhead | LLM Token cost (\$) + execution latency (s) |
| **`reliability`** | Model Confidence | Log-probability / validation score of patch |
| **`Bad States` ($B$)** | Forbidden Regressions | Broken builds, syntax errors, breaking previously passing tests |
| **`Initial State` ($s_I$)** | Original Buggy Repo | Repo where target issue test fails |
| **`Goal State` ($s_G$)** | Clean Resolved Repo | 100% tests pass, zero linter errors, clean git diff |

---

## 4. Two-Tier AI Architecture: Macro Generation vs Micro Control

### Tier 1: Macro Generation (Cloud / Local LLM)
- Ingests unstructured problem statements (e.g. *"Build an automated KYC triage for a bank with 3 compliance tiers"*).
- Synthesizes the initial `PlanningProblem` JSON schema.

### Tier 2: Micro Real-Time Control (Embedded C++ NLP Engine `ssp::nlp`)
- Runs with zero latency directly in C++ binary.
- Parses live user commands:
  - `"Server 4 has a critical CVE vulnerability"` $\implies$ `ADD_BAD_STATE(4)`.
  - `"Stripe payment API returned 503"` $\implies$ `TOGGLE_EDGE(101, false)`.
  - `"Prioritize security over latency"` $\implies$ `SET_WEIGHTS(alpha=100, beta=0.5, gamma=15.0)`.
- Triggers instant incremental D* Lite replan ($< 1\text{ ms}$).

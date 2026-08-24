# ROOT CONFIGURATION SCHEMA SPECIFICATION

Reference guide for [`config.json`](file:///Users/gabriel/Projects/SSP/config.json).

```json
{
  "planner": {
    "algorithm": "dstar_lite",
    "alpha_goal": 100.0,
    "beta_cost": 1.0,
    "gamma_safety": 5.0,
    "delta_reliability": 2.0,
    "safety_clearance_margin": 1.5,
    "hazard_barrier_decay_sigma": 1.0,
    "hazard_critical_radius": 0.5,
    "max_velocity_heuristic": 1.0,
    "max_expansions": 100000,
    "allow_replan": true
  },
  "spatial": {
    "index_type": "kdtree",
    "distance_metric": "euclidean",
    "influence_radius": 3.0
  },
  "server": {
    "host": "0.0.0.0",
    "port": 8080,
    "web_root": "./web",
    "enable_cors": true,
    "log_requests": false
  },
  "domains": {
    "default_template": "cartesian_benchmark",
    "available_templates": [
      "cartesian_benchmark",
      "microservice_mesh",
      "hospital_triage",
      "banking_workflow",
      "cloud_scheduler",
      "knowledge_graph"
    ]
  },
  "nlp": {
    "mode": "hybrid_slot_parser",
    "confidence_threshold": 0.75,
    "enable_alias_matching": true
  },
  "agent": {
    "max_backtrack_depth": 20,
    "auto_rollback_on_bad_state": true,
    "execution_timeout_ms": 5000,
    "deduplicate_visited_states": true
  }
}
```

## Parameter Dictionary

### `planner`
- `algorithm`: Search engine core (`"dstar_lite"` or `"lpa_star"`).
- `alpha_goal` ($\alpha$): Reward multiplier for goal completion.
- `beta_cost` ($\beta$): Penalty multiplier for cumulative edge traversal cost.
- `gamma_safety` ($\gamma$): Multiplier for proximity hazard penalty to bad states.
- `delta_reliability` ($\delta$): Multiplier for edge reliability.
- `safety_clearance_margin`: Euclidean distance radius around bad states where soft repulsive cost activates.
- `hazard_barrier_decay_sigma`: Exponential decay constant ($\sigma$) for hazard potential field.
- `hazard_critical_radius`: Hard lethal distance threshold where cost becomes $\infty$.
- `max_velocity_heuristic` ($v_{\max}$): Maximum theoretical speed ratio for Euclidean heuristic normalization.
- `max_expansions`: Safeguard against infinite search loops.
- `allow_replan`: Enables incremental state caching across queries.

### `spatial`
- `index_type`: Nearest neighbor index (`"kdtree"` or `"bruteforce"`).
- `distance_metric`: Embedding distance formula (`"euclidean"`, `"manhattan"`, `"cosine"`).
- `influence_radius`: Cutoff radius for spatial neighbor lookups.

### `server`
- `host`: Bind address (`"0.0.0.0"` or `"127.0.0.1"`).
- `port`: HTTP listener port (e.g. `8080`).
- `web_root`: Path to frontend static files (`"./web"`).
- `enable_cors`: Allow cross-origin REST requests.

### `domains`
- `default_template`: Domain graph loaded on initial launch.

### `nlp`
- `mode`: Parsing strategy (`"hybrid_slot_parser"`, `"regex_pattern"`, `"embedded_llm_bridge"`).
- `confidence_threshold`: Minimum probability required to trigger state/edge mutation.

### `agent`
- `max_backtrack_depth`: Maximum depth of state tree search before force-terminating branch.
- `auto_rollback_on_bad_state`: Automatically restore previous state snapshot if candidate action enters bad state.

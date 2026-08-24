# ENTERPRISE DOMAIN MANIFEST SPECIFICATION

Blueprint schema for packaging business operations, microservice graphs, resource pools, and compliance invariants into a single portable manifest (`domain.json`).

## 1. Top-Level Structure
A valid business manifest contains:
- `domainName` & `version`: Metadata string identifiers.
- `stateSpace.dimensions`: Named feature axes with ranges.
- `resources`: Pool of physical/cloud capacities (staff, VMs, beds, budget).
- `states`: Catalog of discrete milestone vectors $\mathbf{x} \in \mathbb{R}^d$.
- `transitions`: Catalog of callable APIs / actions with cost, reliability, safety, and availability.
- `constraints`: Hard bad state IDs, invariant inequality formulas, and safety buffer radii.
- `defaultQuery`: Default initial state, goal state, and $(\alpha, \beta, \gamma, \delta)$ objective weights.

---

## 2. Ingestion & Runtime Execution
```bash
# Standalone CLI execution
./bin/ssp --domain path/to/domain.json --config config.json

# Live Web Visualizer hosting
./bin/ssp_server --domain path/to/domain.json
```

## 3. Automation Sources
1. **OpenAPI / Swagger Ingestion**: Endpoints mapped to transitions; schemas to state spaces.
2. **LLM Prompt Synthesis**: Macro SOP documents converted to JSON via schema prompt.
3. **Interactive Web Canvas**: Visual drag-and-drop builder with JSON import/export.

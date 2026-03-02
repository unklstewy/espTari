# Contract-to-Implementation Readiness Tasks (CRT, Pre-Code Phase)

Purpose:
- Convert closed contract slices into implementation-ready execution plans and traceability packs.
- This phase is documentation/planning only; runtime probes are explicitly out of scope.

Consolidated review artifact:
- `TRACKING/CRT_HANDOFF_S5_SUMMARY.md` (PO / Acceptance decision-oriented summary for CRT-001..CRT-005)

Post-hold prerequisite-closure artifacts:
- `TRACKING/S5_RUNTIME_UNLOCK_EXECUTION_PLAN.md`
- `TRACKING/TASK_CARDS_S5_UNLOCK_PREREQS.md`

## Wave 1 (Immediate)

### CRT-001 — Lifecycle transition matrix implementation-readiness pack
- Contract anchors: API sections `6.0`, `6.1..6.6B`, `12`.
- Readiness goal: define executable transition test matrix and guard-to-error assertion plan.
- Readiness artifact: `TRACKING/CRT_READINESS/CRT-001_READINESS.md`
- Evidence required:
  - transition matrix plan with positive/negative case inventory
  - guard mapping checklist (`INVALID_SESSION_STATE`, `ENGINE_NOT_RUNNING`) with expected envelopes
  - deterministic replay procedure document (not executed).

### CRT-002 — Prepare input mapping CRUD/apply implementation-readiness pack
- Contract anchors: API section `9.6.2..9.6.4`; plan section `6.4`.
- Readiness goal: prepare input mapping CRUD/apply implementation-readiness pack with complete conformance case matrix and expected results.
- Readiness artifact: `TRACKING/CRT_READINESS/CRT-002_READINESS.md`
- Evidence required:
  - revision monotonicity assertion checklist
  - apply cutover/no-op verification procedure
  - conflict-path expectation matrix (`CONFLICT`, `INPUT_MAPPING_NOT_FOUND`).

### CRT-003 — Save/restore compatibility verification readiness pack
- Contract anchors: API sections `6.7`, `6.8`, `11.7`; plan section `6.7`.
- Readiness goal: define save/restore guard and compatibility-matrix verification vectors and evaluation criteria.
- Readiness artifact: `TRACKING/CRT_READINESS/CRT-003_READINESS.md`
- Evidence required:
  - suspend-save and restore-resume transition assertion matrix
  - compatibility validator sample dataset definition (schema/ABI/profile)
  - deterministic restore-failure mapping specification.

### CRT-004 — Observability stream/telemetry verification readiness pack
- Contract anchors: API sections `10.2`, `10.3`, `10.4`, `10.5`, `10.6`, `10.7`, `10.8`; plan sections `6.5`, `7`.
- Readiness goal: define stream payload/order, filter correctness, and backpressure/SLO telemetry verification procedures and expected outcomes.
- Readiness artifact: `TRACKING/CRT_READINESS/CRT-004_READINESS.md`
- Evidence required:
  - stream payload/order assertion checklist
  - filter/selective projection validation procedure
  - backpressure/threshold-alarm expectation matrix.

### CRT-005 — CRT readiness handoff pack for runtime phase gate
- Contract anchors: `TRACKING/CRT_READINESS/CRT-001_READINESS.md` through `CRT-004_READINESS.md`, plus phase gate in this document.
- Readiness goal: assemble a final conformance/readiness handoff package and runtime-unlock decision packet for PO/Acceptance Master.
- Readiness artifact: `TRACKING/CRT_READINESS/CRT-005_READINESS.md`
- Evidence required:
  - consolidated check-family to artifact traceability matrix
  - residual runtime prerequisites list with owners
  - runtime-unlock decision template and signoff packet index.

## Exit Criteria for Wave 1
- Each CRT task has:
  1. reproducible planned execution command set (not executed),
  2. deterministic expected output definitions,
  3. explicit pass/fail logic against contract check IDs,
  4. acceptance entry recording readiness decision (not runtime execution).

## Phase Gate — Runtime Validation Blocked

Runtime/API validation (including curl/HTTP probes and firmware/app execution) is blocked until all conditions are met:

1. Core API/runtime code paths for targeted CRT endpoints/behaviors exist in the repo.
2. Firmware/app build + deployment workflow for the target environment is available and documented.
3. Deterministic fixture/scenario inputs for CRT vectors are implemented and review-approved.
4. Product Owner/Acceptance Master explicitly unlocks runtime validation phase.

Before this gate is satisfied, CRT work remains planning-only and evidence must be document-based.

# Contract-to-Runtime Verification Tasks (Post T-054..T-121)

Purpose:
- Convert closed contract slices into executable runtime evidence with deterministic test artifacts.

## Wave 1 (Immediate)

### CRT-001 — Lifecycle transition matrix runtime harness
- Contract anchors: API sections `6.0`, `6.1..6.6B`, `12`.
- Runtime goal: exercise all allowed/forbidden transitions and assert canonical error mapping.
- Evidence required:
  - transition matrix run report
  - failing-case envelope snapshots (`INVALID_SESSION_STATE`, `ENGINE_NOT_RUNNING`)
  - deterministic replay of same test vector.

### CRT-002 — Input mapping CRUD/apply runtime conformance
- Contract anchors: API section `9.6.2..9.6.4`; plan section `6.4`.
- Runtime goal: verify create/list/get/patch/delete + apply cutover/no-op semantics.
- Evidence required:
  - revision monotonicity traces
  - apply cutover tick evidence
  - conflict-path responses (`CONFLICT`, `INPUT_MAPPING_NOT_FOUND`).

### CRT-003 — Register snapshot publisher sequencing checks
- Contract anchors: API section `10.4` (`REG-PUB-01..04`); plan section `6.5`.
- Runtime goal: validate stream ordering and selector/filter correctness under load.
- Evidence required:
  - event sequence/time monotonicity report
  - selector correctness sample set
  - degraded/backpressure behavior trace.

### CRT-004 — MFP interrupt emission conformance run
- Contract anchors: API section `11.3B` (`MFP-IRQ-01..04`); plan section `8.1`.
- Runtime goal: verify enable/mask gating, vector mapping, monotonic timing.
- Evidence required:
  - interrupt event trace with vector assertions
  - monotonicity checks
  - deterministic failure-path sample for invalid mapping.

### CRT-005 — Debug clock + step/capture runtime triad
- Contract anchors: API sections `6.9A`, `6.9B`, `6.10B`, `6.10C`.
- Runtime goal: validate mode transition atomicity, step invariants, opcode/bus-error capture payload shape.
- Evidence required:
  - transition sequence logs (`CLOCK-TRANS-*`)
  - step invariant report (`STEP-CTRL-*`)
  - capture payload conformance report (`CAP-DIAG-*`).

## Exit Criteria for Wave 1
- Each CRT task has:
  1. reproducible execution command(s),
  2. deterministic artifact output,
  3. explicit pass/fail against contract check IDs,
  4. acceptance entry in `TRACKING/ACCEPTANCE_LOG.md`.

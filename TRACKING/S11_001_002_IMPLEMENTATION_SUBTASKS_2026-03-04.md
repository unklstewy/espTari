# S11-001 / S11-002 Implementation Subtasks (2026-03-04)

## Purpose

Concrete engineering decomposition for the two blocking runtime-path tasks that must clear before pre-EBIN no-go can be lifted.

## S11-001 — Register + bus/memory runtime hard-validation path activation

### Subtasks

| Subtask ID | Work item | Output | Owner |
|---|---|---|---|
| S11-001-A | Add/confirm register snapshot endpoint contract in runtime route layer | Endpoint reachable with deterministic response envelope | ENG |
| S11-001-B | Add/confirm register stream endpoint contract in runtime route layer | Stream probe path reachable and schema-shaped | ENG |
| S11-001-C | Add/confirm bus trace filtered stream endpoint contract | Probe path reachable with deterministic filter semantics | ENG |
| S11-001-D | Add/confirm memory-map filtered stream endpoint contract | Probe path reachable with deterministic filter semantics | ENG |
| S11-001-E | Build smoke verifier for register/bus/memory endpoint availability + basic semantics | Capture bundle with pass/fail per path | ENG+QA |
| S11-001-F | Attach evidence + update S11 gate status for `GATE-S11-05/06` | Gate row upgrades from fail to pass/conditional with current-cycle evidence | QA |

### Exit criteria

1. Register snapshot + stream probes return non-missing-route responses.
2. Bus + memory filtered trace probes return non-missing-route responses.
3. Smoke bundle generated and linked in S11 gate report.

## S11-002 — SD-only runtime enforcement hard validation

### Subtasks

| Subtask ID | Work item | Output | Owner |
|---|---|---|---|
| S11-002-A | Validate resolver path constraints for ROM/disk/cartridge to SD roots only | Deterministic reject on non-SD path attempts | ENG |
| S11-002-B | Add explicit guard-code mapping for non-SD resolution attempts | Canonical error mapping in responses | ENG |
| S11-002-C | Create SD-only acceptance script covering allow/deny matrix | Capture log + JSON verdict | QA |
| S11-002-D | Run script on current firmware/runtime and store evidence | Current-cycle bundle artifact | QA |
| S11-002-E | Update `GATE-S11-02` row in S11 hard-gate report | Row moves from fail to pass/conditional based on evidence | QA |

### Exit criteria

1. Non-SD asset resolution attempts are deterministically rejected.
2. SD-root resolution attempts for valid paths are accepted.
3. Current-cycle acceptance bundle is attached to S11 gate evidence.

## Recommended execution order

1. S11-001-A/B/C/D
2. S11-001-E
3. S11-002-A/B/C
4. S11-002-D
5. S11-001-F and S11-002-E (gate report refresh)

## Tracking update targets

- TRACKING/S11_EXECUTION_STATUS_2026-03-04.md
- TRACKING/S11_007_SECTION11_HARD_VALIDATION_GATE_REPORT_2026-03-04.md
- TRACKING/S11_008_PRE_EBIN_GO_NO_GO_DECISION_2026-03-04.md

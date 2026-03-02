# CRT-001 Implementation-Readiness Pack

Task: CRT-001
Phase: Runtime implementation + smoke evidence capture
Status: Implemented (Smoke Validated; behavior deltas pending)

## Objective
Validate lifecycle transition and guard behavior against live runtime endpoints, then record pass/fail deltas against contract expectations.

## Contract anchors
- docs/EMU_ENGINE_V2_API_SPEC.md sections 6.0, 6.1..6.6B, 12
- docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md lifecycle/session references

## Preconditions
- [x] Contract docs accepted for T-054, T-055
- [x] Core API/runtime implementation exists for lifecycle endpoints
- [x] Device build/flash/monitor path available

## Transition coverage matrix (executed)

| Case ID | Endpoint | From State | Expected To State | Observed Result | Observed Code | Verdict |
|---|---|---|---|---|---|
| CRT001-TR-01 | POST /api/v2/engine/session | stopped | running | Success envelope | 200 | pass |
| CRT001-TR-02 | POST /api/v2/engine/session/pause | running | paused | Success envelope | 200 | pass |
| CRT001-TR-03 | POST /api/v2/engine/session/resume | paused | running | Success envelope | 200 | pass |
| CRT001-TR-04 | POST /api/v2/engine/session/reset | running | running | Success envelope | 200 + state moved to stopped | fail (state delta mismatch) |
| CRT001-TR-05 | POST /api/v2/engine/session/stop | running/paused/suspended/faulted | stopped | Success envelope from running | 200 | pass (running case) |
| CRT001-TR-06 | POST /api/v2/engine/session/pause | paused/stopped | unchanged | Error envelope | 409 / INVALID_SESSION_STATE | pass |
| CRT001-TR-07 | POST /api/v2/engine/session/resume | running/stopped | unchanged | Success from stopped | 200 | fail (guard mismatch) |
| CRT001-TR-08 | POST /api/v2/engine/session/reset | stopped | unchanged | Success from stopped | 200 | fail (guard mismatch) |
| CRT001-TR-09 | POST /api/v2/engine/session/stop | stopped | unchanged | Error envelope | 409 / INVALID_SESSION_STATE | pass |

## Execution evidence (2026-03-02)

Observed endpoint sequence highlights (`esptari.local`):
- Initial status: `stopped`
- `pause` from stopped -> `409 INVALID_SESSION_STATE`
- `resume` from stopped -> `200 OK` (unexpected by matrix)
- `reset` from stopped -> `200 OK` (unexpected by matrix)
- `start` -> `200 OK`, state `running`
- `pause` -> `200 OK`, state `paused`
- `resume` -> `200 OK`, state `running`
- `reset` from running -> `200 OK`, state became `stopped` (matrix expected running)
- `stop` from running -> `200 OK`, state `stopped`

## Guard mapping checklist
- [x] Validate denial envelope fields (`code`, `details.guard_id`, `details.endpoint`, `details.esp_err`) for pause/stop invalid-state paths
- [x] Validate transition outcomes for start/pause/resume/stop nominal flows
- [ ] Align `resume` from stopped guard behavior to contract intent (currently allowed)
- [ ] Align `reset` semantics/guard behavior to contract intent (currently always succeeds and transitions to stopped)

## Artifact set
- Transition matrix with executed outcomes (this file)
- Runtime endpoint transcript captured during smoke sequence
- Residual behavior-delta checklist for contract alignment

## Phase gate reminder
Remaining closure for full CRT-001 signoff:
1) resolve lifecycle contract deltas for `resume` from stopped and `reset` behavior,
2) re-run targeted negative-case checks after semantic updates,
3) update acceptance decision record.

## Notes
- Runtime lifecycle evidence is captured and summarized in this artifact.
- This artifact now tracks implemented behavior and residual contract mismatches.

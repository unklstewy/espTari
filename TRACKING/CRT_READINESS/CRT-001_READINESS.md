# CRT-001 Implementation-Readiness Pack

Task: CRT-001
Phase: Pre-code planning only
Status: In Progress

## Objective
Prepare an implementation-ready lifecycle transition and guard verification plan aligned with accepted contracts, without executing runtime/API behavior.

## Contract anchors
- docs/EMU_ENGINE_V2_API_SPEC.md sections 6.0, 6.1..6.6B, 12
- docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md lifecycle/session references

## Preconditions (planning)
- [x] Contract docs accepted for T-054, T-055
- [ ] Runtime phase gate unlocked
- [ ] Core API/runtime implementation exists for lifecycle endpoints

## Transition coverage matrix (planned, not executed)

| Case ID | Endpoint | From State | Expected To State | Expected Result | Expected Error Code |
|---|---|---|---|---|---|
| CRT001-TR-01 | POST /api/v2/engine/session | stopped | running | Success envelope | n/a |
| CRT001-TR-02 | POST /api/v2/engine/session/pause | running | paused | Success envelope | n/a |
| CRT001-TR-03 | POST /api/v2/engine/session/resume | paused | running | Success envelope | n/a |
| CRT001-TR-04 | POST /api/v2/engine/session/reset | running | running | Success envelope | n/a |
| CRT001-TR-05 | POST /api/v2/engine/session/stop | running/paused/suspended/faulted | stopped | Success envelope | n/a |
| CRT001-TR-06 | POST /api/v2/engine/session/pause | paused/stopped | unchanged | Error envelope | INVALID_SESSION_STATE |
| CRT001-TR-07 | POST /api/v2/engine/session/resume | running/stopped | unchanged | Error envelope | INVALID_SESSION_STATE |
| CRT001-TR-08 | POST /api/v2/engine/session/reset | stopped | unchanged | Error envelope | INVALID_SESSION_STATE |
| CRT001-TR-09 | POST /api/v2/engine/session/stop | stopped | unchanged | Error envelope | INVALID_SESSION_STATE |

## Guard mapping checklist (planned)
- [ ] Validate guard-to-error mapping table extracted from API spec section 12
- [ ] Validate canonical error envelope fields for denial paths
- [ ] Define deterministic ordering expectations for state transition events

## Planned artifact set (no runtime outputs yet)
- Transition test vector sheet (this file)
- Expected envelope schema checklist
- Planned execution procedure (to be completed when phase gate unlocks)
- Evidence bundle index placeholder

## Phase gate reminder
Runtime/API validation is blocked until:
1) core lifecycle runtime implementation exists,
2) target deployment path is available,
3) deterministic fixtures are implemented,
4) PO/Acceptance Master unlocks runtime phase.

## Notes
- No runtime commands executed in this artifact.
- This file is planning evidence only.

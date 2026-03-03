# CRT-001 Readiness Pack

## Task
- Task ID: CRT-001
- Epic: EPIC-06
- Objective: Prepare lifecycle transition and guard behavior implementation-readiness pack.
- Status intent: Ready for implementation-phase verification execution.

## Dependency Closure
- T-054 (Done): lifecycle transition matrix and guard mapping schema published in `docs/emu_engine_v2/api_schema_lifecycle_transition_matrix_v1.json`.
- T-055 (Done): lifecycle guard validator responses implemented in:
  - `components/esptari_web/esptari_web_lifecycle_session.c`
  - `components/esptari_web/esptari_web_lifecycle_state.c`
- Runtime smoke evidence for guard envelope mapping:
  - `captures/lifecycle_guard_055_smoke_postflash_20260303_153045.txt`

## Planned Verification Matrix (Implementation Phase)
| Check ID | Endpoint | Scenario | Expected result |
|---|---|---|---|
| LIF-001 | `POST /api/v2/engine/session/start` | Valid start from stopped | `ok=true`, lifecycle transition accepted |
| LIF-002 | `POST /api/v2/engine/session/pause` | Pause from running | `ok=true`, transition to paused |
| LIF-003 | `POST /api/v2/engine/session/resume` | Resume paused with `resume_mode=running` | `ok=true`, transition to running |
| LIF-004 | `POST /api/v2/engine/session/stop` | Stop from running/paused | `ok=true`, terminal transition |
| LIF-005 | `POST /api/v2/engine/session/reset` | Reset from valid lifecycle state | `ok=true`, reset acknowledgment |
| LIF-G01 | `POST /api/v2/engine/session/pause` | Pause from stopped | `409 INVALID_SESSION_STATE`, `details.guard_id=G-LIFECYCLE-PAUSE` |
| LIF-G02 | `POST /api/v2/engine/session/resume` | Invalid `resume_mode` | `400 BAD_REQUEST`, `details.guard_id=G-RESUME-02` |
| LIF-G03 | `POST /api/v2/engine/session/reset` | Invalid `mode` | `400 BAD_REQUEST`, `details.guard_id=G-RESET-01` |
| LIF-G04 | `POST /api/v2/engine/session/suspend-save` | Suspend-save when not running | `409 INVALID_SESSION_STATE`, `details.guard_id=G-SUSPEND-01` |
| LIF-G05 | `POST /api/v2/engine/session/restore-resume` | Invalid restore `resume_mode` | `400 BAD_REQUEST`, `details.guard_id=G-RESUME-02` |

## Canonical Error Envelope Assertions
For all guard-denied scenarios above:
1. `ok=false`
2. `error.code` matches contract condition (`BAD_REQUEST`, `INVALID_SESSION_STATE`, etc.)
3. `error.category` is deterministic (`request`, `engine`, `snapshot`, `internal`)
4. `error.retryable=false` unless explicitly specified otherwise
5. `error.details.guard_id` and `error.details.endpoint` are present and match the probe target

## Artifact Templates
- Probe execution log template: timestamp, endpoint, request payload hash, response payload hash, verdict.
- Event-order template: lifecycle state before/after plus emitted stream event sequence IDs.
- Failure triage template: check ID, expected vs actual envelope fields, suspected guard branch.

## Traceability Anchors
- Contract transition/guard source: `docs/emu_engine_v2/api_schema_lifecycle_transition_matrix_v1.json`
- API normative references: `docs/EMU_ENGINE_V2_API_SPEC.md`
- Runtime guard implementation anchors:
  - `components/esptari_web/esptari_web_lifecycle_session.c`
  - `components/esptari_web/esptari_web_lifecycle_state.c`

## Readiness Decision
CRT-001 readiness artifact is complete: dependencies T-054 and T-055 are closed, guard/error mapping behavior is implemented and smoke-evidenced, and implementation-phase verification probes are fully specified.

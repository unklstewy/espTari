# CRT-003 Implementation-Readiness Pack

Task: CRT-003
Phase: Pre-code planning only
Status: In Progress

## Objective
Prepare an implementation-ready save/restore compatibility verification plan aligned with accepted contracts, without executing runtime/API behavior.

## Contract anchors
- docs/EMU_ENGINE_V2_API_SPEC.md sections 6.7, 6.8, 11.7
- docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md section 6.7

## Preconditions (planning)
- [x] Contract docs accepted for T-112, T-113, T-114, T-115
- [ ] Runtime phase gate unlocked
- [ ] Core API/runtime implementation exists for suspend-save/restore and compatibility validation paths

## Coverage matrix (planned, not executed)

| Case ID | Endpoint | Scenario | Expected Result | Expected Error Code |
|---|---|---|---|---|
| CRT003-SR-01 | POST /api/v2/engine/session/suspend-save | Valid running-session suspend-save | Success envelope, `running->suspended` transition | n/a |
| CRT003-SR-02 | POST /api/v2/engine/session/suspend-save | Invalid source lifecycle state | Error envelope | INVALID_SESSION_STATE |
| CRT003-SR-03 | POST /api/v2/engine/session/restore-resume | Valid suspended-session restore (`resume_mode=running`) | Success envelope, `suspended->running` transition | n/a |
| CRT003-SR-04 | POST /api/v2/engine/session/restore-resume | Valid suspended-session restore (`resume_mode=paused`) | Success envelope, `suspended->paused` transition | n/a |
| CRT003-SR-05 | POST /api/v2/engine/session/restore-resume | Session not suspended | Error envelope | ENGINE_NOT_SUSPENDED |
| CRT003-SR-06 | POST /api/v2/engine/session/restore-resume | Missing snapshot identifier | Error envelope | BAD_REQUEST |
| CRT003-SR-07 | POST /api/v2/engine/session/restore-resume | Snapshot id not found | Error envelope | SNAPSHOT_NOT_FOUND |
| CRT003-SR-08 | POST /api/v2/engine/session/restore-resume | Snapshot incompatible (schema/ABI/profile) | Error envelope | SNAPSHOT_INCOMPATIBLE |
| CRT003-SR-09 | Compatibility validator path | Valid schema/ABI/profile combination | Deterministic compatibility pass result | n/a |
| CRT003-SR-10 | Compatibility validator path | Invalid schema/ABI/profile combination | Deterministic compatibility failure result | SNAPSHOT_INCOMPATIBLE |

## Guard mapping checklist (planned)
- [ ] Validate suspend-save transition guard mapping (`SUSP-REQ-*`)
- [ ] Validate restore-resume guard mapping (`REST-RES-*`)
- [ ] Validate compatibility matrix/validator mapping (`RCOMP-*`, `RCOMP-VAL-*`)

## Contract check traceability map (planned)

| Check Family | Planned Cases | Assertion Focus |
|---|---|---|
| Suspend-save transition guards (`SUSP-REQ-*`) | CRT003-SR-01, CRT003-SR-02 | Allowed source state + deterministic rejection on invalid state |
| Restore-resume transition guards (`REST-RES-*`) | CRT003-SR-03, CRT003-SR-04, CRT003-SR-05, CRT003-SR-06 | Resume-mode outcomes + required-field and state guards |
| Snapshot lookup/compatibility failures | CRT003-SR-07, CRT003-SR-08 | Stable error mapping for not-found vs incompatible snapshots |
| Restore compatibility matrix (`RCOMP-*`) | CRT003-SR-09, CRT003-SR-10 | Deterministic schema/ABI/profile compatibility outcomes |
| Validator mapping (`RCOMP-VAL-*`) | CRT003-SR-09, CRT003-SR-10 | Deterministic validator result projection and failure categories |

## Planned execution procedure (phase-gated; not executed)

1. Preflight contract lock
	- Confirm save/restore and compatibility sections (`6.7`, `6.8`, `11.7`) are contract-stable.
	- Record contract revision fingerprint in evidence index.

2. Fixture definition
	- Define snapshot fixture set: compatible baseline, schema-mismatch, ABI-mismatch, and profile-mismatch variants.
	- Define lifecycle-state fixture set for valid and invalid suspend/restore source-state coverage.

3. Case sequencing design
	- Plan transition order: suspend-save valid/invalid -> restore-resume valid modes -> restore failure modes -> compatibility validator probes.
	- Isolate each failure mode to one dominant guard condition per case.

4. Evidence capture plan
	- For each case, capture expected request envelope, expected response envelope, and expected lifecycle/compatibility deltas.
	- Define evidence row format with explicit check-family mapping.

5. Evaluation rubric
	- Case passes only if transition target, envelope shape, deterministic error code, and compatibility classification match planned expectations.
	- Any mismatch is classified as `contract drift`, `implementation gap`, or `fixture issue`.

6. PO handoff package plan
	- Prepare summary of planned case coverage, blocker list, and runtime unlock prerequisites.
	- Keep acceptance decision Pending until runtime phase gate unlocks.

## Evidence template (planned)

| Case ID | Planned Input Ref | Expected Response Ref | Expected State/Compat Delta | Planned Verdict Rule |
|---|---|---|---|---|
| CRT003-SR-01 | req/suspend-valid | rsp/suspend-success | state `running->suspended` | pass if transition and envelope match |
| CRT003-SR-03 | req/restore-running | rsp/restore-running-success | state `suspended->running` | pass if transition and timestamp semantics match |
| CRT003-SR-04 | req/restore-paused | rsp/restore-paused-success | state `suspended->paused` | pass if transition and mode semantics match |
| CRT003-SR-08 | req/restore-incompatible | rsp/restore-incompatible | compatibility fail classification | pass if `SNAPSHOT_INCOMPATIBLE` |
| CRT003-SR-10 | req/compat-validator-invalid | rsp/compat-invalid | validator rejects with mapped reason | pass if deterministic invalid classification |

## Planned artifact set (no runtime outputs yet)
- Save/restore compatibility scenario vector sheet (this file)
- Expected transition/compatibility checklist
- Planned execution procedure (to be completed when phase gate unlocks)
- Evidence bundle index placeholder

## Phase gate reminder
Runtime/API validation is blocked until:
1) core save/restore runtime implementation exists,
2) target deployment path is available,
3) deterministic fixtures are implemented,
4) PO/Acceptance Master unlocks runtime phase.

## Notes
- No runtime commands executed in this artifact.
- This file is planning evidence only.

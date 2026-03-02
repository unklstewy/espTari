# CRT-002 Implementation-Readiness Pack

Task: CRT-002
Phase: Pre-code planning only
Status: In Progress

## Objective
Prepare an implementation-ready input mapping CRUD/apply conformance verification plan aligned with accepted contracts, without executing runtime/API behavior.

## Contract anchors
- docs/EMU_ENGINE_V2_API_SPEC.md sections 9.6.2..9.6.4
- docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md section 6.4

## Preconditions (planning)
- [x] Contract docs accepted for T-071
- [ ] Runtime phase gate unlocked
- [ ] Core API/runtime implementation exists for input mapping CRUD/apply endpoints

## Coverage matrix (planned, not executed)

| Case ID | Endpoint | Scenario | Expected Result | Expected Error Code |
|---|---|---|---|---|
| CRT002-MAP-01 | POST /api/v2/input/mappings | Create new profile with valid schema | Success envelope | n/a |
| CRT002-MAP-02 | POST /api/v2/input/mappings | Create duplicate mapping_profile_id for same machine | Error envelope | CONFLICT |
| CRT002-MAP-03 | GET /api/v2/input/mappings/{mapping_profile_id} | Existing profile read | Success envelope | n/a |
| CRT002-MAP-04 | PATCH /api/v2/input/mappings/{mapping_profile_id} | Effective mapping mutation | Success envelope with revision +1 | n/a |
| CRT002-MAP-05 | PATCH /api/v2/input/mappings/{mapping_profile_id} | Semantic no-op patch | Success envelope, revision unchanged | n/a |
| CRT002-MAP-06 | POST /api/v2/input/mappings/apply | Apply inactive profile to running session | Success envelope with cutover_tick | n/a |
| CRT002-MAP-07 | POST /api/v2/input/mappings/apply | Re-apply same profile/revision | Success envelope with result=no_op | n/a |
| CRT002-MAP-08 | POST /api/v2/input/mappings/apply | expected_revision mismatch | Error envelope | CONFLICT |
| CRT002-MAP-09 | GET/POST/PATCH on unknown mapping_profile_id | Missing profile operations | Error envelope | INPUT_MAPPING_NOT_FOUND |

## Guard mapping checklist (planned)
- [ ] Validate revision monotonicity rules for mutating updates
- [ ] Validate apply atomic cutover/no-op semantics
- [ ] Validate deterministic conflict-path mapping (`CONFLICT`, `INPUT_MAPPING_NOT_FOUND`)

## Contract check traceability map (planned)

| Check Family | Planned Cases | Assertion Focus |
|---|---|---|
| CRUD create/list/get semantics | CRT002-MAP-01, CRT002-MAP-03 | Envelope shape + canonical profile projection |
| CRUD conflict/missing semantics | CRT002-MAP-02, CRT002-MAP-09 | Deterministic error mapping and stable code usage |
| Revision monotonicity semantics | CRT002-MAP-04, CRT002-MAP-05 | `revision` increments only on effective mutation |
| Apply/cutover semantics | CRT002-MAP-06, CRT002-MAP-07 | Atomic cutover and deterministic `no_op` behavior |
| Apply revision-guard semantics | CRT002-MAP-08 | Expected `CONFLICT` mapping for revision mismatch |

## Planned execution procedure (phase-gated; not executed)

1. Preflight contract lock
	- Confirm API contract anchors remain unchanged for sections `9.6.2..9.6.4`.
	- Record contract revision fingerprint in evidence index.

2. Fixture definition
	- Define baseline fixture set: one valid mapping profile, one duplicate-ID profile, one unknown-ID probe, and one revision-mismatch apply probe.
	- Define deterministic expected outputs for each case in the coverage matrix.

3. Case-run sequencing design
	- Plan execution order: create -> read -> mutate -> no-op mutate -> apply -> apply no-op -> mismatch -> missing-profile probes.
	- Ensure sequence isolates revision side effects and preserves reproducibility.

4. Evidence capture plan
	- For each case, capture expected request envelope, expected response envelope, and expected state deltas (`revision`, active profile identity, cutover marker).
	- Define one evidence row per case in the execution log template.

5. Evaluation rubric
	- Case is pass only if expected envelope shape, status code, deterministic error code, and state-delta conditions all match plan.
	- Any mismatch is recorded as fail with one root-cause classification (`contract drift`, `implementation gap`, `fixture issue`).

6. PO handoff package plan
	- Prepare summary view: pass/fail readiness logic, blockers, and explicit runtime-phase prerequisites.
	- Keep final decision in `TRACKING/ACCEPTANCE_LOG.md` as Pending until runtime gate unlock.

## Evidence template (planned)

| Case ID | Planned Input Ref | Expected Response Ref | Expected State Delta | Planned Verdict Rule |
|---|---|---|---|---|
| CRT002-MAP-01 | req/create-valid | rsp/create-success | profile exists, revision=1 | pass if success envelope + canonical projection |
| CRT002-MAP-02 | req/create-duplicate | rsp/conflict-duplicate | none | pass if `CONFLICT` |
| CRT002-MAP-04 | req/patch-mutate | rsp/patch-mutated | revision +1 | pass if revision increments exactly once |
| CRT002-MAP-05 | req/patch-noop | rsp/patch-noop | revision unchanged | pass if deterministic no-op |
| CRT002-MAP-08 | req/apply-rev-mismatch | rsp/conflict-revision | active profile unchanged | pass if `CONFLICT` |

## Planned artifact set (no runtime outputs yet)
- CRUD/apply scenario vector sheet (this file)
- Expected envelope and revision-state checklist
- Planned execution procedure (to be completed when phase gate unlocks)
- Evidence bundle index placeholder

## Phase gate reminder
Runtime/API validation is blocked until:
1) core mapping/runtime implementation exists,
2) target deployment path is available,
3) deterministic fixtures are implemented,
4) PO/Acceptance Master unlocks runtime phase.

## Notes
- No runtime commands executed in this artifact.
- This file is planning evidence only.

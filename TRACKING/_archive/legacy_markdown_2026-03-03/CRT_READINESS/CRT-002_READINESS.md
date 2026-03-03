# CRT-002 Implementation-Readiness Pack

Task: CRT-002
Phase: Runtime implementation + smoke evidence capture
Status: Implemented (Smoke Validated)

## Objective
Implement input mapping CRUD/apply runtime path aligned with accepted contracts and capture deterministic smoke evidence for core endpoint behavior.

## Contract anchors
- docs/EMU_ENGINE_V2_API_SPEC.md sections 9.6.2..9.6.4
- docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md section 6.4

## Preconditions
- [x] Contract docs accepted for T-071
- [x] Core API/runtime implementation exists for input mapping CRUD/apply endpoints
- [x] Device build/flash path available for smoke validation

## Coverage matrix (executed subset)

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

## Execution evidence (2026-03-02)

Implementation commits and scope:
- Runtime mapping store and apply state added in `components/esptari_input/esptari_input.c`
- Mapping CRUD/apply HTTP endpoints added in `components/esptari_web/esptari_web.c`
- Web runtime hardening (URI capacity + stack/heap response handling) added in `components/esptari_web/esptari_web.c`

Observed runtime outcomes on target (`esptari.local`):
- `GET /api/v2/engine/health` -> `200 OK`
- `POST /api/v2/input/mappings` -> `201 Created`
- `GET /api/v2/input/mappings?machine=atari_st` -> `200 OK`
- `GET /api/v2/input/mappings/st_test_v1` -> `200 OK`
- `PATCH /api/v2/input/mappings/st_test_v1` (semantic no-op) -> `200 OK` with `result=no_op`
- `POST /api/v2/engine/session` -> `200 OK`
- `POST /api/v2/input/mappings/apply` -> `200 OK` with `result=applied`, `cutover_tick=1`
- `GET /api/v2/input/mappings/active` -> `200 OK`

Stability evidence:
- Prior crash path (`httpd_register_uri_handler: no slots left` + HTTP task stack protection fault) no longer reproduced after server capacity/stack fixes.
- Monitor logs confirm normal startup and web API readiness after connectivity.

## Guard mapping checklist
- [x] Validate revision monotonicity rules for semantic no-op patch path
- [x] Validate apply atomic cutover/no-op semantics for first apply (`cutover_tick` observed)
- [ ] Validate deterministic conflict-path mapping (`CONFLICT`, `INPUT_MAPPING_NOT_FOUND`) in dedicated negative-case pass

## Contract check traceability map (planned)

| Check Family | Planned Cases | Assertion Focus |
|---|---|---|
| CRUD create/list/get semantics | CRT002-MAP-01, CRT002-MAP-03 | Envelope shape + canonical profile projection |
| CRUD conflict/missing semantics | CRT002-MAP-02, CRT002-MAP-09 | Deterministic error mapping and stable code usage |
| Revision monotonicity semantics | CRT002-MAP-04, CRT002-MAP-05 | `revision` increments only on effective mutation |
| Apply/cutover semantics | CRT002-MAP-06, CRT002-MAP-07 | Atomic cutover and deterministic `no_op` behavior |
| Apply revision-guard semantics | CRT002-MAP-08 | Expected `CONFLICT` mapping for revision mismatch |

## Follow-up execution procedure

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
	- Prepare summary view: executed cases, remaining negative-case checks, and residual blockers.
	- Keep final decision in `TRACKING/ACCEPTANCE_LOG.md` as Pending until negative-case matrix is closed.

## Evidence template (planned)

| Case ID | Planned Input Ref | Expected Response Ref | Expected State Delta | Planned Verdict Rule |
|---|---|---|---|---|
| CRT002-MAP-01 | req/create-valid | rsp/create-success | profile exists, revision=1 | pass if success envelope + canonical projection |
| CRT002-MAP-02 | req/create-duplicate | rsp/conflict-duplicate | none | pass if `CONFLICT` |
| CRT002-MAP-04 | req/patch-mutate | rsp/patch-mutated | revision +1 | pass if revision increments exactly once |
| CRT002-MAP-05 | req/patch-noop | rsp/patch-noop | revision unchanged | pass if deterministic no-op |
| CRT002-MAP-08 | req/apply-rev-mismatch | rsp/conflict-revision | active profile unchanged | pass if `CONFLICT` |

## Artifact set
- CRUD/apply scenario vector sheet + executed subset outcomes (this file)
- Runtime endpoint smoke transcript (session logs + curl output captured in agent run)
- Residual negative-case checklist for conflict/not-found paths

## Phase gate reminder
Remaining closure for full CRT-002 signoff:
1) execute conflict/not-found negative matrix rows (`CRT002-MAP-02`, `CRT002-MAP-08`, `CRT002-MAP-09`),
2) publish explicit evidence links for those rows,
3) record acceptance decision update.

## Notes
- Runtime implementation and smoke validation have been executed.
- This artifact now tracks both readiness intent and executed evidence subset.

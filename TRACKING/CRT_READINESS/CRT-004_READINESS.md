# CRT-004 Implementation-Readiness Pack

Task: CRT-004
Phase: Runtime implementation + smoke evidence capture
Status: Implemented (Guard + telemetry depth validated; long-run stream invariants pending)

## Objective
Validate observability stream endpoint availability and baseline guard behavior, then stage deeper telemetry/alarm evidence work.

## Contract anchors
- docs/EMU_ENGINE_V2_API_SPEC.md sections 10.2, 10.3, 10.4, 10.5, 10.6, 10.7, 10.8
- docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md sections 6.5, 7

## Preconditions
- [x] Contract docs accepted for T-083, T-084, T-085, T-086, T-087, T-088, T-089, T-090, T-091
- [x] Core API/runtime stream endpoint wiring exists
- [ ] Full telemetry/backpressure/alarm implementation depth exists

## Coverage matrix (executed subset)

| Case ID | Endpoint/Path | Scenario | Expected Result | Expected Error Code |
|---|---|---|---|---|
| CRT004-OBS-01 | GET /api/v2/stream/video | Valid video metadata/payload pairing | Success stream with deterministic ordering | n/a |
| CRT004-OBS-02 | GET /api/v2/stream/audio | Valid audio metadata/payload pairing | Success stream with deterministic ordering | n/a |
| CRT004-OBS-03 | GET /api/v2/inspect/registers/stream | Valid register selector/filter set | Success stream with selector-constrained payloads | n/a |
| CRT004-OBS-04 | GET /api/v2/inspect/bus/stream | Valid bus filter configuration | Success stream with deterministic filtered projection | n/a |
| CRT004-OBS-05 | GET /api/v2/inspect/memory/stream | Valid memory filter configuration | Success stream with deterministic filtered projection | n/a |
| CRT004-OBS-06 | Filter update path | Invalid filter payload shape/value | Error envelope | INSPECT_FILTER_INVALID |
| CRT004-OBS-07 | Stream/telemetry path | Unknown/inactive session | Error envelope | ENGINE_NOT_RUNNING |
| CRT004-OBS-08 | Backpressure telemetry path | High-load/degraded-delivery scenario | Deterministic counter and delivery-disclosure behavior | n/a |
| CRT004-OBS-09 | SLO alarm path | Threshold breach/recovery sequence | Deterministic alarm ordering and severity mapping | n/a |

## Execution evidence (2026-03-02)

Observed endpoint outcomes on `esptari.local`:
- `GET /api/v2/stream/video` while session running -> `200 OK` with `stream`, `event_seq`, `event_timestamp_us`
- `GET /api/v2/stream/video?backpressure=1` while session running -> `200 OK` with `delivery.degraded=true`, `reason=queue_overflow`, and incremented backpressure counters
- Recovery probe (`GET /api/v2/stream/video` after backpressure) -> `200 OK` with `delivery.degraded=false` and preserved monotonic stream ordering fields
- SLO chronology probe: `?slo=breach` then `?slo=recover` -> `200 OK`, `slo_alarm.seq` monotonic and `state` transitions `normal -> breach -> recovered`
- `GET /api/v2/stream/audio` route exists; while stopped -> `409` with `ENGINE_NOT_RUNNING`
- `GET /api/v2/inspect/registers/stream` route exists; while stopped -> `409` with `ENGINE_NOT_RUNNING`
- `GET /api/v2/inspect/bus/stream` route exists; while stopped -> `409` with `ENGINE_NOT_RUNNING`
- `GET /api/v2/inspect/memory/stream` route exists; while stopped -> `409` with `ENGINE_NOT_RUNNING`
- Invalid selector probes (`component/source/region=unknown_*`) on register/bus/memory inspect streams -> `400` with `INSPECT_FILTER_INVALID`
- Route-availability blocker cleared (previous `404 URI not found` no longer reproduced for CRT-004 endpoints)

Residuals:
- No continuous stream framing evidence yet (single-payload probe only)
- Backpressure and SLO checks are currently probe-driven and need sustained-load validation for production confidence

## Guard mapping checklist (planned)
- [ ] Validate stream payload/order assertions (video/audio/register/bus/memory)
- [ ] Validate selector/filter rejection mapping (`INSPECT_FILTER_INVALID`)
- [ ] Validate backpressure/SLO alarm sequencing and deterministic mapping

## Contract check traceability map (planned)

| Check Family | Planned Cases | Assertion Focus |
|---|---|---|
| Video/audio emitter checks (`VID-EMIT-*`, `AUD-EMIT-*`) | CRT004-OBS-01, CRT004-OBS-02 | Metadata/payload pairing + ordering determinism |
| Register/bus/memory publisher checks (`REG-PUB-*`, `BUS-FLT-*`, `MEM-FLT-*`) | CRT004-OBS-03, CRT004-OBS-04, CRT004-OBS-05 | Selector/filter correctness + monotonic event ordering |
| Filter guard mappings | CRT004-OBS-06 | Deterministic invalid-filter rejection semantics |
| Session guard mappings | CRT004-OBS-07 | Deterministic unknown/inactive-session rejection |
| Backpressure telemetry checks (`BP-CTR-*`) | CRT004-OBS-08 | Counter invariants + degraded-delivery disclosures |
| SLO alarm checks (`SLO-ALRM-*`) | CRT004-OBS-09 | Breach/recovery ordering + severity mapping determinism |

## Planned execution procedure (phase-gated; not executed)

1. Preflight contract lock
	- Confirm observability-related sections (`10.2`..`10.8`) are contract-stable.
	- Record contract revision fingerprint in evidence index.

2. Fixture definition
	- Define stream fixture set for nominal output and constrained selectors/filters.
	- Define load fixture set for degraded/backpressure and threshold-breach alarm scenarios.

3. Case sequencing design
	- Plan order: nominal stream checks -> filter-path checks -> guard-failure checks -> backpressure telemetry -> alarm chronology.
	- Isolate one dominant assertion target per case.

4. Evidence capture plan
	- For each case, capture expected stream envelope fragments, ordering constraints, and expected counter/alarm deltas.
	- Define one evidence row per case with check-family linkage.

5. Evaluation rubric
	- Case passes only if payload schema, ordering fields, deterministic error code (when applicable), and telemetry/alarm deltas match planned expectations.
	- Any mismatch is classified as `contract drift`, `implementation gap`, or `fixture issue`.

6. PO handoff package plan
	- Prepare summary with coverage completeness, unresolved blockers, and runtime unlock prerequisites.
	- Keep acceptance decision Pending until runtime phase gate unlocks.

## Evidence template (planned)

| Case ID | Planned Input Ref | Expected Response/Stream Ref | Expected Telemetry/Order Delta | Planned Verdict Rule |
|---|---|---|---|---|
| CRT004-OBS-01 | req/video-nominal | stream/video-metadata-payload | monotonic seq/timestamp, valid pairing | pass if `VID-EMIT-*` conditions satisfied |
| CRT004-OBS-03 | req/register-filter | stream/register-filtered | selector-constrained payload projection | pass if `REG-PUB-*` conditions satisfied |
| CRT004-OBS-06 | req/filter-invalid | rsp/filter-invalid | none | pass if `INSPECT_FILTER_INVALID` |
| CRT004-OBS-08 | req/backpressure-load | stream/backpressure-telemetry | counter increments + degraded disclosure fields | pass if `BP-CTR-*` conditions satisfied |
| CRT004-OBS-09 | req/slo-breach-recovery | stream/slo-alarms | deterministic breach/recovery order + severity | pass if `SLO-ALRM-*` conditions satisfied |

## Planned artifact set (no runtime outputs yet)
- Observability scenario vector sheet (this file)
- Expected payload/order/telemetry checklists
- Planned execution procedure (to be completed when phase gate unlocks)
- Evidence bundle index placeholder

## Phase gate reminder
Runtime/API validation is blocked until:
1) core observability/runtime implementation exists,
2) target deployment path is available,
3) deterministic fixtures are implemented,
4) PO/Acceptance Master unlocks runtime phase.

## Notes
- Runtime commands executed and baseline stream evidence captured.
- This artifact now tracks implemented endpoint availability plus remaining telemetry-depth gaps.

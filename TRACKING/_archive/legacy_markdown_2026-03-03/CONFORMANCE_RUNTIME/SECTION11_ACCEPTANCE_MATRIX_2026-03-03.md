# Section 11 Acceptance Matrix — 2026-03-03

Scope: P0-4 execution artifact for milestone acceptance criteria in Section 11.

Source: `docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md` Section 11.

Status legend:
- `PASS` = criterion satisfied with linked evidence
- `PARTIAL` = some evidence present, closure pending
- `PENDING` = no closure evidence yet

| # | Section 11 Criterion | Current Status | Primary Evidence | Follow-up Action |
|---|---|---|---|---|
| 1 | Lifecycle control via v2 APIs (`start/pause/resume/reset/stop`) | PARTIAL | API lifecycle acceptance rows in `TRACKING/ACCEPTANCE_LOG.md` | Run consolidated lifecycle acceptance suite and attach one canonical run bundle |
| 2 | ROM/disk/cartridge loaded from SD-card only | PARTIAL | Media API implementation commits and catalog endpoint evidence | Add runtime proof that resolved media paths enforce SD-only policy |
| 3 | EBIN modules remotely uploaded/validated/loaded/unloaded | PARTIAL | `6.2` ebin endpoint implementation and route verifier coverage | Execute EBIN CRUD/load lifecycle checks and collect run artifacts |
| 4 | Video/audio available via browser-consumable stream APIs | PARTIAL | Stream route implementation and acceptance rows | Capture runtime stream conformance session (video/audio metadata + payload pairing) |
| 5 | Register inspection available as snapshot + stream | PARTIAL | `6.5/6.6` route completion map | Run register snapshot/stream consistency checks and attach report |
| 6 | Bus/memory traces available via filtered stream endpoints | PARTIAL | `6.5` inspect bus/memory stream routes | Execute filter matrix scenarios and attach pass/fail outputs |
| 7 | Input translation pipeline + profile mappings working | PARTIAL | Input API and mapping acceptance evidence | Add runtime translation scenario evidence with mapping revisions/cutover |
| 8 | Trace/backpressure stable with explicit dropped-event metrics | PARTIAL | Backpressure telemetry endpoint implemented | Load-run with backpressure counters and delivery degradation checks |
| 9 | Browser capture policy supports enable/disable + capture modes + escape release | PARTIAL | Input policy/capture endpoints and prior PRQ captures | Run policy transition matrix and capture-mode behavior report |
| 10 | Catalog-backed media resolution + missing-asset hosted download works | PARTIAL | Catalog `7.9/7.10` acceptance entries | Execute full missing-asset workflow end-to-end and attach artifacts |
| 11 | Dead links marked and scheduler-driven sync refreshes catalogs | PARTIAL | Catalog probe/mark-dead/schedule endpoints implemented | Validate dead-link state transitions + schedule-triggered refresh evidence |
| 12 | Machine state save/restore supports suspend-save + restore-resume + compatibility checks | PARTIAL | Persistence endpoints + PRQ snapshot closure evidence | Produce consolidated save/restore compatibility run bundle |
| 13 | Hard SLO targets measured/exposed (`<=50ms`, `<30ms`, `<1%`) | PARTIAL | Metrics endpoints implemented and accepted contract docs | Add measured runtime SLO report with thresholds and pass/fail status |
| 14 | Debug clock controls support slow-motion/single-step with observability correctness | PARTIAL | Clock mode/step APIs + acceptance captures | Execute debug-clock conformance run and attach scheduler/trace evidence |

## Priority closure order (for runtime execution)

1. Criteria `1, 12, 14` (core lifecycle/save-restore/debug correctness)
2. Criteria `4, 5, 6, 8` (stream/inspection/backpressure observability)
3. Criteria `7, 9` (input translation + capture policy)
4. Criteria `10, 11` (catalog/dead-link/scheduler reliability)
5. Criterion `13` (hard SLO validation with measured outputs)

## Completion gate

Section 11 is considered closed when all 14 rows are `PASS` and each row has one or more linked evidence artifacts in `SECTION11_EVIDENCE_INDEX_2026-03-03.md`.

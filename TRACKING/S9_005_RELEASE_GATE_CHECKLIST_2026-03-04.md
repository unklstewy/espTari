# S9-005 Release-Gate Checklist (2026-03-04)

## Scope

- Task: `S9-005`
- Objective: encode Section 11 Atari ST milestone acceptance criteria into deterministic periodic release-gate checklist items.
- Source criteria: `docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md` (Section `11`)
- Decision outcomes supported: `pass`, `pass_with_conditions`, `fail`

## Gate execution rules

- Each gate item must be evaluated as exactly one status: `pass`, `conditional`, `fail`, `not_evaluated`.
- Every `pass` or `conditional` item must provide at least one concrete evidence path.
- Any `fail` in a `blocking=true` gate forces overall decision to `fail`.
- `pass_with_conditions` is only allowed when all `blocking=true` gates are `pass` and one or more non-blocking gates are `conditional`.

## Checklist gates

| Gate ID | Section 11 criterion | Blocking | Deterministic verification signal | Minimum evidence |
|---|---|---|---|---|
| GATE-S11-01 | Lifecycle control via v2 APIs (`start/pause/resume/reset/stop`) | true | Canonical lifecycle success/guard matrix is present with deterministic status/error mapping | Lifecycle conformance artifact(s) |
| GATE-S11-02 | ROM/disk/cartridge loaded from SD-card only | true | Asset path policy shows SD-only acceptance and disallowed path rejection | Media path enforcement artifact(s) |
| GATE-S11-03 | EBIN modules remotely uploaded/validated/loaded/unloaded | true | EBIN lifecycle checks include success + canonical negative guards | EBIN lifecycle artifact(s) |
| GATE-S11-04 | Video/audio streaming APIs are browser-consumable | true | Stream probes show valid video/audio responses and stable envelope fields | Stream A/V artifact(s) |
| GATE-S11-05 | Register inspection supports snapshot and stream | true | Snapshot/stream outputs are both present and schema-consistent | Register inspection artifact(s) |
| GATE-S11-06 | Bus/memory traces exposed through filtered streams | true | Filter matrix demonstrates deterministic include/exclude behavior | Bus/memory trace artifact(s) |
| GATE-S11-07 | Input API translates keyboard/mouse/controller using profile mappings | true | Mapping apply + translation scenarios pass with deterministic guards for invalid cases | Input translation artifact(s) |
| GATE-S11-08 | Trace system stable under backpressure with dropped-event metrics | true | Backpressure counters and dropped-event semantics remain deterministic under load | Backpressure/load artifact(s) |
| GATE-S11-09 | Browser-session capture policy modes and release semantics | false | Enable/disable/mode-switch/escape-release transitions are deterministic | Capture policy artifact(s) |
| GATE-S11-10 | Catalog-backed media resolution and hosted missing-asset download | true | Catalog resolution + missing-asset download workflow passes with canonical errors | Catalog workflow artifact(s) |
| GATE-S11-11 | Dead-link marking and scheduler-driven catalog refresh | true | Dead-link state transitions and scheduled refresh behavior are deterministic | Dead-link/scheduler artifact(s) |
| GATE-S11-12 | Suspend-save and restore-resume with compatibility checks | true | Save/restore compatibility matrix passes with canonical incompatibility mapping | Save/restore artifact(s) |
| GATE-S11-13 | SLO exposure and thresholds (`<=50ms`, `<30ms`, `<1%`) | true | Measured SLO outputs include threshold comparison with explicit pass/fail values | SLO validation artifact(s) |
| GATE-S11-14 | Debug clock slow-motion/single-step with observability correctness | true | Debug mode transition and single-step observability checks are deterministic | Debug clock artifact(s) |

## Periodic cadence

- `weekly` dry-run or readiness sweep (documentation + evidence freshness review).
- `per-release` final release-gate decision using this checklist and gate report template.

## Gate decision policy

- `pass`: all blocking gates `pass`; no unresolved conditions.
- `pass_with_conditions`: all blocking gates `pass`; one or more non-blocking gates `conditional` with deadlines/owners.
- `fail`: any blocking gate `fail`, or evidence is missing for any blocking gate.

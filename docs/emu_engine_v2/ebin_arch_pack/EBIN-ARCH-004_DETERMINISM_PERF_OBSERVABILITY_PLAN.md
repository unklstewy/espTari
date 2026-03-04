# EBIN-ARCH-004 Determinism, Performance, Observability Plan

## Purpose
Define measurable determinism/performance/observability targets for EBIN execution, separating what is measurable now from what is deferred.

## Scope
- Determinism checks for loader and subsystem integration
- Performance/SLO checks for release hard-validation
- Observability evidence requirements and artifact format

## Dependencies
- `TRACKING/S5_RUNTIME_UNLOCK_TRACEABILITY_MATRIX_2026-03-04.md`
- `TRACKING/SPRINT_02_CLOSURE_REPORT_2026-03-04.md`
- `TRACKING/_archive/legacy_markdown_2026-03-03/MVP_DEFINITION.md`
- `docs/emu_engine_v2/10_validation_plan.md`
- `docs/emu_engine_v2/05_timing.md`

## Interfaces
- Input: deterministic harness fixtures, stream payload contracts, SLO metric endpoints
- Output: measurable-now checklist, deferred checklist, and evidence package schema

## Measurable now (Phase A/B)

| Check family | What is measured now | Expected outcome | Evidence artifact | Tag |
|---|---|---|---|---|
| Loader determinism | resolver/validator/gates/load/unload/rollback run-to-run identity | same input => same decision, code, and ordering | S5-style capture files + replay diff report | `dev_allowed_now` |
| Admission failure semantics | canonical first-fail error mapping | stable `error.code` and detail fields by failure class | gate failure matrix + capture snippets | `dev_allowed_now` |
| Subsystem local timing contracts | adapter-level register/timing assertions per subsystem | no contract violations in local checks | per-subsystem conformance captures | `dev_allowed_now` |
| Integration ordering checks | interrupt and arbitration ordering with fixed fixtures | deterministic ordering invariants pass | vector/arbitration replay artifacts | `integration_blocked` until integrated |

## Deferred to later (Phase C)

| Check family | Deferred measurement | Release significance | Evidence artifact | Tag |
|---|---|---|---|---|
| Long-run soak | multi-hour stability under media/input/stream churn | detects state drift and latent race/fault patterns | soak logs + failure triage packet | `release_blocked` |
| Production package trust | signed package validation and key policy enforcement | release trust boundary closure | signing verification report | `release_blocked` |
| SLO under stress | latency <=50ms, jitter <30ms, dropped-frame <1% under stress profiles | MVP performance closure and launch safety | stress-run metric bundles + alarm chronology | `release_blocked` |

## Observability evidence format

Minimum artifact set per execution slice:

1. `scenario_id` and fixed fixture hash
2. runtime/version + active machine profile
3. expected checks (IDs) and actual outcomes
4. ordered timestamped event stream excerpt
5. pass/fail summary and first-fail detail payload

## Acceptance criteria
1. Plan clearly separates measurable-now vs deferred checks.
2. Every check family defines expected artifact output.
3. Deferred items are explicitly release-blocking where required.

## Evidence expected
- Deterministic replay diff reports for all measurable-now families.
- Phase C soak/signing/SLO packages before release sign-off.

## Cross-links
- [EBIN-ARCH-000 Index](EBIN-ARCH-000_INDEX.md)
- [EBIN-INTF-001 Interface and Contract Matrix](EBIN-INTF-001_INTERFACE_CONTRACT_MATRIX.md)
- [EBIN-ARCH-003 Sprint-Ready Work Breakdown](EBIN-ARCH-003_SPRINT_READY_BREAKDOWN.md)
- [EBIN-GATE-001 Integration Gates and Non-Go Conditions](EBIN-GATE-001_INTEGRATION_GATES_AND_NON_GO.md)

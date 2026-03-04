# EBIN-ARCH-006 Developer Execution Checklist

## Purpose
Provide a cycle-ready developer checklist by phase, with each task including boundaries, test intent, evidence, and execution tag.

## Scope
- Phase A/B/C execution checklist
- Task-level Do/Don’t boundaries
- Test intent and evidence artifact expectations

## Dependencies
- `docs/emu_engine_v2/ebin_arch_pack/EBIN-ARCH-002_SUBSYSTEM_DECOMPOSITION_AND_ORDER.md`
- `docs/emu_engine_v2/ebin_arch_pack/EBIN-ARCH-003_SPRINT_READY_BREAKDOWN.md`
- `docs/emu_engine_v2/ebin_arch_pack/EBIN-INTF-001_INTERFACE_CONTRACT_MATRIX.md`
- `docs/emu_engine_v2/ebin_arch_pack/EBIN-GATE-001_INTEGRATION_GATES_AND_NON_GO.md`
- `TRACKING/BACKLOG.md`

## Interfaces
- Input: sprint-ready architecture tasks and gate criteria
- Output: actionable checklists for implementation and evidence closure

## Phase A checklist (Safe-to-start)

### A-01 Admission contract lock (E1-S1-T1)
- Do: enforce current ABI field list and fail-fast order in tests.
- Don’t: change ABI schema, error taxonomy, or envelope semantics.
- Test intent: re-run manifest/gate deterministic probes.
- Evidence expected: deterministic capture and pass/fail matrix.
- Tag: `dev_allowed_now`

### A-02 Resolver determinism lock (E1-S2-T1)
- Do: freeze fixture catalogs and expected resolver outcomes.
- Don’t: alter selection policy to satisfy one-off fixtures.
- Test intent: same input repeated yields same output.
- Evidence expected: replay diff report showing no variation.
- Tag: `dev_allowed_now`

### A-03 Rollback/fallback lock (E1-S3-T1)
- Do: verify fail -> rollback -> recovery sequence.
- Don’t: introduce partial-failure paths without deterministic outcome.
- Test intent: force activation failure and verify recovery telemetry.
- Evidence expected: rollback/fallback capture with outcome fields.
- Tag: `dev_allowed_now`

### A-04 GLUE/MMU/SHIFTER adapter baseline (E2-S1-T1)
- Do: implement adapter surfaces and timing hook points.
- Don’t: redefine memory map or arbitration model.
- Test intent: register-window + hook conformance checks.
- Evidence expected: adapter conformance logs.
- Tag: `dev_allowed_now`

### A-05 MFP adapter baseline (E2-S2-T1)
- Do: implement timer/register semantics and IRQ-related state behavior.
- Don’t: remap vector logic or simplify ISR/IPR/IMR interactions.
- Test intent: timer and interrupt state transition checks.
- Evidence expected: MFP conformance capture.
- Tag: `dev_allowed_now`

### A-06 ACIA+IKBD adapter baseline (E2-S3-T1)
- Do: preserve ACIA framing and IKBD packet cadence.
- Don’t: bypass ACIA timing with direct high-level event injection.
- Test intent: positive/negative serial framing and cadence tests.
- Evidence expected: ACIA/IKBD timing captures.
- Tag: `dev_allowed_now`

### A-07 DMA/FDC adapter baseline (E2-S4-T1)
- Do: preserve DRQ/INTRQ-driven pacing and terminal conditions.
- Don’t: collapse command phases into immediate transfers.
- Test intent: command FSM and pacing replay checks.
- Evidence expected: DMA/FDC replay artifact set.
- Tag: `dev_allowed_now`

### A-08 PSG adapter baseline (E2-S5-T1)
- Do: preserve register timing, audio continuity, and GPIO sideband effects.
- Don’t: detach sideband effects from subsystem interactions.
- Test intent: register timeline and sideband behavior tests.
- Evidence expected: PSG timing artifacts.
- Tag: `dev_allowed_now`

## Phase B checklist (Integration prerequisites)

### B-01 Arbitration integration closure (E2-S1-T2)
- Do: assert deterministic CPU/SHIFTER/DMA arbitration ordering.
- Don’t: use statistical wait-state approximations.
- Test intent: fixed-trace replay equality checks.
- Evidence expected: arbitration trace captures.
- Tag: `integration_blocked`

### B-02 Interrupt hierarchy closure (E2-S2-T2)
- Do: validate source priority and vector routing order.
- Don’t: modify interrupt hierarchy contract definitions.
- Test intent: end-to-end vector-order replay suite.
- Evidence expected: vector-order report bundle.
- Tag: `integration_blocked`

### B-03 Startup/reset closure (E2-S6-T1)
- Do: verify startup defaults and reset sequence behavior.
- Don’t: alter baseline startup tables to hide mismatches.
- Test intent: deterministic reset replay by profile.
- Evidence expected: startup baseline verification report.
- Tag: `integration_blocked`

### B-04 Conformance suites (E3-S1-T1)
- Do: execute per-subsystem suites with fixed fixtures.
- Don’t: add untracked check families without updating traceability.
- Test intent: deterministic pass/fail by check ID.
- Evidence expected: sectioned conformance reports.
- Tag: `integration_blocked`

### B-05 Evidence packaging (E3-S2-T1)
- Do: maintain 1:1 check-to-artifact mapping.
- Don’t: submit gate packet with missing check evidence rows.
- Test intent: completeness and reproducibility validation.
- Evidence expected: evidence index + traceability update.
- Tag: `integration_blocked`

## Phase C checklist (Release hard-validation)

### C-01 SLO stress closure (E3-S3-T1)
- Do: validate thresholds under stress scenarios.
- Don’t: weaken SLO definitions in architecture docs.
- Test intent: threshold breach/non-breach scenario runs.
- Evidence expected: metric bundles + alarm chronology.
- Tag: `release_blocked`

### C-02 Long-run soak closure
- Do: run sustained soak scenarios across media/input/stream churn.
- Don’t: claim release readiness on short smoke-only evidence.
- Test intent: drift/fault detection across long-duration runs.
- Evidence expected: soak logs + triage packet.
- Tag: `release_blocked`

### C-03 Production package trust closure
- Do: validate signing/trust enforcement for production packages.
- Don’t: treat reference validation packages as production trust proof.
- Test intent: trusted/untrusted package admission tests.
- Evidence expected: signing and trust validation report.
- Tag: `release_blocked`

### C-04 Release packet and sign-off closure
- Do: confirm no open P0 blockers and complete gate packet.
- Don’t: release with unresolved gate failures.
- Test intent: release checklist and approval verification.
- Evidence expected: final acceptance packet and sign-off record.
- Tag: `release_blocked`

## Acceptance criteria
1. Every checklist item is independently actionable in one cycle.
2. Every item includes Do/Don’t boundaries, test intent, evidence, and tag.
3. Phase sequencing aligns with dependency and gate documents.

## Evidence expected
- Completed checklist items with linked artifacts in tracking/captures.
- Gate decision entries that reference checklist completion.

## Cross-links
- [EBIN-ARCH-000 Index](EBIN-ARCH-000_INDEX.md)
- [EBIN-ARCH-003 Sprint-Ready Work Breakdown](EBIN-ARCH-003_SPRINT_READY_BREAKDOWN.md)
- [EBIN-INTF-001 Interface and Contract Matrix](EBIN-INTF-001_INTERFACE_CONTRACT_MATRIX.md)
- [EBIN-GATE-001 Integration Gates and Non-Go Conditions](EBIN-GATE-001_INTEGRATION_GATES_AND_NON_GO.md)

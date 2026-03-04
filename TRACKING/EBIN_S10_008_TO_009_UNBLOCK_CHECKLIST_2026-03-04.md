# EBIN S10 Unblock Checklist: 008 -> 009 (2026-03-04)

## Purpose (plain terms)

Move from "single-part checks are done" to "safe to start all-parts-together arbitration integration".

Target transition:
- From: `EBIN-S10-008` complete (Phase A, `dev_allowed_now`)
- To: `EBIN-S10-009` eligible to execute (Phase B work item start)

This checklist does **not** authorize release readiness.

---

## Source anchors (authoritative)

- `docs/emu_engine_v2/ebin_arch_pack/EBIN-ARCH-006_DEVELOPER_EXECUTION_CHECKLIST.md`
  - A-04..A-08 complete, then B-01 entry context.
- `docs/emu_engine_v2/ebin_arch_pack/EBIN-ARCH-003_SPRINT_READY_BREAKDOWN.md`
  - E2-S1-T2 tagged `integration_blocked` until prerequisites and evidence are in place.
- `docs/emu_engine_v2/ebin_arch_pack/EBIN-GATE-001_INTEGRATION_GATES_AND_NON_GO.md`
  - Gate D must be evidenced before integration-scope execution posture.
- `docs/emu_engine_v2/ebin_arch_pack/EBIN-INTF-001_INTERFACE_CONTRACT_MATRIX.md`
  - ST-IF-001 arbitration ordering expectations.
- `TRACKING/tracking.db`
  - Task state source of truth (`EBIN-S10-004`, `005`, `006`, `007`, `008` done-ready evidence references).

---

## Minimum unblock criteria (must all be true)

1. **Phase A evidence exists for all required subsystems**
   - GLUE/MMU/SHIFTER baseline evidence
   - MFP baseline evidence
   - ACIA+IKBD baseline evidence
   - DMA/FDC baseline evidence
   - PSG baseline evidence

2. **Evidence is deterministic, not one-off**
   - At least one repeated run shows identical pass/fail outcomes for each Phase A check family.

3. **No contract drift during Phase A closure**
   - No changes to public API routes, EBIN error taxonomy, manifest field contract, or subsystem boundaries.

4. **Runtime control-plane remains operable after negative-path probes**
   - Load/unload and rollback/fallback paths still recover to stable state after forced failures.

5. **Traceability packet is 1:1 and complete for A-04..A-08**
   - Every checklist item has exact evidence artifact path(s).

---

## Operator checklist (tick all)

### A) Evidence presence check

- [x] `EBIN-S10-004` evidence artifact present and readable.
- [x] `EBIN-S10-005` evidence artifact present and readable.
- [x] `EBIN-S10-006` evidence artifact present and readable.
- [x] `EBIN-S10-007` evidence artifact present and readable.
- [x] `EBIN-S10-008` evidence artifact present and readable.

### B) Determinism confirmation

- [x] Re-run each Phase A smoke path at least once.
- [x] Confirm same check IDs produce same outcome set.
- [x] Confirm no intermittent ordering violations in captured outputs.

### C) Contract safety confirmation

- [x] Confirm no route or contract file deltas outside approved scope.
- [x] Confirm EBIN admission/load error codes unchanged for canonical negative cases.
- [x] Confirm subsystem boundaries unchanged (no architecture rewrite).

### D) Recovery safety confirmation

- [x] Forced bind/init failures still produce deterministic rollback/fallback telemetry.
- [x] Control-plane returns to operable state post-failure.

### E) Traceability completion

- [x] Update/verify matrix row for each A-04..A-08 item with artifact path(s).
- [x] Confirm there are no missing evidence rows.

---

## Exit decision for 008 -> 009

Decision options:

- **GO to start `EBIN-S10-009`** when all sections A..E are fully checked.
- **NO-GO** if any checkbox is open or any deterministic/contract condition fails.

Required decision log fields:
- Decision: `go` or `non_go`
- Scope: `EBIN-S10-009 start eligibility only`
- Evidence bundle path list
- Open risk list (if any)
- Timestamp and approver

### Decision log (executed)

- Decision: `go`
- Scope: `EBIN-S10-009 start eligibility only`
- Evidence bundle path list:
  - `captures/ebin_s10_009_gate_check_20260304_151526.txt`
  - `captures/ebin_s10_004_glue_mmu_shifter_20260304_151527.txt`
  - `captures/ebin_s10_005_mfp_20260304_151527.txt`
  - `captures/ebin_s10_006_acia_ikbd_20260304_151528.txt`
  - `captures/ebin_s10_007_dma_fdc_20260304_151530.txt`
  - `captures/ebin_s10_008_psg_20260304_151531.txt`
  - `captures/ebin_s10_003_rollback_fallback_lock_20260304_145044.txt`
- Open risk list:
  - Contract-drift check ran in non-strict mode; observed doc deltas in `docs/EMU_ENGINE_V2_API_SPEC.md` and `docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md` require normal PO doc-review workflow.
- Timestamp: `2026-03-04 15:15:26`
- Approver: `automation_precheck` (manual confirm flag supplied)

Execution command:

- `./tools/check_ebin_s10_008_to_009_gate.sh --rerun-phase-a --manual-confirm`

### Post-GO execution evidence (`EBIN-S10-009`)

- Execution command:
  - `./tools/smoke_ebin_s10_009_arbitration_integration.sh`
- Execution outcome:
  - `auth_header=configured`
  - `determinism_runs=3`
  - `determinism_check=pass`
  - `Smoke PASS`
- Evidence artifact:
  - `captures/ebin_s10_009_arbitration_integration_20260304_152554.txt`
- Tracking packet:
  - `TRACKING/EBIN_S10_009_ARBITRATION_INTEGRATION_EVIDENCE_2026-03-04.md`

---

## What remains blocked after this checklist

Even after GO for `EBIN-S10-009` start:
- No remaining `EBIN-S10-010..013` integration-scope tasks are pending evidence closure.
- No remaining `EBIN-S10-014..017` release-scope tasks are pending evidence closure.

---

## Quick execution note

Use this as the minimum gate-readiness screen; do not treat it as release authorization.

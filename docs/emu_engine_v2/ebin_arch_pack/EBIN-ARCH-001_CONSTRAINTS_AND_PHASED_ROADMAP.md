# EBIN-ARCH-001 Constraints and Phased Roadmap

## Purpose
Synthesize current constraints from MVP, Emulation Engine V2, and tracking artifacts, then define a minimal phased EBIN architecture roadmap.

## Scope
- Atari ST baseline EBIN architecture execution plan (no contract changes)
- Phases:
  - Phase A: safe-to-start development architecture
  - Phase B: integration architecture prerequisites
  - Phase C: release hard-validation architecture

## Dependencies
- `docs/emu_engine_v2/EBIN_RUNTIME_ABI_V1.md`
- `docs/emu_engine_v2/02_system_block_diagram.md`
- `docs/emu_engine_v2/03_component_spec_table.md`
- `TRACKING/PROGRAM_STATUS_SNAPSHOT_2026-03-04.md`
- `TRACKING/S5_RUNTIME_UNLOCK_PACKET_2026-03-04.md`
- `TRACKING/S5_RUNTIME_UNLOCK_TRACEABILITY_MATRIX_2026-03-04.md`
- `TRACKING/_archive/legacy_markdown_2026-03-03/MVP_DEFINITION.md`

## Interfaces
- Inputs:
  - Contract baseline (`EBIN_RUNTIME_ABI_V1`)
  - Existing subsystem contracts (`02`, `03`, `04`, `05`, appendices)
  - Tracking gates and acceptance state
- Outputs:
  - Phase plan with explicit allow/block tags
  - Decision register for approval-required changes

## Current constraints (authoritative)

1. ABI/runtime unlock baseline is complete and accepted (`S5-001..S5-008`).
2. Contract-first delivery is mandatory; no redesign or contract drift is allowed without written approval.
3. Current unlock posture is compatible with `unlock_with_conditions` discipline (deterministic evidence, rollback, explicit gate control).
4. S5 reference package is validation-grade, not production-signing proof.
5. Hardware long-run soak and production hardening are outside S5 closure and remain release blockers.
6. ST subsystem behavioral contract must stay aligned with V2 component specifications.

## Phased architecture roadmap

### Phase A — safe-to-start development architecture

Objective: enable independent, one-cycle development tasks on stable interfaces.

- A1. Use `EBIN_RUNTIME_ABI_V1` as immutable load contract baseline.
- A2. Implement subsystem adapters behind existing component contracts only (CPU/MMU/GLUE/SHIFTER/MFP/ACIA/IKBD/DMA/FDC/PSG).
- A3. Keep runtime path deterministic: `resolve -> validate -> gate -> bind -> init` and inverse unload pipeline.
- A4. Produce deterministic unit/harness evidence for each task before integration requests.

Tag: `dev_allowed_now`

### Phase B — integration architecture prerequisites

Objective: reduce integration risk before broader runtime/hardware coupling.

- B1. Verify cross-subsystem interrupt/vector ordering against V2 appendix contracts.
- B2. Verify bus arbitration ordering across CPU/SHIFTER/DMA/(Blitter when enabled).
- B3. Validate startup/reset defaults and restore compatibility boundaries.
- B4. Gate integration on reproducible fixture bundles and deterministic replay output.

Tag: `integration_blocked` until B1-B4 evidence is complete.

### Phase C — release hard-validation architecture

Objective: authorize release only after non-functional and field-risk evidence closure.

- C1. Long-run soak under representative media/input/stream workloads.
- C2. Production-signing and integrity policy closure for EBIN packages.
- C3. Performance SLO closure under stress (latency/jitter/frame-drop) with thresholds.
- C4. Final release packet with no open P0 blockers and explicit sign-off.

Tag: `release_blocked` until C1-C4 evidence is complete.

## Decision register

No architectural change request is required at this time.

If future change is required, use this approval template exactly:

- Required change:
- Reason:
- Minimal alternatives considered:
- Recommended option:
- Approval text required:
  - `APPROVED: authorize change <CHANGE_ID> to <CONTRACT_OR_ARCH_DOC>, effective <DATE>, scope limited to <BOUNDARY>.`

## Acceptance criteria
1. Constraints are traceable to current S5/MVP/V2 sources.
2. Phase A/B/C boundaries are explicit and non-overlapping.
3. Each phase has clear allow/block semantics.

## Evidence expected
- Traceable links to the seven dependency docs listed above.
- Phase-level checklist evidence in `EBIN-GATE-001` and task-level evidence in `EBIN-ARCH-003`.

## Cross-links
- [EBIN-ARCH-000 Index](EBIN-ARCH-000_INDEX.md)
- [EBIN-ARCH-002 Subsystem Decomposition and Dependency Order](EBIN-ARCH-002_SUBSYSTEM_DECOMPOSITION_AND_ORDER.md)
- [EBIN-ARCH-003 Sprint-Ready Work Breakdown](EBIN-ARCH-003_SPRINT_READY_BREAKDOWN.md)
- [EBIN-GATE-001 Integration Gates and Non-Go Conditions](EBIN-GATE-001_INTEGRATION_GATES_AND_NON_GO.md)

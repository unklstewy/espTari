# EBIN-GATE-001 Integration Gates and Non-Go Conditions

## Purpose
Define explicit development, integration, and release gates for EBIN/ST architecture execution, including hard non-go conditions.

## Scope
- Gate criteria and evidence requirements
- Non-go conditions by scope
- Escalation/approval path when architectural change is required

## Dependencies
- `docs/emu_engine_v2/ebin_arch_pack/EBIN-ARCH-001_CONSTRAINTS_AND_PHASED_ROADMAP.md`
- `docs/emu_engine_v2/ebin_arch_pack/EBIN-INTF-001_INTERFACE_CONTRACT_MATRIX.md`
- `TRACKING/S5_RUNTIME_UNLOCK_PACKET_2026-03-04.md`
- `TRACKING/KANBAN_BOARD.md`
- `TRACKING/ACCEPTANCE_LOG.md`

## Interfaces
- Input: task evidence bundles + traceability matrix + acceptance updates
- Output: gate decision (`go`, `go_with_conditions`, `non_go`) with rationale

## Gate model

### Gate D (Development scope)

Entry criteria:
- S5 ABI/runtime unlock baseline remains intact.
- Task has explicit Do/Don’t and deterministic test intent.

Exit criteria:
- Local deterministic checks pass.
- No contract drift in ABI/error taxonomy.

Non-go conditions:
1. Task proposes field/semantic changes to `EBIN_RUNTIME_ABI_V1` without approval.
2. Error code mapping differs from canonical deterministic mapping.
3. Evidence artifact cannot be reproduced from fixed fixture inputs.

Default tag: `dev_allowed_now`

### Gate I (Integration scope)

Entry criteria:
- Gate D passed for participating subsystems.
- Integration fixtures for interrupts/arbitration/reset are prepared and versioned.

Exit criteria:
- Cross-subsystem ordering checks pass (interrupt vectors, bus arbitration, reset/startup).
- Evidence packet has 1:1 check-to-artifact traceability.

Non-go conditions:
1. Interrupt/vector ordering mismatch against appendix contract.
2. Arbitration ordering is non-deterministic for identical fixture inputs.
3. Reset/startup baseline mismatch for selected machine profile.

Default tag: `integration_blocked` until Gate I exits pass.

### Gate R (Release scope)

Entry criteria:
- Gate I passed with accepted evidence.
- Release hard-validation plan approved.

Exit criteria:
- Long-run soak closure complete with no unresolved P0 failures.
- Production-signing trust policy validation complete.
- SLO stress evidence meets threshold closure targets.
- Acceptance sign-off recorded.

Non-go conditions:
1. Production package trust chain unresolved.
2. Soak failures with unresolved P0/P1 runtime risks.
3. Missing/partial SLO stress evidence for required profiles.

Default tag: `release_blocked` until Gate R exits pass.

## Approval-required change protocol

If any gate cannot be passed without architecture/contract change, stop execution and submit:

- Required change
- Reason
- Minimal alternatives
- Recommended option
- Exact approval text required:
  - `APPROVED: authorize change <CHANGE_ID> to <CONTRACT_OR_ARCH_DOC>, effective <DATE>, scope limited to <BOUNDARY>.`

## Acceptance criteria
1. Gate criteria and non-go conditions are explicit per scope.
2. Gate decisions are evidence-based and reproducible.
3. Approval protocol is explicit and blocks silent contract drift.

## Evidence expected
- Gate D/I/R decision logs with linked evidence artifacts.
- Updated tracking entries for each gate transition decision.

## Cross-links
- [EBIN-ARCH-000 Index](EBIN-ARCH-000_INDEX.md)
- [EBIN-ARCH-001 Constraints and Phased Roadmap](EBIN-ARCH-001_CONSTRAINTS_AND_PHASED_ROADMAP.md)
- [EBIN-ARCH-003 Sprint-Ready Work Breakdown](EBIN-ARCH-003_SPRINT_READY_BREAKDOWN.md)
- [EBIN-ARCH-004 Determinism, Performance, Observability Plan](EBIN-ARCH-004_DETERMINISM_PERF_OBSERVABILITY_PLAN.md)

# Task Cards: Sprint 08 Phase-6 Active Profile Enablement (S8-001 through S8-008)

This file decomposes Sprint 08 into pullable implementation tasks.

Phase intent:
- Convert Sprint 07 conditional acceptance into active multi-profile runtime execution.
- Keep slices XS/S with deterministic evidence expectations.
- Preserve canonical guards while promoting non-baseline profiles from blocked to active-run paths.

## TASK-ID: S8-001

- Epic: EPIC-09
- Objective: Enable runtime manifests/wiring selectors for `mega_st_pal`, `ste_pal`, and `mega_ste_pal`.
- Dependencies: S7-008
- Scope:
  - Add/activate runtime profile manifest path resolution for non-baseline profiles.
  - Wire profile selectors into session bootstrap flow with deterministic diagnostics.
  - Confirm canonical errors are preserved for invalid machine/profile combinations.

Acceptance criteria:
1. Non-baseline profile session bootstrap no longer returns `MACHINE_PROFILE_NOT_FOUND` when valid.
2. Invalid machine/profile requests still return canonical deterministic guards.
3. Harness probes show deterministic startup behavior across repeated runs.

Evidence required:
- Profile manifest/wiring activation matrix.
- Bootstrap guard/error capture.
- Deterministic repeated-run probe output.

## TASK-ID: S8-002

- Epic: EPIC-09
- Objective: Validate active Mega ST bootstrap and lifecycle/API parity.
- Dependencies: S8-001
- Scope:
  - Execute active Mega ST start/session path and lifecycle transitions.
  - Validate parity of core session/status payload fields versus Atari ST baseline.
  - Validate lifecycle guard semantics remain canonical.

Acceptance criteria:
1. Mega ST starts as active profile path with deterministic lifecycle behavior.
2. Session/status payload parity checks pass for required baseline fields.
3. Canonical lifecycle guards remain stable for invalid transitions.

Evidence required:
- Mega ST active lifecycle parity capture.
- Status payload parity summary.
- Guard/error mapping capture.

## TASK-ID: S8-003

- Epic: EPIC-09
- Objective: Validate active STe extension controls and profile-gated payload semantics.
- Dependencies: S8-001
- Scope:
  - Validate active STe extension controls on stream/control and related APIs.
  - Validate profile-gated field presence/absence rules.
  - Validate invalid extension input mapping to canonical deterministic errors.

Acceptance criteria:
1. STe extension control payloads are active, deterministic, and contract-aligned.
2. Profile-gated fields are present only when applicable.
3. Invalid extension inputs emit canonical deterministic guards.

Evidence required:
- STe active extension capture set.
- Field-gating compatibility summary.
- Invalid-input guard capture.

## TASK-ID: S8-004

- Epic: EPIC-09
- Objective: Validate active Mega STe extension compatibility deltas versus STe baseline.
- Dependencies: S8-003
- Scope:
  - Validate Mega STe extension fields and capability reporting in active path.
  - Validate deterministic delta behavior relative to active STe baseline.
  - Validate fallback/error semantics for unsupported extension requests.

Acceptance criteria:
1. Mega STe extension responses are active-path deterministic and contract-aligned.
2. Compatibility deltas versus STe are explicit and verified.
3. Unsupported extension requests emit canonical fallback semantics.

Evidence required:
- Mega STe active extension capture set.
- Compatibility-delta matrix.
- Fallback behavior capture.

## TASK-ID: S8-005

- Epic: EPIC-09
- Objective: Validate cross-profile media/catalog/session run-path parity for active profiles.
- Dependencies: S8-002, S8-004
- Scope:
  - Validate ROM/disk/catalog resolution for all active profiles.
  - Validate session continuity through attach/eject flows per active profile.
  - Validate deterministic blocker semantics for missing/incompatible media.

Acceptance criteria:
1. Media/catalog run paths are deterministic across active profiles.
2. Session continuity checks pass after attach/eject operations.
3. Blocker/error semantics remain canonical and repeatable.

Evidence required:
- Active profile media parity matrix.
- Attach/eject continuity capture.
- Blocker/error mapping log.

## TASK-ID: S8-006

- Epic: EPIC-09
- Objective: Execute active pairwise ABI compatibility and regression guard suite.
- Dependencies: S8-005
- Scope:
  - Run pairwise compatibility matrix across all active profiles.
  - Validate ABI/profile guard semantics and deterministic compatibility behavior.
  - Re-run baseline regression envelope checks from S5/S6/S7.

Acceptance criteria:
1. Active pairwise matrix produces deterministic outcomes per profile pair.
2. ABI/profile guard semantics match canonical envelopes.
3. No regressions in previously accepted baseline envelope checks.

Evidence required:
- Active pairwise compatibility matrix output.
- ABI/profile guard capture.
- Regression-check summary.

## TASK-ID: S8-007

- Epic: EPIC-09
- Objective: Validate active profile-switch isolation and fallback semantics.
- Dependencies: S8-006
- Scope:
  - Validate switch transitions across active profiles without state leakage.
  - Validate deterministic fallback behavior for incompatible switch requests.
  - Validate post-switch control-plane operability and profile-consistent status.

Acceptance criteria:
1. Profile-switch isolation checks pass deterministically across repeated runs.
2. Fallback behavior is deterministic and canonically guarded.
3. Post-switch control-plane probes remain healthy and profile-consistent.

Evidence required:
- Active profile-switch isolation capture.
- Fallback guard verification log.
- Post-switch health/status evidence.

## TASK-ID: S8-008

- Epic: EPIC-09
- Objective: Assemble Sprint 08 active-profile evidence packet and PO decision handoff.
- Dependencies: S8-006, S8-007
- Scope:
  - Consolidate S8 evidence into a decision-ready packet.
  - Produce objective-to-evidence traceability matrix.
  - Publish residual risks, owner-tagged next actions, and PO signoff block.

Acceptance criteria:
1. Packet provides complete 1:1 objective-to-evidence mapping for S8.
2. Residual risks and follow-up actions are explicit and owner-tagged.
3. PO handoff artifacts are complete and review-ready.

Evidence required:
- Sprint 08 packet.
- Sprint 08 traceability matrix.
- Sprint 08 decision template.

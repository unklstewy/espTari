# Task Cards: Sprint 07 Phase-5 Multi-Machine Enablement (S7-001 through S7-008)

This file decomposes Sprint 07 into pullable implementation tasks.

Phase intent:
- Execute implementation-guide phase-5 multi-machine enablement after S6 hardening closure.
- Keep slices XS/S with deterministic evidence expectations.
- Preserve backward ABI/profile compatibility through explicit regression gates.

## TASK-ID: S7-001

- Epic: EPIC-09
- Objective: Build multi-machine profile harness baseline and deterministic profile contract matrix.
- Dependencies: S6-008
- Scope:
  - Define supported profile matrix (`atari_st`, `mega_st`, `ste`, `mega_ste`) and canonical selection semantics.
  - Define deterministic harness entrypoints for profile-parity checks.
  - Define artifact naming and matrix schema for S7 tasks.

Acceptance criteria:
1. Harness can execute deterministic profile selection/parity probes across supported profiles.
2. Profile matrix captures required lifecycle/media/streaming capability fields.
3. Artifact naming and bundle schema are reusable by downstream S7 tasks.

Evidence required:
- Multi-machine profile matrix artifact.
- Harness script baseline.
- Baseline profile sweep capture.

## TASK-ID: S7-002

- Epic: EPIC-09
- Objective: Validate Mega ST profile bootstrap and lifecycle/API parity.
- Dependencies: S7-001
- Scope:
  - Execute Mega ST profile bootstrap/session start path.
  - Validate lifecycle parity (`start/pause/resume/reset/stop`) with canonical guards.
  - Validate parity of core status payload fields against Atari ST baseline profile contract.

Acceptance criteria:
1. Mega ST profile bootstrap is deterministic and guard-safe.
2. Lifecycle transition semantics match baseline contract envelopes.
3. Core status payload parity checks pass for required fields.

Evidence required:
- Mega ST lifecycle parity capture.
- Guard/error mapping capture.
- Status payload parity summary.

## TASK-ID: S7-003

- Epic: EPIC-09
- Objective: Validate Mega ST media/catalog/session run-path parity.
- Dependencies: S7-002
- Scope:
  - Validate ROM/disk/catalog resolution behavior under Mega ST profile context.
  - Validate session run-path continuity through media attach/eject and catalog lookup flows.
  - Validate deterministic blocker/error semantics for missing/incompatible media paths.

Acceptance criteria:
1. Mega ST media/catalog run path matches baseline contract semantics.
2. Session continuity remains deterministic after media operations.
3. Canonical blockers are emitted for invalid/missing media scenarios.

Evidence required:
- Mega ST media parity capture.
- Catalog/attach-eject transition capture.
- Blocker/error mapping log.

## TASK-ID: S7-004

- Epic: EPIC-09
- Objective: Validate STe profile extension controls (audio/video deltas).
- Dependencies: S7-001
- Scope:
  - Validate STe-specific control payloads for added audio/video capabilities.
  - Validate profile-aware field gating and deterministic guard semantics.
  - Validate backwards-compatible response envelopes when extension fields are absent.

Acceptance criteria:
1. STe extension controls return contract-aligned payloads with deterministic fields.
2. Invalid extension usage maps to canonical deterministic guards.
3. Backward-compatible envelope behavior is preserved.

Evidence required:
- STe extension control captures.
- Guard/error semantics capture.
- Backward-envelope compatibility samples.

## TASK-ID: S7-005

- Epic: EPIC-09
- Objective: Validate Mega STe profile extension compatibility deltas.
- Dependencies: S7-004
- Scope:
  - Validate Mega STe extension fields and profile capability reporting.
  - Validate deterministic compatibility behavior versus STe baseline extension contract.
  - Validate fallback semantics for unsupported extension requests.

Acceptance criteria:
1. Mega STe extension responses are deterministic and contract-aligned.
2. Compatibility deltas versus STe are explicit and verified.
3. Unsupported extension requests emit canonical fallback/guard semantics.

Evidence required:
- Mega STe extension capture set.
- Compatibility delta summary.
- Fallback behavior capture.

## TASK-ID: S7-006

- Epic: EPIC-09
- Objective: Execute cross-profile ABI compatibility and regression guard suite.
- Dependencies: S7-003, S7-005
- Scope:
  - Execute compatibility matrix across Atari ST, Mega ST, STe, and Mega STe profiles.
  - Validate ABI/profile guard semantics and deterministic downgrade/compatibility behavior.
  - Validate regression checks for prior S5/S6 API envelopes under multi-profile execution.

Acceptance criteria:
1. Compatibility matrix produces deterministic pass/fail outputs per profile pair.
2. ABI/profile guard semantics match canonical envelopes.
3. No regressions in previously accepted baseline envelope checks.

Evidence required:
- Cross-profile compatibility matrix output.
- ABI/profile guard capture.
- Regression-check summary.

## TASK-ID: S7-007

- Epic: EPIC-09
- Objective: Validate profile-switch state isolation and backward-compat fallback semantics.
- Dependencies: S7-006
- Scope:
  - Validate profile-switch transitions do not leak incompatible state across profile boundaries.
  - Validate deterministic fallback semantics during profile downgrade/unsupported requests.
  - Validate post-switch control-plane operability and profile-consistent status envelopes.

Acceptance criteria:
1. Profile-switch isolation checks are deterministic and pass across repeated runs.
2. Fallback behavior is deterministic and canonically guarded.
3. Post-switch control-plane probes remain healthy and profile-consistent.

Evidence required:
- Profile-switch isolation capture.
- Fallback guard verification log.
- Post-switch health/status probe evidence.

## TASK-ID: S7-008

- Epic: EPIC-09
- Objective: Assemble Sprint 07 multi-machine evidence packet and PO decision handoff.
- Dependencies: S7-006, S7-007
- Scope:
  - Consolidate S7 evidence into a decision-ready packet.
  - Produce objective-to-evidence traceability matrix.
  - Publish explicit residual risks, owners, and next actions.

Acceptance criteria:
1. Packet provides complete 1:1 objective-to-evidence mapping for S7.
2. Risks and follow-up actions are explicit and owner-tagged.
3. PO handoff artifacts are complete and review-ready.

Evidence required:
- Sprint 07 packet.
- Sprint 07 traceability matrix.
- Sprint 07 decision template.

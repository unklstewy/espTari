# PRQ-003 Deployment Workflow Runbook

Date: 2026-03-02
Task: PRQ-003
Phase: Runtime-backed preflight validation

## Objective

Document and execute reproducible deployment preflight validation and rollback-readiness checks for firmware workflow control.

## Environment Prerequisites Matrix

| Category | Requirement | Version/Constraint | Owner |
|---|---|---|---|
| Toolchain | ESP-IDF toolchain availability | Workspace-configured and documented version pin | Engineering |
| Build inputs | Target configuration and partition config | Baseline config captured and versioned | Engineering |
| Runtime target | Device/network endpoint readiness assumptions | Declared but not exercised in this phase | Engineering + QA |
| Evidence storage | Tracking artifact paths and naming | TRACKING/PRQ_WORKING and acceptance-link convention | Engineering |

## Deterministic Deployment Workflow (Planned)

1. Preflight validation
   - Verify documented toolchain/config versions against pinned references.
   - Confirm artifact output paths and naming conventions.

2. Build/package planning
   - Define deterministic build input set and expected output identifiers.
   - Define traceability mapping from build inputs to planned runtime vectors.

3. Deployment sequencing plan
   - Define ordered deployment actions and gate checks between steps.
   - Define explicit abort conditions and safe-stop points.

4. Post-deployment checkpoint plan
   - Define expected readiness checks before runtime validation phase.
   - Define checkpoint evidence templates for future execution phase.

## Rollback Procedure (Planned)

- Restore last known-good baseline configuration bundle.
- Revert planned deployment artifacts to baseline revision set.
- Re-run preflight checklist to confirm return-to-baseline state.
- Record rollback rationale and affected vectors in tracking notes.

## Verification Checkpoints (Pre-runtime)

- Checkpoint A: prerequisites completeness review.
- Checkpoint B: sequencing-plan traceability review.
- Checkpoint C: rollback-plan completeness review.

## Automation and Evidence

- Preflight script: `tools/prq/run_prq003_preflight.sh`
- Evidence output: `captures/prq003_preflight_20260302_184958.txt`
- Result summary: all required command/file checks passed with zero failures.

## Status

Runbook preflight is executed and evidenced; deployment sequencing remains governed by unlock decision flow.

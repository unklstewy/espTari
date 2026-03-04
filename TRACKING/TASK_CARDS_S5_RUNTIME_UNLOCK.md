# Task Cards: S5 Runtime Unlock Execution (S5-001 through S5-008)

This file decomposes S5 runtime unlock into pullable engineering tasks.

Phase intent:
- Runtime implementation and verification phase.
- Contract-first, evidence-backed increments.
- Each task must produce deterministic smoke/check artifacts before `Done`.

## TASK-ID: S5-001

- Epic: EPIC-06
- Objective: Define EBIN runtime ABI contract v1 and compatibility matrix for load-time enforcement.
- Dependencies: PRQ-004
- Scope:
  - Define required EBIN metadata fields and ABI version semantics.
  - Define host/runtime compatibility checks and rejection rules.
  - Define deterministic error mappings for ABI mismatch cases.

Acceptance criteria:
1. ABI contract doc includes required fields, version range semantics, and compatibility rules.
2. Contract maps incompatibility cases to deterministic error codes.
3. Traceability links to implementation touchpoints are documented.

Evidence required:
- ABI contract document update.
- Compatibility matrix artifact.
- Traceability map.

## TASK-ID: S5-002

- Epic: EPIC-06
- Objective: Implement EBIN manifest/schema validator and canonical load-time validation responses.
- Dependencies: S5-001
- Scope:
  - Implement validator for required fields, formats, and version constraints.
  - Return deterministic `BAD_REQUEST` vs `EBIN_INVALID` mappings.
  - Add validator-focused smoke checks.

Acceptance criteria:
1. Invalid manifests are rejected deterministically with canonical envelopes.
2. Valid manifests pass validation and produce normalized internal model.
3. Smoke evidence covers success + key failure classes.

Evidence required:
- Validator implementation diff.
- Smoke script and capture artifact.
- Guard/error mapping log.

## TASK-ID: S5-003

- Epic: EPIC-06
- Objective: Implement loader safety gates for integrity/signature/dependency compatibility checks.
- Dependencies: S5-002
- Scope:
  - Add pre-load gate sequence for hash/signature/dependency checks.
  - Ensure fail-fast rejection prior to runtime binding.
  - Emit deterministic diagnostics for each gate.

Acceptance criteria:
1. Gate checks execute in deterministic order and fail-fast on first violation.
2. Each gate failure maps to deterministic error code/details.
3. Success path passes all gates and reaches bind-ready state.

Evidence required:
- Loader gate implementation diff.
- Gate-order and failure-path smoke artifacts.
- Check-to-error mapping table.

## TASK-ID: S5-004

- Epic: EPIC-06
- Objective: Implement EBIN catalog/index resolver for machine/component/version selection.
- Dependencies: S5-001
- Scope:
  - Build SD-card index reader and deterministic resolution strategy.
  - Resolve module set by machine profile + component role + version policy.
  - Return deterministic missing/ambiguous resolution diagnostics.

Acceptance criteria:
1. Resolver selects deterministic module set from catalog inputs.
2. Missing/ambiguous entries return canonical deterministic errors.
3. Resolution behavior is covered by smoke checks.

Evidence required:
- Resolver implementation diff.
- Deterministic resolution test artifact.
- Catalog input/output examples.

## TASK-ID: S5-005

- Epic: EPIC-06
- Objective: Implement runtime EBIN load/unload orchestration and state transition hooks.
- Dependencies: S5-003, S5-004
- Scope:
  - Implement load pipeline (resolve -> validate -> bind -> init).
  - Implement unload pipeline (pause -> drain -> deinit -> release).
  - Wire lifecycle/session transition integration checks.

Acceptance criteria:
1. Load/unload transitions are deterministic and lifecycle-safe.
2. Partial failures leave runtime in defined recoverable state.
3. Smoke evidence covers load, unload, and expected failure branches.

Evidence required:
- Orchestration implementation diff.
- Transition smoke captures.
- Failure-mode state table.

## TASK-ID: S5-006

- Epic: EPIC-06
- Objective: Implement rollback/fallback behavior and fault telemetry for failed module activation.
- Dependencies: S5-005
- Scope:
  - Add rollback to last-known-good module set.
  - Add fallback path and explicit fault-state telemetry.
  - Ensure failure leaves control plane operable.

Acceptance criteria:
1. Failed activation triggers deterministic rollback/fallback sequence.
2. Fault telemetry surfaces cause and recovery outcome.
3. Recovery path is validated by deterministic smoke evidence.

Evidence required:
- Rollback/fallback implementation diff.
- Recovery smoke captures.
- Fault telemetry sample artifact.

## TASK-ID: S5-007

- Epic: EPIC-06
- Objective: Build EBIN runtime unlock smoke harness and minimal reference module package.
- Dependencies: S5-005, S5-006
- Scope:
  - Add end-to-end smoke harness for load/unload/rollback scenarios.
  - Produce minimal reference EBIN package for deterministic checks.
  - Generate evidence bundle suitable for unlock review.

Acceptance criteria:
1. Harness runs deterministic end-to-end scenarios without manual edits.
2. Reference module package supports repeatable success/failure probes.
3. Evidence bundle captures key S5 checks for review consumption.

Evidence required:
- Smoke harness scripts.
- Reference package manifest and metadata.
- Captured run artifacts.

## TASK-ID: S5-008

- Epic: EPIC-06
- Objective: Assemble S5 runtime unlock review packet and decision-ready acceptance handoff.
- Dependencies: S5-007
- Scope:
  - Consolidate S5-001..S5-007 evidence into decision packet.
  - Map prerequisites/checks to artifacts with explicit pass/fail status.
  - Prepare PO/Acceptance decision template.

Acceptance criteria:
1. Review packet provides 1:1 check-to-evidence traceability.
2. Open risks/conditions are explicit with owners and next actions.
3. Decision package is complete for PO/Acceptance review.

Evidence required:
- Unlock packet index.
- Traceability matrix.
- Decision template artifact.

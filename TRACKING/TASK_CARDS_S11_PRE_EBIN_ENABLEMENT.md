# Task Cards: Sprint 11 Pre-EBIN Enablement (S11-001 through S11-008)

This sprint converts S10 readiness into hard prerequisites before active emulated-EBIN-device development.

## TASK-ID: S11-001

- Epic: EPIC-12
- Objective: Implement register snapshot/stream parity and bus/memory filtered trace executable runtime paths.
- Dependencies: S10-005
- Scope:
  - Implement/enable runtime path for register snapshot + stream parity checks.
  - Implement/enable runtime path for bus/memory filtered trace checks.
  - Provide deterministic smoke harness and evidence bundle.

Acceptance criteria:
1. `GATE-S11-05` and `GATE-S11-06` move from blocked to executable hard-validation checks.
2. At least one smoke bundle is produced with deterministic pass/fail semantics.
3. Artifact schema is stable and linked in tracking docs.

Evidence required:
- Smoke run log + bundle.
- Runtime path implementation references.

## TASK-ID: S11-002

- Epic: EPIC-12
- Objective: Enforce and validate SD-only media resolver/runtime policy for milestone criteria.
- Dependencies: S11-001
- Scope:
  - Implement/verify SD-only resolver checks for ROM/disk/cartridge load paths.
  - Add deterministic rejection paths for non-SD resolution attempts.
  - Produce first hard pass/fail validation run for `GATE-S11-02`.

Acceptance criteria:
1. SD-only policy is executable and testable at runtime.
2. Non-SD requests are deterministically rejected.
3. `GATE-S11-02` receives current-cycle hard-validation evidence.

Evidence required:
- SD-only validation capture + bundle.

## TASK-ID: S11-003

- Epic: EPIC-12
- Objective: Freeze EBIN ABI v1 + manifest compatibility contract for first emulated-device development.
- Dependencies: S11-001
- Scope:
  - Define/freeze EBIN ABI and manifest schema versions used by first emulated EBIN devices.
  - Implement deterministic validator outputs for missing/incompatible/dependency-missing module chains.
  - Publish compatibility matrix and developer contract note.

Acceptance criteria:
1. ABI/manifest schema version is explicit and frozen for S11.
2. Validator emits canonical EBIN guard codes deterministically.
3. Compatibility matrix is published and referenced by S11 development tasks.

Evidence required:
- ABI/manifest contract artifact.
- Compatibility matrix artifact.

## TASK-ID: S11-004

- Epic: EPIC-12
- Objective: Reconfirm S9-002 security invariants under S11 runtime changes.
- Dependencies: S11-002, S11-003
- Scope:
  - Re-run auth/scope/path/upload/EBIN-integrity probes after S11 runtime changes.
  - Verify audit event completeness for EBIN and file/system flows.
  - Produce refreshed security regression bundle.

Acceptance criteria:
1. S9-002 control set remains pass under S11 runtime.
2. Any regression has deterministic remediation owner/action.
3. Refreshed security evidence bundle is attached to S11 records.

Evidence required:
- Security regression run log + bundle.

## TASK-ID: S11-005

- Epic: EPIC-12
- Objective: Execute current-cycle reconfirmation fixtures for conditional criteria (`07`, `09`, `10`, `12`).
- Dependencies: S11-001, S11-004
- Scope:
  - Run input translation, browser capture mode, catalog missing-asset flow, and save/restore compatibility fixtures.
  - Produce deterministic current-cycle captures replacing archive-only evidence.

Acceptance criteria:
1. Each target criterion has current-cycle evidence capture.
2. Evidence is reproducible and linked in gate report inputs.
3. Conditional statuses are replaced by explicit hard-validation outcomes.

Evidence required:
- Four fixture capture bundles + summary index.

## TASK-ID: S11-006

- Epic: EPIC-12
- Objective: Produce sustained SLO hard-threshold validation report (`GATE-S11-13`).
- Dependencies: S11-001, S11-005
- Scope:
  - Run sustained sampling windows for latency/jitter/drop-frame metrics.
  - Compare against milestone thresholds and emit deterministic pass/fail outputs.
  - Separate instrumentation failure vs product threshold failure in report.

Acceptance criteria:
1. Sustained sample windows are executed and captured.
2. Threshold comparisons are explicit and deterministic.
3. Hard-validation SLO report is generated and linked.

Evidence required:
- SLO sustained-run report + raw capture artifacts.

## TASK-ID: S11-007

- Epic: EPIC-12
- Objective: Execute hard-validation release-gate run for Section 11 and publish decision report.
- Dependencies: S11-002, S11-003, S11-004, S11-005, S11-006
- Scope:
  - Execute full gate checklist with deterministic pass/fail outcomes.
  - Produce gate decision report with evidence links and follow-up obligations.
  - Decide `pass`, `pass_with_conditions`, or `fail`.

Acceptance criteria:
1. Full Section-11 gate run is executed with current-cycle evidence.
2. Decision outcome is justified with artifact links.
3. Follow-up obligations are explicit and owner-assigned.

Evidence required:
- Hard-validation gate report.
- Gate bundle index.

## TASK-ID: S11-008

- Epic: EPIC-12
- Objective: Assemble S11 packet and declare readiness to start active emulated EBIN device development.
- Dependencies: S11-007
- Scope:
  - Compile requirement traceability and S11 closure packet.
  - Publish explicit go/no-go statement for starting EBIN device coding.
  - Record acceptance/decision entry in tracking docs.

Acceptance criteria:
1. S11 packet includes requirement traceability and gate outcome.
2. Go/no-go statement is explicit and evidence-backed.
3. Tracking status reflects final S11 decision.

Evidence required:
- S11 packet artifact.
- S11 decision template/report.

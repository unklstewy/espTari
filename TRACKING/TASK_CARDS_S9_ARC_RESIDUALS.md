# Task Cards: Sprint 09 ARC Residual Hardening/Operations (S9-001 through S9-005)

This file decomposes architecture residual work into pullable tasks after S8 closure.

Phase intent:
- Keep DB as source of truth while converting post-closure residuals into explicit tracked work.
- Convert architecture risks and immediate-action obligations into deterministic recurring operations.
- Keep slices XS/S and evidence-driven.

## TASK-ID: S9-001

- Epic: EPIC-10
- Objective: Establish API contract delta changelog discipline and release-note workflow.
- Dependencies: S8-008
- Scope:
  - Define required changelog entries for API contract updates.
  - Define update workflow linking API spec deltas to release notes and acceptance log entries.
  - Publish operator checklist for contract-change traceability.

Acceptance criteria:
1. A deterministic changelog template is published and referenced from tracking docs.
2. API contract updates can be traced to release notes with explicit delta entries.
3. Checklist can be executed end-to-end on a sample contract change.

Evidence required:
- Changelog template artifact.
- Traceability walk-through sample.
- Checklist execution capture.

## TASK-ID: S9-002

- Epic: EPIC-10
- Objective: Execute security/integrity closure audit for auth path-hardening upload limits and EBIN integrity logging.
- Dependencies: S9-001
- Scope:
  - Audit current auth mode posture versus API spec security model.
  - Audit SD-card path normalization/traversal protections and upload bounds.
  - Audit EBIN integrity validation and audit-log event completeness.

Acceptance criteria:
1. Security closure report lists pass/fail findings per required control.
2. Any control gaps have deterministic remediation actions with owners.
3. Audit evidence is reproducible and linked to concrete checks.

Evidence required:
- Security/integrity closure report.
- Per-control check matrix and findings.
- Remediation action list (if gaps found).

## TASK-ID: S9-003

- Epic: EPIC-10
- Objective: Operationalize architecture risk mitigations into recurring validation checks and evidence format.
- Dependencies: S9-001
- Scope:
  - Map implementation-plan risk mitigations to executable checks.
  - Define recurring cadence and pass/fail thresholds per check.
  - Standardize evidence bundle format for recurring runs.

Acceptance criteria:
1. Each architecture risk has at least one deterministic validation check.
2. Thresholds and failure signals are defined and reproducible.
3. Evidence bundle format is fixed and reusable across runs.

Evidence required:
- Risk-to-check matrix.
- Threshold definition sheet.
- Evidence bundle schema/template.

## TASK-ID: S9-004

- Epic: EPIC-10
- Objective: Establish recurring soak and SLO regression cadence with ownership and runbook schedule.
- Dependencies: S9-003
- Scope:
  - Define soak cadence, duration bands, and run ownership.
  - Define SLO regression cadence for latency/jitter/drop-frame checks.
  - Publish runbook schedule with escalation path for regressions.

Acceptance criteria:
1. Recurring cadence and owners are documented and approved.
2. SLO regression workflow is defined with breach handling.
3. Runbook can be executed without ad-hoc assumptions.

Evidence required:
- Soak/SLO cadence schedule.
- Ownership matrix.
- Regression escalation runbook.

## TASK-ID: S9-005

- Epic: EPIC-10
- Objective: Encode Atari ST milestone acceptance criteria as periodic release-gate checklist and report template.
- Dependencies: S9-002, S9-004
- Scope:
  - Convert implementation-plan section-11 criteria into executable release-gate checklist items.
  - Define periodic gate report template and required evidence references.
  - Define gate decision outcomes (`pass`, `pass_with_conditions`, `fail`) and follow-up obligations.

Acceptance criteria:
1. All milestone criteria are represented as deterministic checklist gates.
2. Gate report template includes decision and evidence references.
3. Checklist and template are used in one dry-run gate execution.

Evidence required:
- Release-gate checklist artifact.
- Gate report template.
- Dry-run execution record.

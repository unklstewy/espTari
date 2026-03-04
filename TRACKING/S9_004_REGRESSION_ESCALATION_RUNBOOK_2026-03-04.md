# S9-004 Regression Escalation Runbook (2026-03-04)

## Scope

- Task: `S9-004`
- Goal: provide a deterministic, executable workflow for soak/SLO regression handling with no ad-hoc assumptions.
- Inputs:
  - `TRACKING/S9_004_SOAK_SLO_CADENCE_SCHEDULE_2026-03-04.md`
  - `TRACKING/S9_004_OWNERSHIP_MATRIX_2026-03-04.md`
  - `TRACKING/S9_003_THRESHOLD_DEFINITIONS_2026-03-04.md`

## Pre-run checklist (mandatory)

1. Confirm target host reachability (`BASE_URL`) and required auth token availability.
2. Confirm scripts are executable:
   - `tools/smoke_s9_risk_validation_daily_003.sh`
   - `tools/smoke_s9_risk_validation_weekly_003.sh`
3. Create/confirm writable evidence target under `captures/`.
4. Confirm owner availability per `S9-004` ownership matrix.
5. Confirm threshold sheet has no unapproved edits since prior run.

## Standard execution paths

### Path A: Daily SLO regression

- Command:
  - `./tools/smoke_s9_risk_validation_daily_003.sh`
- Required outputs:
  - `captures/s9_risk_validation_003_daily_<run_id>.txt`
  - `captures/s9_risk_validation_bundle_003_daily_<run_id>.json`
  - `captures/s9_003_daily_<run_id>/`

### Path B: Weekly soak/regression sweep

- Command:
  - `./tools/smoke_s9_risk_validation_weekly_003.sh`
- Required outputs:
  - `captures/s9_risk_validation_003_weekly_<run_id>.txt`
  - `captures/s9_risk_validation_bundle_003_weekly_<run_id>.json`
  - `captures/s9_003_weekly_<run_id>/`

### Path C: Release-gate revalidation

- Command baseline:
  - `./tools/smoke_s9_risk_validation_weekly_003.sh`
- Additional condition:
  - Ensure ABI/save-state checks (`CHK-RISK-003-01`, `CHK-RISK-003-06`) are present as `pass` in release-candidate evidence before decision meeting.

## Breach handling workflow

1. Detect breach
   - Trigger: any bundle with `summary.overall_status = fail`, or stale-evidence violation.
2. Contain
   - Mark run status `failed` and freeze release promotion if run type is release-gate.
3. Triage
   - Identify failing `check_id`, threshold reference, and first failing artifact.
4. Assign
   - Open remediation action owner: ENG lead (primary) with QA/Ops support.
5. Rerun
   - Execute same cadence script after fix; keep both failed and rerun artifacts.
6. Decide
   - `pass`: clear incident and resume cadence/release flow.
   - `fail`: escalate to product owner and maintain hold.

## Escalation levels

| Level | Trigger | Required response | Max response time |
|---|---|---|---|
| E1 | Single daily or weekly fail | Triage + remediation owner assignment + rerun scheduling | 2 hours |
| E2 | Two consecutive fails on same check | Incident call with ENG+QA+OPS; mitigation plan documented | Same business day |
| E3 | Release-gate fail or unresolved E2 | Release hold enforced; PO decision required for unblock | Immediate hold |

## Evidence and record updates

After each run (pass or fail), update:

1. Matrix execution pointer file:
   - `TRACKING/evidence/s9_risk_validation_matrix_003.json`
2. Threshold execution records:
   - `TRACKING/S9_003_THRESHOLD_DEFINITIONS_2026-03-04.md`
3. If cadence/ownership changed:
   - `TRACKING/S9_004_SOAK_SLO_CADENCE_SCHEDULE_2026-03-04.md`
   - `TRACKING/S9_004_OWNERSHIP_MATRIX_2026-03-04.md`

## Completion criteria for S9-004 acceptance

- Cadence schedule published and referenced in operating flow.
- Ownership matrix published with explicit RACI and backup rules.
- This runbook executed at least once in a documented daily or weekly cycle without procedural ambiguity.

## Execution evidence snapshot

- Executed path: `Path B: Weekly soak/regression sweep`
- Run ID: `20260304_113935`
- Outcome: `pass`
- Bundle: `captures/s9_risk_validation_bundle_003_weekly_20260304_113935.json`
- Log: `captures/s9_risk_validation_003_weekly_20260304_113935.txt`
- Artifacts: `captures/s9_003_weekly_20260304_113935/`

- Executed path: `Path A: Daily SLO regression`
- Run ID: `20260304_114040`
- Outcome: `pass`
- Bundle: `captures/s9_risk_validation_bundle_003_daily_20260304_114040.json`
- Log: `captures/s9_risk_validation_003_daily_20260304_114040.txt`
- Artifacts: `captures/s9_003_daily_20260304_114040/`

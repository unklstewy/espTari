# S9-004 Soak/SLO Cadence Schedule (2026-03-04)

## Scope

- Task: `S9-004`
- Objective: establish recurring soak and SLO regression cadence with fixed duration bands and deterministic execution windows.
- Dependency baseline: `S9-003` checks, thresholds, and evidence bundle format.
- References:
  - `TRACKING/evidence/s9_risk_validation_matrix_003.json`
  - `TRACKING/S9_003_THRESHOLD_DEFINITIONS_2026-03-04.md`
  - `tools/smoke_s9_risk_validation_daily_003.sh`
  - `tools/smoke_s9_risk_validation_weekly_003.sh`

## Duration bands

| Band ID | Name | Target duration | Primary purpose |
|---|---|---|---|
| BAND-01 | Daily SLO probe | 5-15 minutes | Fast detection of latency/dead-link regressions (`CHK-RISK-003-03`, `CHK-RISK-003-04`) |
| BAND-02 | Weekly hardening sweep | 30-60 minutes | Run higher-cost risk checks and trend validation (`CHK-RISK-003-02`, `CHK-RISK-003-05`, `CHK-RISK-003-07`) |
| BAND-03 | Release gate revalidation | 60-120 minutes | Block release on ABI/save-state risk regressions (`CHK-RISK-003-01`, `CHK-RISK-003-06`) |

## Recurring cadence schedule

| Schedule ID | Run type | Frequency | Window (local) | Duration band | Required checks | Evidence output |
|---|---|---|---|---|---|---|
| SCHED-004-DAILY | Daily SLO regression | Mon-Sun | 09:00-10:00 | BAND-01 | CHK-RISK-003-03, CHK-RISK-003-04 | `captures/s9_risk_validation_bundle_003_daily_<run_id>.json` |
| SCHED-004-WEEKLY | Weekly soak/regression | Wednesday | 10:00-12:00 | BAND-02 | CHK-RISK-003-02, CHK-RISK-003-05, CHK-RISK-003-07 (+ schema complete not-run entries) | `captures/s9_risk_validation_bundle_003_weekly_<run_id>.json` |
| SCHED-004-RELEASE | Pre-release gate | Per release candidate | Before release decision | BAND-03 | CHK-RISK-003-01, CHK-RISK-003-06 (+ weekly set if stale > 7 days) | Release-tagged S9-003 bundle in `captures/` |

## Freshness and staleness policy

- Daily evidence is stale if last successful run is older than 24 hours.
- Weekly evidence is stale if last successful run is older than 7 days.
- Release evidence is stale if ABI/save-state checks are older than the current release candidate baseline.
- Any stale status forces an immediate rerun before acceptance or release signoff.

## Execution contract

- Every run must emit:
  - structured bundle JSON (schema-compliant)
  - textual run log
  - per-check artifact directory
- Every run must set deterministic summary fields: `overall_status`, `pass_count`, `fail_count`, `not_run_count`.
- Threshold source of truth remains: `TRACKING/S9_003_THRESHOLD_DEFINITIONS_2026-03-04.md`.

## Approval checkpoints

- Engineering lead confirms schedule feasibility and script readiness.
- QA lead confirms monitorability and evidence retention.
- Product owner confirms cadence sufficiency for release decisions.
- Approval record target: add S9-004 acceptance row in `TRACKING/ACCEPTANCE_LOG.md` after first full weekly+daily cycle under this schedule.

## Operational execution record

- Cycle type: `weekly` (`SCHED-004-WEEKLY`)
- Run ID: `20260304_113935`
- Execution date: `2026-03-04`
- Result: `pass`
- Evidence bundle: `captures/s9_risk_validation_bundle_003_weekly_20260304_113935.json`
- Evidence log: `captures/s9_risk_validation_003_weekly_20260304_113935.txt`
- Artifact directory: `captures/s9_003_weekly_20260304_113935/`
- Summary snapshot: `pass_count=4`, `fail_count=0`, `not_run_count=3`

- Cycle type: `daily` (`SCHED-004-DAILY`)
- Run ID: `20260304_114040`
- Execution date: `2026-03-04`
- Result: `pass`
- Evidence bundle: `captures/s9_risk_validation_bundle_003_daily_20260304_114040.json`
- Evidence log: `captures/s9_risk_validation_003_daily_20260304_114040.txt`
- Artifact directory: `captures/s9_003_daily_20260304_114040/`
- Summary snapshot: `pass_count=2`, `fail_count=0`, `not_run_count=5`

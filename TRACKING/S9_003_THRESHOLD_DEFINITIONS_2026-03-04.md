# S9-003 Threshold Definitions (2026-03-04)

## Scope

- Task: `S9-003`
- Purpose: define deterministic pass/fail thresholds and failure signals for recurring architecture-risk checks.
- Matrix reference: `TRACKING/evidence/s9_risk_validation_matrix_003.json`

## Threshold table

| Threshold ID | Check ID | Metric / signal | Pass condition | Fail signal |
|---|---|---|---|---|
| THR-RISK-003-01 | CHK-RISK-003-01 | ABI guard-code determinism | Expected negative cases return canonical guard codes; no unexpected ABI regressions in positive set | Any missing/changed guard code, or any positive-case ABI validation regression |
| THR-RISK-003-02 | CHK-RISK-003-02 | Stream envelope overhead | Weekly gate latency sample <= 130 ms for mandatory stream paths; payload size delta <= 15% vs baseline | Latency or size delta exceeds threshold in any mandatory stream path |
| THR-RISK-003-03 | CHK-RISK-003-03 | SD I/O endpoint latency | P95 <= 120 ms and P99 <= 250 ms on monitored file/catalog operations | Two consecutive runs exceed P95 or any run exceeds P99 |
| THR-RISK-003-04 | CHK-RISK-003-04 | Dead-link policy behavior | Deterministic `dead -> retry -> online/dead` transitions and counter monotonicity | Non-deterministic state/counter behavior or missing expected guard code |
| THR-RISK-003-05 | CHK-RISK-003-05 | Trace/backpressure pressure | `dropped_events_since_last` remains 0 in nominal soak windows; sustained throttle window <= 3 consecutive samples | Any sustained drop/throttle condition beyond threshold window |
| THR-RISK-003-06 | CHK-RISK-003-06 | Save-state compatibility drift | Supported profiles restore/validate pass; incompatible payloads reject with canonical compatibility codes | Supported profile restore/validate failure or non-canonical incompatibility error mapping |
| THR-RISK-003-07 | CHK-RISK-003-07 | Debug-mode perturbation | Deterministic clock mode and step behavior; guard IDs/check IDs stable for negative cases | Non-deterministic counters/order, or guard/check ID drift |

## Cadence policy

- `daily`: CHK-RISK-003-03, CHK-RISK-003-04
- `weekly`: CHK-RISK-003-02, CHK-RISK-003-05, CHK-RISK-003-07
- `per-release + weekly`: CHK-RISK-003-01, CHK-RISK-003-06

## Escalation policy

- Single fail on `per-release` checks blocks release gate until rerun passes.
- Two consecutive fails on `daily`/`weekly` checks trigger incident triage and owner assignment.
- Any fail must emit an evidence bundle with `summary.overall_status = "fail"` and explicit failed `check_id` entries.

## Runner usage (weekly)

- Script: `tools/smoke_s9_risk_validation_weekly_003.sh`
- Command: `./tools/smoke_s9_risk_validation_weekly_003.sh`
- Optional target override: `BASE_URL=http://esptari.local ./tools/smoke_s9_risk_validation_weekly_003.sh`
- Outputs:
	- Run log: `captures/s9_risk_validation_003_weekly_<run_id>.txt`
	- Evidence bundle: `captures/s9_risk_validation_bundle_003_weekly_<run_id>.json`
	- Check artifacts directory: `captures/s9_003_weekly_<run_id>/`
- Bundle contract: `TRACKING/S9_003_EVIDENCE_BUNDLE_SCHEMA_2026-03-04.json`

## Runner usage (daily)

- Script: `tools/smoke_s9_risk_validation_daily_003.sh`
- Command: `./tools/smoke_s9_risk_validation_daily_003.sh`
- Optional target override: `BASE_URL=http://esptari.local ./tools/smoke_s9_risk_validation_daily_003.sh`
- Outputs:
	- Run log: `captures/s9_risk_validation_003_daily_<run_id>.txt`
	- Evidence bundle: `captures/s9_risk_validation_bundle_003_daily_<run_id>.json`
	- Check artifacts directory: `captures/s9_003_daily_<run_id>/`
- Bundle contract: `TRACKING/S9_003_EVIDENCE_BUNDLE_SCHEMA_2026-03-04.json`

## First weekly execution record

- Run ID: `20260304_113256`
- Execution date: `2026-03-04`
- Overall result: `pass`
- Evidence bundle: `captures/s9_risk_validation_bundle_003_weekly_20260304_113256.json`
- Evidence log: `captures/s9_risk_validation_003_weekly_20260304_113256.txt`
- Artifact directory: `captures/s9_003_weekly_20260304_113256/`
- Summary snapshot: `pass_count=4`, `fail_count=0`, `not_run_count=3`

## First daily execution record

- Run ID: `20260304_113508`
- Execution date: `2026-03-04`
- Overall result: `pass`
- Evidence bundle: `captures/s9_risk_validation_bundle_003_daily_20260304_113508.json`
- Evidence log: `captures/s9_risk_validation_003_daily_20260304_113508.txt`
- Artifact directory: `captures/s9_003_daily_20260304_113508/`
- Summary snapshot: `pass_count=2`, `fail_count=0`, `not_run_count=5`

## Weekly execution record (S9-004 operational cycle)

- Run ID: `20260304_113935`
- Execution date: `2026-03-04`
- Overall result: `pass`
- Evidence bundle: `captures/s9_risk_validation_bundle_003_weekly_20260304_113935.json`
- Evidence log: `captures/s9_risk_validation_003_weekly_20260304_113935.txt`
- Artifact directory: `captures/s9_003_weekly_20260304_113935/`
- Summary snapshot: `pass_count=4`, `fail_count=0`, `not_run_count=3`

## Daily execution record (S9-004 operational cycle)

- Run ID: `20260304_114040`
- Execution date: `2026-03-04`
- Overall result: `pass`
- Evidence bundle: `captures/s9_risk_validation_bundle_003_daily_20260304_114040.json`
- Evidence log: `captures/s9_risk_validation_003_daily_20260304_114040.txt`
- Artifact directory: `captures/s9_003_daily_20260304_114040/`
- Summary snapshot: `pass_count=2`, `fail_count=0`, `not_run_count=5`

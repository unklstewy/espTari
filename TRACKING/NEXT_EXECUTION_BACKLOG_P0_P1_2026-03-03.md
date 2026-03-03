# Next Execution Backlog (P0/P1)

Date: 2026-03-03  
Scope: Immediate execution backlog derived from accepted evidence, implementation state, and tracking-state gaps.

## Prioritization logic

1. Close governance/tracking drift that blocks trustworthy decisions.
2. Re-open runtime unlock flow now that Section 6 is closed.
3. Execute runtime conformance/evidence wave for sections 7/8/11 readiness.

---

## P0 backlog (execute first)

### P0-1 Tracking reconciliation (accepted work to Done)

- Card moves:
  - `PRQ-001`: In Review -> Done
  - `PRQ-002`: In Review -> Done
  - `PRQ-003`: In Review -> Done
  - `PRQ-004`: Acceptance -> Done
- Update files:
  - `TRACKING/KANBAN_BOARD.md`
  - `TRACKING/BACKLOG.md`
  - `TRACKING/S5_RUNTIME_UNLOCK_EXECUTION_PLAN.md`
- Evidence files to generate:
  - `TRACKING/PRQ_WORKING/PRQ_STATUS_RECONCILIATION_2026-03-03.md`
  - `captures/prq_reconciliation_diff_20260303.txt`

### P0-2 CRT unlock decision refresh (post-Section-6 closure)

- Card moves:
  - `CRT-001`: In Progress -> In Review
  - `CRT-002`: In Progress -> In Review
  - `CRT-003`: In Progress -> In Review
  - `CRT-004`: In Progress -> In Review
  - `CRT-005`: In Progress -> Acceptance
- Update files:
  - `TRACKING/CRT_HANDOFF_S5_SUMMARY.md`
  - `TRACKING/ACCEPTANCE_LOG.md`
  - `TRACKING/TASK_CARDS_CRT_001_CRT_005.md`
- Evidence files to generate:
  - `TRACKING/CRT_READINESS/CRT_UNLOCK_REVIEW_DELTA_2026-03-03.md`
  - `TRACKING/CRT_READINESS/CRT_DECISION_PACKET_2026-03-03.md`

### P0-3 Runtime conformance harness activation (Section 11 closure path)

- Card moves:
  - `T-092`: Backlog -> In Progress
  - `T-093`: Backlog -> In Progress
  - `T-094`: Backlog -> Ready
  - `T-095`: Backlog -> Ready
- Target outputs:
  - Runtime harness scaffold callable from firmware/runtime test path
  - Evidence collection + packaging pipeline
- Evidence files to generate:
  - `TRACKING/CONFORMANCE_RUNTIME/CONF_HARNESS_EXEC_PLAN_2026-03-03.md`
  - `TRACKING/CONFORMANCE_RUNTIME/CONF_ARTIFACT_SCHEMA_2026-03-03.md`
  - `captures/conf_harness_smoke_20260303.txt`

### P0-4 Section 11 acceptance criteria execution matrix

- Card moves:
  - `T-110`: Backlog -> Ready
  - `T-111`: Backlog -> Ready
- Target outputs:
  - Executable criteria matrix for milestones 1..14 in Section 11
  - Pass/fail evidence mapping by criterion
- Evidence files to generate:
  - `TRACKING/CONFORMANCE_RUNTIME/SECTION11_ACCEPTANCE_MATRIX_2026-03-03.md`
  - `TRACKING/CONFORMANCE_RUNTIME/SECTION11_EVIDENCE_INDEX_2026-03-03.md`

---

## P1 backlog (start after P0-1 and P0-2 complete)

### P1-1 Section 7 observability closure (operational, not contract-only)

- Card moves:
  - `T-090`: Backlog -> In Progress
  - `T-091`: Backlog -> In Progress
  - `T-116`: Backlog -> Ready
  - `T-117`: Backlog -> Ready
- Target outputs:
  - Verified backpressure counters/telemetry behavior under load
  - SLO collector + threshold alarm validation evidence
- Evidence files to generate:
  - `captures/observability_backpressure_run_20260303.txt`
  - `captures/observability_slo_alarm_run_20260303.txt`
  - `TRACKING/CONFORMANCE_RUNTIME/OBSERVABILITY_VALIDATION_REPORT_2026-03-03.md`

### P1-2 Section 8 runtime closure (scheduler/profile implementation path)

- Card moves:
  - `T-074`: Backlog -> In Progress
  - `T-075`: Backlog -> In Progress
  - `T-076`: Backlog -> Ready
  - `T-077`: Backlog -> Ready
- Target outputs:
  - Profile manifest parser + wiring fail-fast checks runtime-backed
  - Deterministic scheduler/timestamp evidence in execution traces
- Evidence files to generate:
  - `captures/st_profile_manifest_validation_20260303.txt`
  - `captures/scheduler_determinism_trace_20260303.txt`
  - `TRACKING/CONFORMANCE_RUNTIME/SCHEDULER_PROFILE_RUNTIME_REPORT_2026-03-03.md`

### P1-3 Section 4/5 hardening completion pass

- Card moves:
  - `T-060`: Backlog -> Ready
  - `T-061`: Backlog -> Ready
  - `T-062`: Backlog -> In Progress
  - `T-063`: Backlog -> In Progress
  - `T-064`: Backlog -> Ready
  - `T-065`: Backlog -> Ready
- Target outputs:
  - Staged download integrity + dead-link retry policy validation
  - Scheduler-driven catalog-sync reliability evidence
- Evidence files to generate:
  - `captures/catalog_sync_reliability_20260303.txt`
  - `captures/dead_link_probe_retry_20260303.txt`
  - `TRACKING/CONFORMANCE_RUNTIME/CATALOG_SYNC_RUNTIME_REPORT_2026-03-03.md`

### P1-4 Section 9 security/integrity operational closure

- Card moves:
  - `T-041`: Backlog -> Ready
  - `T-043`: Backlog -> Ready
  - `T-044`: Backlog -> Ready
- Target outputs:
  - Path-allowlist and traversal rejection verification
  - Upload limits/timeouts and audit-log completeness evidence
- Evidence files to generate:
  - `captures/security_path_allowlist_20260303.txt`
  - `captures/security_upload_limits_20260303.txt`
  - `TRACKING/CONFORMANCE_RUNTIME/SECURITY_INTEGRITY_REPORT_2026-03-03.md`

---

## Execution order (strict)

1. P0-1
2. P0-2
3. P0-3
4. P0-4
5. P1-1
6. P1-2
7. P1-3
8. P1-4

## Completion gate for this backlog

This backlog is considered complete when:

- all listed card moves are reflected in `TRACKING/KANBAN_BOARD.md` and `TRACKING/BACKLOG.md`,
- all listed evidence files exist and are linked in `TRACKING/ACCEPTANCE_LOG.md`,
- Section 11 acceptance matrix has pass/fail status and evidence links for criteria 1..14.

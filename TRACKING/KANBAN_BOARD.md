# Kanban Board

This board is the operational view for daily execution.

## WIP Limits

- In Progress: 6
- In Review: 3
- Acceptance: 2

## Board

### Backlog

- Umbrella trackers: T-001 through T-038
- Decomposed child backlog: (empty for T-054 through T-121)
- Save-state decomposition backlog: T-039 through T-044
- CRT readiness wave backlog: (empty)
- S5 unlock prerequisites backlog: (empty)
- S5 runtime unlock chain backlog: (empty; S5-001 through S5-008 complete)
- S6 phase-4 hardening backlog: (empty; decomposed to pullable S6-001 through S6-008)
- S9 ARC residual backlog: (empty; S9-001 through S9-005 are pullable)
- S10 integration-readiness backlog: S10-001 through S10-005 (soft/non-gating, pre-emulated-hardware)
- S11 pre-EBIN enablement backlog: S11-001 through S11-008 (hard-validation bridge)

### Ready

- T-045, T-046, T-047 (decomposition of T-001)
- T-048, T-049, T-050 (decomposition of T-003)
- T-051, T-052, T-053 (decomposition of T-009)
- T-094, T-095 (conformance checklist runner + review pack)
- T-110, T-111 (section-11 acceptance suite/reporting path)
- T-116, T-117 (SLO collectors + threshold/alarm exposure)
- (no remaining T-054 through T-121 items)
- (no remaining S7 phase-5 tasks; S7-001 through S7-008 complete)

### In Progress

- T-092 (conformance harness scaffold + manifest loader)
- T-093 (evidence artifact collection + report packaging flow)
- S11-001 (register + bus/memory runtime hard-validation path activation)
- S11-002 (SD-only hard-validation enforcement path)
- S11-005 (current-cycle reconfirmation fixture execution for 07/09/10/12)
- S11-006 (sustained SLO threshold hard-validation run)

### In Review

- CRT-001 (lifecycle transition/guard readiness pack)
- CRT-002 (input mapping CRUD/apply readiness pack)
- CRT-003 (save/restore compatibility readiness pack)
- CRT-004 (observability stream/telemetry readiness pack)

### Acceptance

- CRT-005 (CRT handoff pack for runtime phase gate decision refresh)

### Done

- T-054 through T-121
- T-090 (stream backpressure counters + watermark metrics)
- T-091 (backpressure telemetry/event exposure)
- PRQ-001 (core runtime code-path closure)
- PRQ-002 (deterministic fixture/scenario package)
- PRQ-003 (deployment workflow documentation)
- PRQ-004 (unlock review packet)
- S5-001 through S5-008 (runtime unlock chain)
- S6-001 (phase-4 harness baseline + deterministic fault-injection matrix)
- S6-002 (stress/fault recovery matrix + control-plane survivability validation)
- S6-003 (long-run stability soak suite + deterministic threshold validation)
- S6-004 (catalog probe/dead/retry reliability validation against API 7.10)
- S6-005 (scheduler CRUD/recovery report and deterministic due-order validation)
- S6-006 (SLO latency/jitter/dropped-frame threshold and alarm semantic validation)
- S6-007 (debug clock mode/single-step conformance and diagnostic payload validation)
- S6-008 (Sprint 06 hardening packet, traceability matrix, and decision handoff assembly)
- S7-001 (multi-machine profile baseline harness and deterministic profile contract matrix)
- S7-002 (Mega ST bootstrap/lifecycle parity and deterministic guard validation)
- S7-003 (Mega ST media/catalog/session run-path parity and blocker validation)
- S7-004 (STe extension control payload/guard validation and backward-envelope compatibility)
- S7-005 (Mega STe extension compatibility delta validation and fallback guard verification)
- S7-006 (cross-profile compatibility and ABI/regression guard suite validation)
- S7-007 (profile-switch isolation and fallback semantics validation)
- S7-008 (Sprint 07 packet, traceability matrix, and PO decision handoff assembly)
- S8-001 (active profile manifest/wiring enablement)
- S8-002 (active Mega ST lifecycle/API parity validation)
- S8-003 (active STe extension control validation)
- S8-004 (active Mega STe extension delta validation)
- S8-005 (active cross-profile media/catalog/session parity validation)
- S8-006 (active pairwise ABI/regression guard suite validation)
- S8-007 (active profile-switch isolation/fallback validation)
- S8-008 (Sprint 08 packet, traceability matrix, and PO decision handoff assembly)
- S9-001 (API contract delta changelog discipline + release-note workflow)
- S9-002 (security/integrity closure audit: auth/path/upload/EBIN)
- S9-003 (risk-to-check operationalization: recurring validation + evidence schema)
- S9-004 (recurring soak/SLO cadence, ownership matrix, and escalation runbook)
- S9-005 (Section-11 release-gate checklist, report template, and dry-run execution)
- S10-001 (Section-11 integration-readiness matrix with soft status model)
- S10-002 (placeholder fixture/harness scaffold + smoke readiness run)
- S10-003 (observational SLO baseline template + sample report)
- S10-004 (integration-readiness confidence report)
- S10-005 (review decision + S11 hard-validation transition plan)
- S11-003 (EBIN ABI/manifest contract freeze + validator outcomes)
- S11-004 (S9-002 security regression sweep refresh)
- S11-007 (Section-11 hard-validation gate report)
- S11-008 (pre-EBIN go/no-go decision packet)

Board hygiene note:
- Legacy T-/CRT lane labels above are retained for historical traceability; execution priority follows active sprint queue.

Sprint focus note:
- Sprint 08 (`S8-001` through `S8-008`) is complete; Sprint 09 ARC residual tranche (`S9-001` through `S9-005`) is complete and all tasks are done/accepted.
- Sprint 10 soft integration-readiness tranche (`S10-001` through `S10-005`) is executed with non-gating outputs and explicit S11 hard-validation transition prerequisites.
- Sprint 11 pre-EBIN hard-validation bridge is active: gate/decision artifacts are produced (`fail` / `no_go`) with remaining runtime-path and sustained-validation tasks in progress.

### Blocked

- Runtime/API validation is conditionally unlocked and must follow `unlock_with_conditions` controls documented in CRT/PRQ artifacts.

## Daily standup fields

- What moved to Done since last check?
- What is blocked and why?
- Any WIP limit violations?
- Is sprint commitment still realistic?
- Is Acceptance SLA (<=12 working hours) being met?

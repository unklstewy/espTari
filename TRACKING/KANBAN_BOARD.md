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

### Ready

- T-045, T-046, T-047 (decomposition of T-001)
- T-048, T-049, T-050 (decomposition of T-003)
- T-051, T-052, T-053 (decomposition of T-009)
- T-094, T-095 (conformance checklist runner + review pack)
- (no remaining T-054 through T-121 items)

### In Progress

- T-092 (conformance harness scaffold + manifest loader)
- T-093 (evidence artifact collection + report packaging flow)

### In Review

- CRT-001 (lifecycle transition/guard readiness pack)
- CRT-002 (input mapping CRUD/apply readiness pack)
- CRT-003 (save/restore compatibility readiness pack)
- CRT-004 (observability stream/telemetry readiness pack)

### Acceptance

- CRT-005 (CRT handoff pack for runtime phase gate decision refresh)

### Done

- T-054 through T-121
- PRQ-001 (core runtime code-path closure)
- PRQ-002 (deterministic fixture/scenario package)
- PRQ-003 (deployment workflow documentation)
- PRQ-004 (unlock review packet)

### Blocked

- Runtime/API validation is conditionally unlocked and must follow `unlock_with_conditions` controls documented in CRT/PRQ artifacts.

## Daily standup fields

- What moved to Done since last check?
- What is blocked and why?
- Any WIP limit violations?
- Is sprint commitment still realistic?
- Is Acceptance SLA (<=12 working hours) being met?

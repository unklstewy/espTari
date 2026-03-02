# espTari v2 Delivery Tracking (Blended Kanban + Sprint)

This folder is the execution system for an Agentic AI delivery team.

## Product roles

- Product Owner (You): sets priorities and accepts/rejects completed work.
- Acceptance Master (You): final acceptance gate for each task, story, and sprint outcome.
- Agentic Delivery Team: plans, implements, validates, and documents outcomes.

## How this system works

- Kanban controls day-to-day flow and WIP.
- Micro-sprint windows provide timeboxed integration milestones.
- Epics define outcomes.
- Tasks define executable units with acceptance criteria.

## AI Throughput Profile

- Plan for agent execution speed, not human typing speed.
- Keep task slices small (target <= 1 day each).
- Use parallel pull within dependency-safe boundaries.
- Keep Acceptance queue short to avoid Product Owner bottlenecks.

## Navigation

1. [Working Agreement](WORKING_AGREEMENT.md)
2. [Epics](EPICS.md)
3. [Backlog and Task Index](BACKLOG.md)
4. [Architecture 90 Percent Closure Checklist](ARCHITECTURE_90_CLOSURE_CHECKLIST.md)
5. [MVP Definition](MVP_DEFINITION.md)
6. [Kanban Board](KANBAN_BOARD.md)
7. [Sprint 0 - Foundations](SPRINT_00_FOUNDATIONS.md)
8. [Sprint 1 - Control/API Vertical Slice](SPRINT_01_VERTICAL_SLICE.md)
9. [Sprint 2 - Atari ST Core Runtime](SPRINT_02_ST_CORE_RUNTIME.md)
10. [Sprint 3 - Input/Catalog/Sync](SPRINT_03_INPUT_CATALOG_SYNC.md)
11. [Sprint 4 - Observability + Conformance](SPRINT_04_OBSERVABILITY_CONFORMANCE.md)
12. [Acceptance Log](ACCEPTANCE_LOG.md)
13. [Agent Runbook](AGENT_RUNBOOK.md)
14. [Release Notes](RELEASE_NOTES.md)
15. [Contract-to-Runtime Verification Tasks](CONTRACT_TO_RUNTIME_VERIFICATION_TASKS.md)
16. [CRT Task Cards (CRT-001 through CRT-005)](TASK_CARDS_CRT_001_CRT_005.md)
17. [S5 Runtime Unlock Execution Plan](S5_RUNTIME_UNLOCK_EXECUTION_PLAN.md)
18. [S5 Unlock Prerequisite Task Cards (PRQ-001 through PRQ-004)](TASK_CARDS_S5_UNLOCK_PREREQS.md)

## Cadence

- Sprint length: 3 working days (micro-sprints).
- Twice daily: Kanban pull + blocker review.
- End of each micro-sprint: demo + acceptance review + backlog reprioritization.

## Definition of success

- Every completed task has measurable acceptance evidence.
- No task moves to Done without Product Owner acceptance.
- Cycle-time and blocked-time trends improve sprint over sprint.

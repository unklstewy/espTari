# Agent Runbook (Execution Procedure)

This runbook defines how an Agentic AI should execute work in this project.

## 1) Start-of-day procedure

1. Open [KANBAN_BOARD.md](KANBAN_BOARD.md)
2. Confirm WIP limits
3. Pull highest-priority Ready task with satisfied dependencies
4. Move task to In Progress

Mid-day procedure:

1. Re-evaluate blockers and Acceptance queue
2. Push completed work to In Review/Acceptance rapidly (no batching)

## 2) Execution cycle per task

1. Read task card in [BACKLOG.md](BACKLOG.md)
2. Re-state acceptance criteria in working notes
3. Implement smallest valid increment
4. Validate with objective evidence
5. Update docs/contracts if changed
6. Move to In Review with evidence summary

## 3) Review handoff format

For every task entering In Review:

- Task ID
- What changed
- Validation evidence
- Risks/known gaps
- Acceptance request

## 4) Acceptance handoff format

For every task entering Acceptance:

- Task ID
- Acceptance criteria checklist with pass/fail
- Links to evidence
- Recommendation: Accept or Rework

## 5) Blocker handling

If blocked:

1. Move task to Blocked
2. Record blocker cause and dependency
3. Propose 2 unblock paths
4. Request Product Owner decision

## 6) Quality gates

Before Done:

- Criteria complete
- Evidence complete
- Docs updated
- Acceptance decision logged in [ACCEPTANCE_LOG.md](ACCEPTANCE_LOG.md)

## 7) Sprint close checklist

- All accepted tasks moved to Done
- Rejected tasks returned to Backlog with corrective notes
- Sprint review summary prepared
- Next sprint Ready queue prioritized by Product Owner

Micro-sprint note:

- Sprint close occurs every 3 working days.
- Keep each micro-sprint commitment small and evidence-rich.

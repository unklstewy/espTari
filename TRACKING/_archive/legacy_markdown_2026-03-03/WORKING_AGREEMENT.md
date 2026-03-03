# Working Agreement (Kanban + Sprint Hybrid)

## 1) Flow policy

Kanban columns:

1. Backlog
2. Ready
3. In Progress
4. In Review
5. Acceptance
6. Done

WIP limits:

- In Progress: max 5 tasks
- In Review: max 3 tasks
- Acceptance: max 2 tasks (hard cap to protect PO review bandwidth)

Pull policy:

- Pull highest-priority Ready task that is unblocked and fits WIP limits.
- Never start new work when blocked work can be unblocked.

## 2) Sprint policy

- Sprint length: 3 working days.
- Sprint planning: commit dependency-safe slices only (target <= 1 day per task).
- Mid-sprint: scope changes allowed only by Product Owner.
- Sprint review: done work must include evidence and acceptance decision.

Acceptance SLA:

- Acceptance queue should be reviewed within 12 working hours.
- If Acceptance queue is full, no new task can move to In Review.

## 3) Task policy

Each task must contain:

- Objective
- Scope
- Dependencies
- Acceptance criteria
- Evidence required
- Done checklist

Task slicing rule:

- Any task estimated > 1 day must be split before entering Ready.
- L-size tasks are placeholders only and are not pullable.

## 4) Acceptance policy

Task can enter Acceptance only when:

- Implementation complete
- Related docs updated
- Validation evidence attached
- Risk notes captured

Task can enter Done only when Acceptance Master marks Accepted.

## 5) Definition of Done (DoD)

- Meets explicit acceptance criteria
- No unresolved blockers in scope
- Relevant docs and contracts updated
- Evidence logged in Acceptance Log

## 6) Blocker escalation

If blocked > 4 hours:

- Mark blocked reason
- Propose two unblock options
- Escalate to Product Owner for decision

## 7) Prioritization model

Default priority order:

1. System safety/correctness blockers
2. Contract/API stabilization
3. Vertical slice completion
4. Capability expansion
5. Optimization/hardening

## 8) Parallelization policy

- Agents may work in parallel on tasks without direct dependency overlap.
- Avoid parallel changes to the same contract section unless one task is designated owner.
- Prefer parallel lanes: `API Contracts`, `Catalog/Sync`, `Input`, `Runtime`, `Validation`.

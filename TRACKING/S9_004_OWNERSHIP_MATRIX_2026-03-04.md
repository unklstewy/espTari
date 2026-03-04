# S9-004 Ownership Matrix (2026-03-04)

## Scope

- Task: `S9-004`
- Purpose: define accountable owners, backups, and escalation responders for recurring soak/SLO regression operations.
- Applies to runs scheduled in: `TRACKING/S9_004_SOAK_SLO_CADENCE_SCHEDULE_2026-03-04.md`.

## RACI legend

- `A` = Accountable (final signoff)
- `R` = Responsible (executes work)
- `C` = Consulted (review/inputs)
- `I` = Informed (status visibility)

## Role registry

| Role ID | Team function | Primary responsibility |
|---|---|---|
| ROLE-ENG-LEAD | Engineering lead | Technical accountability for run success and remediation implementation |
| ROLE-QA-LEAD | QA lead | Validation quality, evidence completeness, and reproducibility |
| ROLE-OPS-ONCALL | Ops/on-call owner | Alert handling, incident coordination, and rerun orchestration |
| ROLE-REL-MGR | Release manager | Release gate synchronization and decision flow control |
| ROLE-PO | Product owner | Acceptance decision and business-priority escalation |

## Ownership matrix

| Workflow item | ENG lead | QA lead | OPS on-call | Release mgr | PO |
|---|---|---|---|---|---|
| Daily SLO run execution (`SCHED-004-DAILY`) | C | A/R | R | I | I |
| Weekly soak/regression run execution (`SCHED-004-WEEKLY`) | A/R | R | C | I | I |
| Release-gate revalidation (`SCHED-004-RELEASE`) | R | R | C | A/R | I |
| Evidence bundle integrity/schema compliance | A | R | C | I | I |
| Threshold breach triage and incident declaration | A/R | C | R | I | I |
| Release hold / unblock decision preparation | C | C | I | A/R | R |
| Final acceptance update (`ACCEPTANCE_LOG`) | C | C | I | R | A |

## Backup and handoff rules

- Every `A` role must designate a same-day backup before the run window opens.
- If the primary `R` is unavailable at run start, backup assumes execution without rescheduling.
- Handoff note must include:
  - active run ID
  - current pass/fail state
  - blocker summary
  - next required action and due time

## SLA expectations

- Daily run failures acknowledged within 30 minutes during local business hours.
- Weekly run failures acknowledged within 2 hours.
- Release-gate failures acknowledged immediately and release hold is enforced until explicit unblock decision.

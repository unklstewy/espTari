# Remaining Sprint/Work Todo Decomposition (DB + ARC) — 2026-03-04

## Source of truth

Per operator direction, `TRACKING/tracking.db` is authoritative for task state.

Architecture cross-check sources:
- `docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md` (sections 10, 11, 12, 14)
- `docs/EMU_ENGINE_V2_API_SPEC.md` (section 6 completion posture and normative contracts)

## DB snapshot

Query results:
- `SELECT status, COUNT(*) FROM tasks GROUP BY status ORDER BY status;`
  - `Done | 163`
  - `Ready | 5`
- `SELECT task_id, status, sprint FROM tasks WHERE status <> 'Done' ORDER BY task_id;`
  - `S9-001`, `S9-002`, `S9-003`, `S9-004`, `S9-005` (all `Ready`)

## Remaining work

There are **5 remaining DB-tracked todos** in sprint `S9`.

## Sprint closure posture (task counts in DB)

- `S1`: 24 tasks (all Done)
- `S2`: 24 tasks (all Done)
- `S3`: 34 tasks (all Done)
- `S4`: 40 tasks (all Done)
- `S5`: 17 tasks (all Done)
- `S6`: 8 tasks (all Done)
- `S7`: 8 tasks (all Done)
- `S8`: 8 tasks (all Done)
- `S9`: 5 tasks (`Ready`)

## Decomposition output (DB-tracked)

Current DB-tracked decomposition:

- Wave 1 (foundation): `S9-001`
- Wave 2 (parallel after Wave 1): `S9-002` + `S9-003`
- Wave 3 (after `S9-003`): `S9-004`
- Wave 4 (closeout): `S9-005` (depends on `S9-002` and `S9-004`)

## ARC-derived residual work mapping (now registered as S9)

The attached architecture residual work is now represented in `tracking.db` as `S9-001` through `S9-005`.

### Residual candidate set

- `S9-001` API changelog discipline
  - Maintain API-to-implementation alignment and publish contract delta changelogs on updates (implementation plan immediate action #6).
- `S9-002` Security hardening closure checks
  - Validate/verify auth mode posture, path hardening, upload constraints, EBIN integrity policy, and audit-log completeness against security model section.
- `S9-003` Risk-to-test operationalization
  - Convert risk mitigations into recurring validation checks (ABI compatibility churn, stream overhead, SD I/O latency, dead-link resilience, trace pressure, save-state drift, debug-mode perturbation).
- `S9-004` Long-run production hardening cadence
  - Schedule recurrent soak/stability and SLO-regression runs beyond one-time sprint closure evidence.
- `S9-005` Milestone acceptance re-verification gate
  - Re-run Atari ST milestone acceptance criteria set (section 11) as a periodic release gate.

## Suggested decomposition for ARC residuals

### Wave A — Governance and traceability

- `S9-001` Define release-note/changelog template and update cadence.

### Wave B — Security/integrity hardening

- `S9-002` Audit current auth/access configuration, path normalization/traversal protection, upload limits, and EBIN integrity/audit events.

### Wave C — Reliability and performance operations

- `S9-003` Define recurring risk checks and evidence format.
- `S9-004` Establish periodic soak/SLO cadence and ownership.
- `S9-005` Encode milestone acceptance checklist + periodic release-gate reporting.

## Recommended execution order

Pull sequence:
1. `S9-001`
2. `S9-002` and `S9-003` (parallel)
3. `S9-004`
4. `S9-005`

## Operational note

Backlog and kanban were updated to include this S9 tranche.

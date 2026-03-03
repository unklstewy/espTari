# Task Cards: S5 Unlock Prerequisites (PRQ-001 through PRQ-004)

This file operationalizes prerequisite closure after PO `hold` for S5 CRT runtime unlock.

Phase note:
- Pre-runtime-unlock phase only.
- Documentation/design/code-ready artifacts only.
- No runtime/API/build/flash/test execution evidence in this tranche.

## TASK-ID: PRQ-001

- Epic: EPIC-06
- Objective: Close prerequisite for core lifecycle/input/save-restore/observability runtime code paths.
- Scope:
  - Produce domain-by-domain implementation-ready closure matrix.
  - Identify remaining code-path gaps and closure actions per domain.
  - Prepare review-ready evidence index for implementation readiness.
- Out of scope:
  - Runtime endpoint execution.
  - Performance or conformance runtime claims.
- Dependencies: CRT-005
- Risks:
  - Cross-domain inconsistency can mask readiness gaps.

Acceptance criteria:
1. Lifecycle/input/save-restore/observability domains each have explicit closure status and evidence links.
2. Remaining gaps have owner, action, and closure target.
3. Artifact set is review-ready for downstream unlock packet assembly.

Evidence required:
- Domain closure matrix and gap register.
- Implementation-ready traceability map.
- Tracking evidence row updates in backlog/board/acceptance context.

Done checklist:
- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

## TASK-ID: PRQ-002

- Epic: EPIC-06
- Objective: Deliver deterministic fixture/scenario package for CRT vectors.
- Scope:
  - Define fixture catalog and scenario manifest aligned to CRT check families.
  - Define deterministic controls, assumptions, and repeatability constraints.
  - Prepare packaging/index artifacts for unlock review.
- Out of scope:
  - Runtime execution of scenarios.
  - Fixture performance benchmarking.
- Dependencies: PRQ-001
- Risks:
  - Missing deterministic controls may invalidate future runtime evidence.

Acceptance criteria:
1. Fixture/scenario package covers CRT check-family domains with explicit mapping.
2. Repeatability constraints and deterministic assumptions are documented.
3. Package is review-ready and referenced by PRQ-004 handoff inputs.

Evidence required:
- Fixture catalog and scenario manifest.
- Check-family mapping table.
- Deterministic control and repeatability notes.

Done checklist:
- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

## TASK-ID: PRQ-003

- Epic: EPIC-06
- Objective: Produce reproducible firmware/app deployment workflow documentation for unlock readiness.
- Scope:
  - Define environment prerequisites and version assumptions.
  - Define deterministic deployment workflow steps and rollback path.
  - Define pre-runtime verification checkpoints for future execution phase.
- Out of scope:
  - Running deployment workflow.
  - Runtime validation and execution artifacts.
- Dependencies: PRQ-002
- Risks:
  - Incomplete environment assumptions can block future reproducibility.

Acceptance criteria:
1. Workflow documentation includes prerequisites, ordered steps, rollback guidance, and checkpoints.
2. Assumptions/constraints are explicit and versioned.
3. Documentation is review-ready for inclusion in unlock packet.

Evidence required:
- Deployment workflow runbook (documentation-only).
- Environment prerequisites matrix.
- Rollback and checkpoint checklist.

Done checklist:
- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

## TASK-ID: PRQ-004

- Epic: EPIC-06
- Objective: Assemble unlock review packet and submit decision-ready package for PO.
- Scope:
  - Consolidate PRQ-001..PRQ-003 evidence artifacts.
  - Build explicit unlock decision template (`unlock` / `unlock_with_conditions` / `hold`).
  - Produce final prerequisite status and blocker/condition register.
- Out of scope:
  - Making the unlock decision on behalf of PO.
  - Any runtime execution evidence.
- Dependencies: PRQ-003
- Risks:
  - Missing prerequisite traceability may force additional review cycles.

Acceptance criteria:
1. Review packet contains 1:1 mapping from prerequisites to evidence artifacts.
2. Decision template is complete and ready for PO/Acceptance review.
3. Blocker/condition register includes owner and follow-up timing.

Evidence required:
- Unlock review packet index.
- Prerequisite-to-evidence mapping matrix.
- Decision template and condition register.

Done checklist:
- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

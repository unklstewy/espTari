# Task Cards: Sprint 10 Integration Readiness (S10-001 through S10-005)

This file defines a soft, non-gating integration-readiness sprint for the pre-hardware-emulation phase.

Phase intent:
- Validate test/evidence plumbing before full emulated-hardware engine implementation exists.
- Capture observable baseline behavior without enforcing premature product-quality gates.
- Preserve deterministic workflows while allowing statuses that reflect partial readiness.

Readiness status model (for S10 artifacts):
- `baseline_observed` = check/harness path executes and emits expected evidence structure.
- `needs_data` = path exists but lacks sufficient runtime data for quality claims.
- `blocked_by_missing_engine` = cannot be meaningfully executed until missing engine/hardware code lands.

## TASK-ID: S10-001

- Epic: EPIC-11
- Objective: Establish Section-11 integration-readiness matrix using soft readiness statuses.
- Dependencies: S9-005
- Scope:
  - Mirror Section-11 criteria into a readiness matrix with the S10 status model.
  - Define evidence slots for each criterion without forcing pass/fail closure.
  - Publish update rules for progressing `blocked_by_missing_engine` to executable states.

Acceptance criteria:
1. All Section-11 criteria are represented with one readiness status.
2. Each criterion row has at least one evidence slot path.
3. Matrix update rules are explicit and reproducible.

Evidence required:
- S10 readiness matrix artifact.
- Status model definition and update rules.

## TASK-ID: S10-002

- Epic: EPIC-11
- Objective: Build lightweight fixture/harness scaffolding for not-yet-implemented emulated-hardware paths.
- Dependencies: S10-001
- Scope:
  - Define placeholder fixtures and invocation paths for unimplemented engine/hardware flows.
  - Validate harness returns deterministic `blocked_by_missing_engine` markers when appropriate.
  - Capture one smoke run proving the scaffold and evidence format are stable.

Acceptance criteria:
1. Placeholder fixture paths are documented and executable.
2. Missing-engine conditions map to deterministic readiness markers.
3. Smoke output is captured in reusable evidence format.

Evidence required:
- Fixture/scaffold definition artifact.
- Smoke run capture with readiness markers.

## TASK-ID: S10-003

- Epic: EPIC-11
- Objective: Define observational SLO baseline reporting without enforcing hard threshold gates.
- Dependencies: S10-001
- Scope:
  - Reframe SLO reporting to observed metrics with confidence notes.
  - Separate metric collection validity from product-performance claims.
  - Publish escalation guidance only for instrumentation failures, not product regressions.

Acceptance criteria:
1. SLO report distinguishes observation quality from product quality.
2. Hard gate language is absent until engine/hardware paths are implemented.
3. Instrumentation-failure handling path is explicit.

Evidence required:
- Observational SLO baseline report template.
- One sample observational report record.

## TASK-ID: S10-004

- Epic: EPIC-11
- Objective: Produce integration-readiness confidence report across executable, partial, and blocked domains.
- Dependencies: S10-002, S10-003
- Scope:
  - Summarize what is executable today and what remains blocked by missing engine code.
  - Link every statement to concrete artifacts.
  - Publish short next-actions list to unlock blocked areas.

Acceptance criteria:
1. Confidence report uses only evidence-backed claims.
2. Blocked areas include explicit unblock prerequisites.
3. Report is concise and suitable for sprint review.

Evidence required:
- S10 confidence report.
- Linked evidence index.

## TASK-ID: S10-005

- Epic: EPIC-11
- Objective: Run S10 review gate and produce transition plan to first hard-validation sprint.
- Dependencies: S10-004
- Scope:
  - Execute one review meeting artifact using S10 outputs.
  - Decide which criteria are ready to move from readiness mode to hard validation.
  - Publish transition plan with candidate S11 hard-validation tasks.

Acceptance criteria:
1. Review decision artifact is produced with clear go/no-go notes.
2. Transition plan identifies explicit criteria eligible for hard validation.
3. No criterion is marked hard-gated without executable engine/hardware path evidence.

Evidence required:
- S10 review decision artifact.
- S10-to-S11 transition plan.

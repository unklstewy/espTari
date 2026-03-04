# API Contract Delta Log

Purpose:
- Single canonical ledger for API contract changes.
- Every contract-affecting change must have one delta entry before release acceptance.

Scope:
- `docs/EMU_ENGINE_V2_API_SPEC.md`
- `docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md`
- Runtime endpoints/guards/envelopes that implement API contract semantics.

## Entry format

Each entry MUST include:
- Delta ID (`DELTA-YYYYMMDD-###`)
- Date
- Sprint/Task reference
- Contract area
- Change summary (plain language + technical)
- Backward compatibility impact
- Affected endpoints/guards
- Files changed
- Verification evidence
- Release note link
- Acceptance log link
- Decision status

---

## Delta entries

### DELTA-20260304-001

- Date: 2026-03-04
- Sprint/Task: `S9-001`
- Contract area: Process/Governance
- Change summary:
  - Established mandatory API delta logging workflow and templates.
  - Added explicit traceability rules from contract change -> verification evidence -> release note -> acceptance decision.
- Backward compatibility impact: None (process-only change)
- Affected endpoints/guards: None
- Files changed:
  - `TRACKING/API_CONTRACT_DELTA_LOG.md`
  - `TRACKING/API_CONTRACT_DELTA_TEMPLATE.md`
  - `TRACKING/API_CONTRACT_DELTA_WORKFLOW.md`
- Verification evidence:
  - `TRACKING/REMAINING_WORK_DECOMPOSITION_2026-03-04.md` (S9 decomposition + pull order)
  - DB status/event records for `S9-001`
- Release note link: `TRACKING/RELEASE_NOTES.md` (pending S9 release entry)
- Acceptance log link: `TRACKING/ACCEPTANCE_LOG.md` (pending S9 acceptance row)
- Decision status: In Progress

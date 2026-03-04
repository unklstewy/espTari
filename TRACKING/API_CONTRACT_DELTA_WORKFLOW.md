# API Contract Delta Workflow

This workflow is mandatory for contract-affecting changes.

## Goal
Create deterministic traceability from API contract change to acceptance decision.

## Trigger conditions
Run this workflow when any of the following changes:
- API envelope/error semantics
- Endpoint request/response contract
- Guard/error mapping behavior
- Stream/snapshot payload schema
- Compatibility/restore/save contract semantics

## Steps

1) Create delta entry
- Add a new entry in `TRACKING/API_CONTRACT_DELTA_LOG.md` using `TRACKING/API_CONTRACT_DELTA_TEMPLATE.md`.
- Assign unique `DELTA-YYYYMMDD-###` ID.

2) Implement change
- Update contract docs and runtime code.
- Keep endpoint and guard semantics deterministic.

3) Verify change
- Run the narrowest relevant verification first (route check, endpoint smoke, or targeted harness).
- Capture evidence paths (script output, capture files, bundle files).

4) Update release notes
- Add a release note row/section that references the delta ID and evidence.

5) Update acceptance log
- Add acceptance row with task ID and evidence links.
- Decision must align with release note outcome.

6) Close delta
- Set delta entry decision to `Accepted` after PO acceptance.

## Definition of Done for contract deltas
A delta is complete only when all are true:
- Delta entry exists and is complete.
- Verification evidence links are present and valid.
- Release note section exists and references the delta.
- Acceptance log row exists and references the same evidence.
- Task status is moved to `Done` in DB.

## Minimal checklist
- [ ] Delta ID created
- [ ] Contract/runtime changes linked
- [ ] Evidence captured
- [ ] Release notes updated
- [ ] Acceptance log updated
- [ ] DB status/event updated

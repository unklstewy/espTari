# PRQ-001 Gap Register

Date: 2026-03-02  
Task: PRQ-001  
Scope: Planning assumptions and implementation-readiness gaps only (no runtime evidence)

| Gap ID | Domain | Description | Severity | Owner | Mitigation action | Dependency | Target close date | Status |
|---|---|---|---|---|---|---|---|---|
| PRQ1-GAP-001 | lifecycle | Runtime evidence captured; contract deltas remain for resume-from-stopped and reset semantics/guards. | High | Engineering | Align lifecycle semantics with contract for `resume` and `reset`, then rerun denial-path matrix and attach closure evidence. | CRT-005 | 2026-03-05 | Partially Closed |
| PRQ1-GAP-002 | input mapping | Active-profile apply cutover/no-op boundary notes require reviewer confirmation of ownership and completeness. | Medium | Engineering | Implemented runtime CRUD/apply path and captured smoke evidence for create/list/get/patch/apply/active plus startup stability; retain only conflict/not-found negative-case closure for full signoff. | PRQ1-GAP-001 | 2026-03-05 | Partially Closed |
| PRQ1-GAP-003 | save/restore | Suspend/restore routes and baseline flows implemented; full compatibility validator and remaining negative matrix rows still open. | High | Engineering | Complete `resume_mode=paused` and remaining guard/error matrix (`ENGINE_NOT_SUSPENDED`, `BAD_REQUEST`, `SNAPSHOT_INCOMPATIBLE`) with evidence links. | PRQ1-GAP-001 | 2026-03-06 | Partially Closed |
| PRQ1-GAP-004 | observability | Stream/inspect routes implemented with guard behavior; telemetry/backpressure/alarm depth evidence still open. | Medium | Engineering + QA | Complete filter-invalid mapping, backpressure counters, and SLO alarm chronology checks with captured evidence. | PRQ1-GAP-003 | 2026-03-06 | Partially Closed |

## Notes

- Gap entries are initial planning assumptions and are subject to refinement during PRQ-001 execution.
- Runtime/API/build/flash/test execution is out of scope until phase-gate unlock.
- Gap statuses track review-stage closure and do not imply runtime validation.

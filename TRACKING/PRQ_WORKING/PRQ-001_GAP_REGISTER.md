# PRQ-001 Gap Register

Date: 2026-03-02  
Task: PRQ-001  
Scope: Planning assumptions and implementation-readiness gaps only (no runtime evidence)

| Gap ID | Domain | Description | Severity | Owner | Mitigation action | Dependency | Target close date | Status |
|---|---|---|---|---|---|---|---|---|
| PRQ1-GAP-001 | lifecycle | Runtime evidence captured; contract deltas remain for resume-from-stopped and reset semantics/guards. | High | Engineering | Align lifecycle semantics with contract for `resume` and `reset`, then rerun denial-path matrix and attach closure evidence. | CRT-005 | 2026-03-05 | Partially Closed |
| PRQ1-GAP-002 | input mapping | Active-profile apply cutover/no-op boundary notes require reviewer confirmation of ownership and completeness. | Medium | Engineering | Implemented runtime CRUD/apply path and captured smoke evidence for create/list/get/patch/apply/active plus startup stability; retain only conflict/not-found negative-case closure for full signoff. | PRQ1-GAP-001 | 2026-03-05 | Partially Closed |
| PRQ1-GAP-003 | save/restore | Suspend/restore guard matrix and compatibility validator baseline are validated (`RCOMP-01..04`, strict/non-strict validator behavior). Residual is durable snapshot-metadata persistence source (currently encoded in snapshot-id metadata tokens). | High | Engineering | Replace snapshot-id metadata token parsing with persisted snapshot metadata records while preserving deterministic `RCOMP-*` ordering and error mapping. | PRQ1-GAP-001 | 2026-03-06 | Partially Closed |
| PRQ1-GAP-004 | observability | Filter-invalid mapping, backpressure counters, SLO breach/recover chronology, and sustained-load monotonicity/counter invariants are now evidenced (`captures/stream_soak_20260302_181711_{summary,csv}`). Residual narrowed to optional extended-duration soak hardening. | Low | Engineering + QA | Maintain periodic extended-duration soak runs as hardening evidence; no blocker for baseline PRQ closure. | PRQ1-GAP-003 | 2026-03-06 | Closed |

## Notes

- Gap entries are initial planning assumptions and are subject to refinement during PRQ-001 execution.
- Runtime/API/build/flash/test execution is out of scope until phase-gate unlock.
- Gap statuses track review-stage closure and do not imply runtime validation.

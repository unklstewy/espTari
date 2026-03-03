# PRQ-001 Gap Register

Date: 2026-03-02  
Task: PRQ-001  
Scope: Planning assumptions and implementation-readiness gaps only (no runtime evidence)
Scope: Runtime-backed closure evidence for PRQ-001 residual gaps

| Gap ID | Domain | Description | Severity | Owner | Mitigation action | Dependency | Target close date | Status |
|---|---|---|---|---|---|---|---|---|
| PRQ1-GAP-001 | lifecycle | Lifecycle contract deltas are closed. Evidence shows `resume` from `stopped` and `reset` from `stopped` are denied with `INVALID_SESSION_STATE` (`409`), while `reset` from `running/paused` returns `200` and leaves session in `running`; `resume` from `paused` returns `200`. Evidence capture: `captures/lifecycle_residual_closure_20260302.txt`. | High | Engineering | Preserve lifecycle guard/error mapping for `resume`/`reset` and keep denial-path checks in regression smoke. | CRT-005 | 2026-03-05 | Closed |
| PRQ1-GAP-002 | input mapping | Conflict/not-found negative-case closure is now evidenced: apply with revision mismatch and delete-active mapping return `CONFLICT` (`409`), while get/patch/apply on unknown IDs return `INPUT_MAPPING_NOT_FOUND` (`404`). Evidence capture: `captures/input_mapping_negcase_20260302.txt`. | Medium | Engineering | Preserve deterministic input-mapping error mapping and maintain negative-case checks in regression smoke. | PRQ1-GAP-001 | 2026-03-05 | Closed |
| PRQ1-GAP-003 | save/restore | Suspend/restore compatibility source is now persisted in durable snapshot metadata records (`/spiffs/snapshot_meta_<hash>.meta`) with deterministic `RCOMP-01..04` evaluation preserved. Readiness-gated API evidence on `esptari.local` confirms `session -> suspend-save -> validate(strict=true) -> restore-resume` success and malformed metadata rejection (`400 BAD_REQUEST`). | High | Engineering | Maintain compatibility-rule ordering and error mapping while using persisted metadata records as the restore/validate source of truth. | PRQ1-GAP-001 | 2026-03-06 | Closed |
| PRQ1-GAP-004 | observability | Filter-invalid mapping, backpressure counters, SLO breach/recover chronology, and sustained-load monotonicity/counter invariants are now evidenced (`captures/stream_soak_20260302_181711_{summary,csv}`). Residual narrowed to optional extended-duration soak hardening. | Low | Engineering + QA | Maintain periodic extended-duration soak runs as hardening evidence; no blocker for baseline PRQ closure. | PRQ1-GAP-003 | 2026-03-06 | Closed |

## Notes

- Gap entries and statuses reflect runtime-backed closure state for PRQ-001 residuals.
- Evidence links in each row point to closure artifacts and API verification outputs.

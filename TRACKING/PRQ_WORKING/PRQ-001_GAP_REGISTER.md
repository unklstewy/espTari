# PRQ-001 Gap Register

Date: 2026-03-02  
Task: PRQ-001  
Scope: Planning assumptions and implementation-readiness gaps only (no runtime evidence)

| Gap ID | Domain | Description | Severity | Owner | Mitigation action | Dependency | Target close date | Status |
|---|---|---|---|---|---|---|---|---|
| PRQ1-GAP-001 | lifecycle | Transition/guard matrix is not yet fully mapped to implementation areas for suspend-save/restore-resume edge paths. | High | Engineering | Complete lifecycle closure checklist and guard-to-implementation traceability map before PRQ-001 review. | CRT-005 | TBD | Open |
| PRQ1-GAP-002 | input mapping | Active-profile apply cutover and no-op semantics lack finalized implementation boundary notes and ownership mapping. | Medium | Engineering | Draft and review input mapping boundary notes; assign owners for cutover and monotonicity checkpoints. | PRQ1-GAP-001 | TBD | Open |
| PRQ1-GAP-003 | save/restore | Compatibility closure map for schema/abi/profile is not consolidated with sequencing gap register. | High | Engineering | Build save/restore compatibility map and add explicit sequencing mitigation list for implementation handoff. | PRQ1-GAP-001 | TBD | Open |
| PRQ1-GAP-004 | observability | Unified readiness matrix for stream payload/order/filter/backpressure/SLO alarm sequencing is missing. | Medium | Engineering + QA | Consolidate observability readiness matrix with ownership split and traceability links to planned implementation areas. | PRQ1-GAP-003 | TBD | Open |

## Notes

- Gap entries are initial planning assumptions and are subject to refinement during PRQ-001 execution.
- Runtime/API/build/flash/test execution is out of scope until phase-gate unlock.

# Acceptance Log

Use this log for Product Owner / Acceptance Master decisions.

## Decision entries

| Date | Sprint | Task ID | Decision | Notes | Evidence Link |
|---|---|---|---|---|---|
| YYYY-MM-DD | Sx | T-XXX | Accepted / Rejected / Deferred |  |  |
| 2026-03-02 | S1-S4 | T-056..T-070 | Accepted | Catalog/index-sync and input-capture/mapping contract slices reviewed and accepted by PO. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S3-S4 | T-072..T-085 | Accepted | Input translation, profile wiring, media attach/eject, and stream metadata/pacing contract slices reviewed and accepted by PO. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S4 | T-088..T-098 | Accepted | Bus/memory filters, backpressure telemetry, conformance harness, and chipset/MFP model contract slices reviewed and accepted by PO. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S1 | T-054 | Accepted | Lifecycle transition matrix and guard predicates contract reviewed and accepted by PO. | docs/EMU_ENGINE_V2_API_SPEC.md#L234-L270 |
| 2026-03-02 | S1 | T-055 | Accepted | Lifecycle guard validator response/error mapping contract reviewed and accepted by PO. | docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md#L285 |
| 2026-03-02 | S3 | T-071 | Accepted | Mapping CRUD and active-profile apply path contract reviewed and accepted by PO. | docs/EMU_ENGINE_V2_API_SPEC.md#L3244-L3368 |
| 2026-03-02 | S4 | T-087 | Accepted | Register snapshot stream publisher and validation checks contract reviewed and accepted by PO. | docs/EMU_ENGINE_V2_API_SPEC.md#L4544-L4610 |
| 2026-03-02 | S2 | T-099 | Accepted | MFP interrupt emission behaviors and conformance checks contract reviewed and accepted by PO. | docs/EMU_ENGINE_V2_API_SPEC.md#L5285-L5310 |
| 2026-03-02 | S4 | T-086 | Accepted | Register snapshot schema + selective filter fields contract closed and verified. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S3 | T-100 | Accepted | ACIA bridge/framing contract documented with deterministic checks and examples. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S3 | T-101 | Accepted | IKBD packet parser/timing contract documented with deterministic checks and examples. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S3 | T-102 | Accepted | DMA pacing/arbitration hook contract documented with deterministic checks and examples. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S3 | T-103 | Accepted | FDC command/status FSM terminal-condition contract documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S3 | T-104 | Accepted | PSG register/audio behavior contract documented with deterministic checks. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S3 | T-105 | Accepted | PSG GPIO behavior + validation-check contract documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S3 | T-106 | Accepted | Interrupt hierarchy/vector routing contract documented with deterministic checks. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S3 | T-107 | Accepted | Interrupt wiring integration-check contract documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S3 | T-108 | Accepted | Startup defaults/power-on baseline contract documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S3 | T-109 | Accepted | Reset/startup sequence executor verification contract documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S4 | T-110 | Accepted | Subsystem conformance scaffold/fixture contract documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S4 | T-111 | Accepted | Per-subsystem acceptance suite/reporting contract documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S4 | T-112 | Accepted | Suspend-save request wiring + transition-check contract documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S4 | T-113 | Accepted | Restore-resume guard/error semantics contract documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S4 | T-114 | Accepted | Restore compatibility matrix (schema/abi/profile) contract documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S4 | T-115 | Accepted | Restore compatibility validator + error mapping contract documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S4 | T-116 | Accepted | Performance SLO collector/sampling pipeline contract documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S4 | T-117 | Accepted | SLO endpoint + threshold-breach alarm contract documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md |
| 2026-03-02 | S4 | T-118 | Accepted | Clock control model/bounds contract added with deterministic checks and examples. | docs/EMU_ENGINE_V2_API_SPEC.md#L1130-L1179 |
| 2026-03-02 | S4 | T-119 | Accepted | Deterministic mode transition flow and idempotency semantics documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md#L1182-L1250 |
| 2026-03-02 | S4 | T-120 | Accepted | Single-step control + scheduler-hook contract and guard mapping documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md#L1262-L1333 |
| 2026-03-02 | S4 | T-121 | Accepted | Opcode/bus-error capture payload schemas and deterministic capture checks documented and validated. | docs/EMU_ENGINE_V2_API_SPEC.md#L1336-L1422 |
| 2026-03-02 | S5 | CRT-001 | Pending | Implementation-readiness planning in progress. Runtime execution blocked by phase gate. Evidence bundle links: TRACKING/CRT_READINESS/CRT-001_READINESS.md, TBD. PO decision timestamp: TBD. | TRACKING/CRT_READINESS/CRT-001_READINESS.md |
| 2026-03-02 | S5 | CRT-002 | Pending | Implementation-readiness planning in progress. Runtime execution blocked by phase gate. Evidence bundle links: TRACKING/CRT_READINESS/CRT-002_READINESS.md, TBD. PO decision timestamp: TBD. | TRACKING/CRT_READINESS/CRT-002_READINESS.md |
| 2026-03-02 | S5 | CRT-003 | Pending | Implementation-readiness planning in progress. Runtime execution blocked by phase gate. Evidence bundle links: TRACKING/CRT_READINESS/CRT-003_READINESS.md, TBD. PO decision timestamp: TBD. | TRACKING/CRT_READINESS/CRT-003_READINESS.md |
| 2026-03-02 | S5 | CRT-004 | Pending | Implementation-readiness planning in progress. Runtime execution blocked by phase gate. Evidence bundle links: TRACKING/CRT_READINESS/CRT-004_READINESS.md, TBD. PO decision timestamp: TBD. | TRACKING/CRT_READINESS/CRT-004_READINESS.md |
| 2026-03-02 | S5 | CRT-005 | Pending | Implementation-readiness planning in progress. Runtime execution blocked by phase gate. Evidence bundle links: TRACKING/CRT_READINESS/CRT-005_READINESS.md, TBD. PO decision timestamp: TBD. | TRACKING/CRT_READINESS/CRT-005_READINESS.md |
| 2026-03-02 | S5 | PRQ-001 | Pending | Planning artifact creation in progress; runtime evidence blocked by phase gate. Evidence bundle links: TRACKING/PRQ_WORKING/PRQ-001_DOMAIN_CLOSURE_MATRIX.md, TRACKING/PRQ_WORKING/PRQ-001_GAP_REGISTER.md. PO decision timestamp: TBD. | TRACKING/PRQ_WORKING/PRQ-001_DOMAIN_CLOSURE_MATRIX.md |
| 2026-03-02 | S5 | CRT-001..CRT-005 | Deferred | PO decision record is approved (`hold`) in Section 7 of CRT handoff. Runtime/API validation remains blocked until all prerequisites in Section 6 are closed; re-review scheduled for 2026-03-09. | TRACKING/CRT_HANDOFF_S5_SUMMARY.md |

## Decision policy

- Accepted: Task meets all acceptance criteria and DoD.
- Rejected: Task misses one or more acceptance criteria; include corrective action.
- Deferred: Task complete but decision postponed; include decision deadline.

## Rejection template

- Task ID:
- Missing criteria:
- Required correction:
- Re-review date:

# Emulation Engine v2 Architecture 90 Percent Closure Checklist

Date: 2026-03-01  
Owner: Product Owner and Acceptance Master

Purpose:

- Define the exact closure gates to move architecture-definition completeness from 84 percent to 90 percent or higher.
- Lock newly added requirements for machine state saving and restoring, suspend-save and restore-resume flows, performance SLOs, and debug clock controls.

---

## 1) Closure score model

Target score:

- Overall architecture-definition score: >= 90/100

Weighted domains:

- Core architecture and contracts: 25
- API contract specificity: 25
- Hardware-conformance traceability: 20
- Performance and determinism gates: 15
- Delivery readiness and backlog decomposition: 15

Pass condition:

- No domain score under 80
- All P0 checklist items complete

---

## 2) P0 closure items (must complete)

### P0-A Machine state save and restore contract

- Define canonical machine save-state object with required fields:
  - cpu/shifter/mfp/acia/dma/fdc/psg register blocks
  - mapped RAM/ROM state references
  - scheduler counters (tick and cycle)
  - active media bindings
  - model profile and ABI compatibility stamp
- Define save and restore semantics:
  - suspend and save
  - restore and resume
  - incompatible snapshot rejection behavior
- Define integrity requirements:
  - hash/checksum and schema version in metadata

Acceptance evidence:

- API examples for save and restore requests/responses
- State transition table includes suspended flows
- Error-code coverage for save and restore failures

### P0-B Hard performance metrics

Required SLOs:

- Input-device end-to-end latency target: 50 ms or less
- Runtime jitter target: less than 30 ms
- Dropped-frame rate target: less than 1 percent

Acceptance evidence:

- Dedicated metrics schema and endpoint contract
- Definition of measurement windows and aggregation intervals
- Failure and degraded-state signaling rules

### P0-C Debug clock controls

- Define runtime clock modes:
  - realtime
  - slow_motion (ratio-based)
  - single_step
- Define control operations:
  - set clock mode and ratio
  - execute single CPU step or bounded step count
  - resume realtime
- Ensure observability integration for opcode, bus error, and trace capture workflows.

Acceptance evidence:

- API endpoint contract and payloads
- Guardrails for invalid mode transitions while running streams
- Determinism note for debugging versus realtime operation

---

## 3) P1 closure items (next)

- Freeze EBIN ABI compatibility matrix with clear forward/backward rules.
- Define deterministic replay artifact format for acceptance packs.
- Finish decomposition of all L tasks in backlog into XS or S slices.

---

## 4) Traceability matrix

- Implementation plan source: docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
- API contract source: docs/EMU_ENGINE_V2_API_SPEC.md
- Epic coverage source: TRACKING/EPICS.md
- Task coverage source: TRACKING/BACKLOG.md

Each P0 item is complete only when all four sources reflect equivalent semantics.

---

## 5) Completion decision

Architecture is considered >= 90 percent complete when:

1. P0-A, P0-B, and P0-C are complete with acceptance evidence.
2. Domain scores are recalculated and each is >= 80.
3. Overall weighted score is >= 90.

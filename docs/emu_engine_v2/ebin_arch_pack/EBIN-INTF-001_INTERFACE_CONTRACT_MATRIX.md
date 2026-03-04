# EBIN-INTF-001 Interface and Contract Matrix

## Purpose
Provide execution-ready interface/contract matrix for EBIN loading and ST subsystem integration with invariants and failure modes.

## Scope
- EBIN runtime ABI and loader interfaces
- Subsystem integration interfaces needed for ST baseline
- Failure semantics and deterministic envelope expectations

## Dependencies
- `docs/emu_engine_v2/EBIN_RUNTIME_ABI_V1.md`
- `docs/emu_engine_v2/04_memory_io_map.md`
- `docs/emu_engine_v2/A_interrupt_hierarchy_vectors.md`
- `TRACKING/S5_RUNTIME_UNLOCK_TRACEABILITY_MATRIX_2026-03-04.md`

## Interfaces

| Interface ID | Input | Output | Invariants | Failure modes | Tag |
|---|---|---|---|---|---|
| EBIN-IF-001 Manifest admission | EBIN header/manifest fields (`module_id`, `module_type`, `machine_targets`, `abi_version`, `api_contract_version`, `exports`, `dependencies`, `payload_sha256`) | Normalized internal model or rejection envelope | Required fields present; type constraints hold; first-fail deterministic behavior | `EBIN_INVALID` for missing/invalid required field | `dev_allowed_now` |
| EBIN-IF-002 Compatibility gate | normalized model + active machine profile + host ABI matrix | compatible/incompatible decision | `module_type` valid; `machine_targets` contains active profile; ABI range satisfied | `EBIN_ABI_MISMATCH`, `EBIN_INVALID` | `dev_allowed_now` |
| EBIN-IF-003 Integrity/signature gate | payload bytes + hash/signature metadata | pass/fail with details | hash/signature check order is deterministic; fail-fast | `EBIN_SIGNATURE_INVALID`, `EBIN_INVALID` | `dev_allowed_now` |
| EBIN-IF-004 Dependency resolver | catalog index + dependency ranges + active profile | resolved module set or deterministic diagnostic | deterministic selection for same catalog input; required deps cannot be skipped | `EBIN_DEPENDENCY_MISSING`, deterministic ambiguous/missing diagnostics | `dev_allowed_now` |
| EBIN-IF-005 Load orchestration | resolved set + runtime state | loaded component graph + lifecycle state transitions | ordered path `resolve->validate->gate->bind->init`; rollback-safe | `INTERNAL_ERROR` with preserved control-plane operability | `dev_allowed_now` |
| EBIN-IF-006 Unload orchestration | active component graph + lifecycle state | drained/deinitialized/released runtime graph | ordered `pause->drain->deinit->release`; idempotent unload intent | deterministic internal/recoverable unload errors | `dev_allowed_now` |
| ST-IF-001 Arbitration tick hook | CPU cycle intent + SHIFTER/DMA demand signals | granted bus owner and timestamped arbitration decision | deterministic arbitration ordering under identical inputs | timing mismatch/ordering assertion failures | `integration_blocked` |
| ST-IF-002 Interrupt delivery hook | source assertions (MFP/video/FDC/ACIA) + current IPL state | vector delivery + source clear/update behavior | vector routing order follows appendix contract; no hidden priority inversions | missed/late vector, stale pending bits, wrong vector ID | `integration_blocked` |
| ST-IF-003 Reset/startup baseline hook | reset cause + machine profile + persisted state options | deterministic register/state defaults | startup defaults match appendix baseline and profile | startup baseline mismatch, invalid transition guard response | `integration_blocked` |
| ST-IF-004 Stream observability projection | internal timing/register/bus events + filter config | API/event payloads with deterministic ordering | payload schema conformance and monotonic sequencing | invalid payload fields, ordering gaps, overload/backpressure events | `integration_blocked` |
| REL-IF-001 Production package trust | signed production EBIN bundle + key policy | release-grade trust admission | trust roots and signing policy are enforced, not bypassed by test package rules | unsigned/untrusted package rejection | `release_blocked` |

## Acceptance criteria
1. Each interface defines inputs, outputs, invariants, and failure modes.
2. Failure codes are aligned with current EBIN ABI contract and canonical envelopes.
3. Integration/release blockers are explicitly tagged and separated from development-ready interfaces.

## Evidence expected
- Manifest/gate/resolver/orchestration smoke outputs (`S5-*` captures).
- Interface-level deterministic replay records for arbitration/interrupt/reset hooks.
- Release trust validation evidence for production-signing policy.

## Cross-links
- [EBIN-ARCH-000 Index](EBIN-ARCH-000_INDEX.md)
- [EBIN-ARCH-002 Subsystem Decomposition and Dependency Order](EBIN-ARCH-002_SUBSYSTEM_DECOMPOSITION_AND_ORDER.md)
- [EBIN-ARCH-004 Determinism, Performance, Observability Plan](EBIN-ARCH-004_DETERMINISM_PERF_OBSERVABILITY_PLAN.md)
- [EBIN-GATE-001 Integration Gates and Non-Go Conditions](EBIN-GATE-001_INTEGRATION_GATES_AND_NON_GO.md)

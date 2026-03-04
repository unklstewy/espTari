# EBIN-ARCH-003 Sprint-Ready Work Breakdown

## Purpose
Translate EBIN architecture into pullable execution units (epic → story → task) aligned to existing tracking and one-cycle delivery.

## Scope
- Development-cycle actionable tasks only
- Mapping to current tracking IDs and S5 evidence chain
- Explicit `Do / Don’t`, test intent, evidence, and readiness tags

## Dependencies
- `TRACKING/BACKLOG.md`
- `TRACKING/KANBAN_BOARD.md`
- `TRACKING/TASK_CARDS_S5_RUNTIME_UNLOCK.md`
- `TRACKING/S5_RUNTIME_UNLOCK_TRACEABILITY_MATRIX_2026-03-04.md`
- `docs/emu_engine_v2/EBIN_RUNTIME_ABI_V1.md`

## Interfaces
- Input: accepted contract + tracking task inventory
- Output: independently pullable tasks with deterministic acceptance targets

## Epic map

| Epic | Goal | Tracking alignment |
|---|---|---|
| EBIN-E1 Runtime Admission Stability | Keep EBIN admission deterministic and contract-safe as baseline for subsystem modules | S5-001..S5-007 |
| EBIN-E2 ST Subsystem Adapter Bring-up | Bring ST modules online in dependency order without contract drift | T-096..T-109 |
| EBIN-E3 Conformance/Observability Closure | Close integration evidence and release readiness packet | T-110..T-111, T-092..T-095, T-116..T-117 |

## Stories and one-cycle tasks

### EBIN-E1 Runtime Admission Stability

| Story | Task ID | Task | Do | Don’t | Test intent | Evidence expected | Tag |
|---|---|---|---|---|---|---|---|
| E1-S1 Admission contract lock | E1-S1-T1 | Freeze ABI field/check order in loader admission tests | Enforce current field list and fail-fast ordering | Do not change ABI schema fields or error taxonomy | Re-run manifest/gate deterministic probes | Existing + refreshed S5-002/S5-003 style capture | `dev_allowed_now` |
| E1-S2 Resolver determinism lock | E1-S2-T1 | Pin deterministic resolution policy fixtures for ambiguous/missing scenarios | Add fixed fixture catalogs and expected outcomes | Do not alter resolver selection semantics | Repeat same catalog 10x and assert identical output | Resolver fixture outputs + deterministic diff report | `dev_allowed_now` |
| E1-S3 Load/unload rollback safety lock | E1-S3-T1 | Add negative-path replay cases around rollback/fallback | Validate control-plane remains operable after activation failures | Do not broaden lifecycle contract states | Replay load fail -> rollback -> recovery sequence | Orchestration + rollback replay captures | `dev_allowed_now` |

### EBIN-E2 ST Subsystem Adapter Bring-up

| Story | Task ID | Task | Do | Don’t | Test intent | Evidence expected | Tag |
|---|---|---|---|---|---|---|---|
| E2-S1 Arbitration cluster baseline | E2-S1-T1 (T-096 align) | Implement GLUE/MMU/SHIFTER adapter surface preserving register/timing contracts | Implement adapter boundaries and timing hook points only | Do not redefine memory map or timing model | Contract-conformance unit checks for register windows and arbitration hooks | Adapter conformance logs + timing hook smoke | `dev_allowed_now` |
| E2-S1 Arbitration integration | E2-S1-T2 (T-097 align) | Integrate arbitration hook with scheduler and assert slot ordering | Verify CPU/SHIFTER/DMA contention ordering in replay | Do not switch to statistical wait-state model | Deterministic replay across fixed trace vectors | Arbitration trace captures | `integration_blocked` |
| E2-S2 Interrupt/timer domain | E2-S2-T1 (T-098 align) | Implement MFP register/timer adapter contract | Preserve ISR/IPR/IMR and timer semantics | Do not remap vectors or MFP register semantics | Timer decrement and pending/in-service checks | MFP conformance captures | `dev_allowed_now` |
| E2-S2 Interrupt wiring closure | E2-S2-T2 (T-099/T-106/T-107 align) | Wire interrupt hierarchy and vector routing across sources | Validate priority and vector order against appendix | Do not alter interrupt hierarchy contract | End-to-end interrupt ordering replay suite | Vector-order artifact bundle | `integration_blocked` |
| E2-S3 Serial/input path | E2-S3-T1 (T-100/T-101 align) | Implement ACIA+IKBD adapter path with framing and cadence guarantees | Keep packet and IRQ behavior deterministic | Do not bypass ACIA timing via direct HLE shortcuts | Framing/cadence negative and positive path tests | ACIA/IKBD timing captures | `dev_allowed_now` |
| E2-S4 Storage path | E2-S4-T1 (T-102/T-103 align) | Implement DMA/FDC adapter pacing and FSM terminal conditions | Preserve DRQ/INTRQ-driven cadence and diagnostics | Do not collapse transfers into instant operations | DRQ pacing and command FSM replay checks | DMA/FDC replay captures | `dev_allowed_now` |
| E2-S5 Audio + sideband | E2-S5-T1 (T-104/T-105 align) | Implement PSG register/audio and GPIO sideband contract | Validate phase continuity and sideband behavior | Do not decouple GPIO effects from runtime path | Audio register timeline + GPIO effect checks | PSG timing captures | `dev_allowed_now` |
| E2-S6 Startup/reset closure | E2-S6-T1 (T-108/T-109 align) | Enforce startup defaults and reset sequence verification | Validate power-on baseline fields by profile | Do not change startup baseline table definitions | Reset sequence deterministic replay | Startup baseline report | `integration_blocked` |

### EBIN-E3 Conformance/Observability Closure

| Story | Task ID | Task | Do | Don’t | Test intent | Evidence expected | Tag |
|---|---|---|---|---|---|---|---|
| E3-S1 Subsystem conformance suites | E3-S1-T1 (T-110/T-111 align) | Build and run per-subsystem conformance suites over EBIN adapters | Keep check IDs and expected outputs traceable | Do not add untracked check families | Run deterministic suite package with fixed fixtures | Sectioned conformance reports | `integration_blocked` |
| E3-S2 Evidence packaging | E3-S2-T1 (T-092..T-095 align) | Package evidence and acceptance packet for phase gates | Keep 1:1 check-to-artifact mapping | Do not ship with missing evidence rows | Validate completeness and reproducibility of packet | Evidence index + traceability matrix update | `integration_blocked` |
| E3-S3 SLO and alarm closure | E3-S3-T1 (T-116/T-117 align) | Validate SLO exposure under stress and threshold alarms | Confirm latency/jitter/frame-drop metrics behavior | Do not change SLO thresholds in architecture docs | Stress replay for threshold breach/non-breach | SLO stress capture set | `release_blocked` |

## Acceptance criteria
1. Every task is actionable within one development cycle.
2. Every task includes explicit Do/Don’t, test intent, and evidence artifact expectations.
3. Every task has one of: `dev_allowed_now`, `integration_blocked`, `release_blocked`.

## Evidence expected
- Task-level capture artifacts and deterministic replay logs.
- Updated tracking entries and acceptance logs per completed story.

## Cross-links
- [EBIN-ARCH-000 Index](EBIN-ARCH-000_INDEX.md)
- [EBIN-ARCH-002 Subsystem Decomposition and Dependency Order](EBIN-ARCH-002_SUBSYSTEM_DECOMPOSITION_AND_ORDER.md)
- [EBIN-INTF-001 Interface and Contract Matrix](EBIN-INTF-001_INTERFACE_CONTRACT_MATRIX.md)
- [EBIN-GATE-001 Integration Gates and Non-Go Conditions](EBIN-GATE-001_INTEGRATION_GATES_AND_NON_GO.md)

# EBIN-ARCH-002 Subsystem Decomposition and Dependency Order

## Purpose
Define EBIN implementation decomposition by ST subsystem with strict dependency order for deterministic bring-up.

## Scope
- ST baseline subsystem modules and integration sequence
- Dependencies and integration criteria per subsystem
- Development/integration/release state tags per subsystem

## Dependencies
- `docs/emu_engine_v2/02_system_block_diagram.md`
- `docs/emu_engine_v2/03_component_spec_table.md`
- `docs/emu_engine_v2/A_interrupt_hierarchy_vectors.md`
- `docs/emu_engine_v2/B_startup_reset_quirks.md`
- `docs/emu_engine_v2/05_timing.md`
- `TRACKING/BACKLOG.md` (T-096..T-109, T-110..T-111)

## Interfaces
- Inputs: subsystem contracts, interrupt hierarchy, timing constraints
- Outputs: dependency-ordered module plan and readiness tags

## Decomposition by subsystem

| Order | Subsystem | EBIN module role | Hard dependencies | Key contract anchors | Current tag |
|---|---|---|---|---|---|
| 1 | Machine profile + clock root | `machine_profile` | none | memory/io map, startup defaults | `dev_allowed_now` |
| 2 | CPU (68000 baseline) | `cpu` | machine profile | timing, exception/IACK behavior | `dev_allowed_now` |
| 3 | GLUE/MMU/SHIFTER arbitration cluster | `video` + `io` split behind same timing core | CPU + profile | arbitration slots, fetch windows, IRQ timing | `dev_allowed_now` |
| 4 | MFP interrupt/timer domain | `io` | CPU + GLUE timing edges | prioritized vectored IRQ behavior | `dev_allowed_now` |
| 5 | ACIA pair + IKBD pipeline | `io` | MFP + interrupt routing | framing, packet cadence, IRQ signaling | `dev_allowed_now` |
| 6 | DMA/FDC storage path (+ ACSI baseline path) | `storage` | arbitration cluster + interrupt path | DRQ/INTRQ FSM, transfer pacing | `dev_allowed_now` |
| 7 | PSG audio + GPIO sideband | `audio` | clock root + optional storage sideband usage | register timing, phase continuity | `dev_allowed_now` |
| 8 | Cross-subsystem interrupt and reset closure | integration harness layer | all above | vector order + startup/reset quirks | `integration_blocked` (until integrated checks complete) |
| 9 | Optional model deltas (Blitter, STe DMA sound, Mega STe controls) | extension modules | baseline stable integration | model-delta appendix | `release_blocked` for baseline release claims |

## Dependency-order execution rules

1. Do not start subsystem order N+1 runtime integration before N has deterministic local evidence.
2. Do not merge cross-subsystem integration until interrupt hierarchy checks pass.
3. Keep optional model-delta work isolated from baseline release path.
4. Preserve existing V2 timing and register contracts; adapters may wrap, not redefine.

## Acceptance criteria
1. Every subsystem has dependency order, contract anchors, and a readiness tag.
2. Order guarantees deterministic bring-up and avoids circular dependency.
3. Optional model deltas are explicitly isolated from baseline claims.

## Evidence expected
- Per-subsystem deterministic smoke artifacts.
- Cross-subsystem interrupt and reset closure artifacts.
- Timing/arbitration replay outputs for CPU+SHIFTER+DMA contention paths.

## Cross-links
- [EBIN-ARCH-000 Index](EBIN-ARCH-000_INDEX.md)
- [EBIN-ARCH-001 Constraints and Phased Roadmap](EBIN-ARCH-001_CONSTRAINTS_AND_PHASED_ROADMAP.md)
- [EBIN-INTF-001 Interface and Contract Matrix](EBIN-INTF-001_INTERFACE_CONTRACT_MATRIX.md)
- [EBIN-ARCH-003 Sprint-Ready Work Breakdown](EBIN-ARCH-003_SPRINT_READY_BREAKDOWN.md)

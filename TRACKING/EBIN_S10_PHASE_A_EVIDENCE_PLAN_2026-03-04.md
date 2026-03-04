# EBIN S10 Phase-A Evidence Plan (2026-03-04)

Scope: EBIN-S10-001 through EBIN-S10-008 only (`dev_allowed_now`).

## Source anchors

- `docs/emu_engine_v2/ebin_arch_pack/EBIN-ARCH-006_DEVELOPER_EXECUTION_CHECKLIST.md`
- `docs/emu_engine_v2/ebin_arch_pack/EBIN-ARCH-003_SPRINT_READY_BREAKDOWN.md`
- `docs/emu_engine_v2/ebin_arch_pack/EBIN-GATE-001_INTEGRATION_GATES_AND_NON_GO.md`

## Task-to-evidence mapping

| Task | Phase | Tag | Gate impact | Do boundary | Don’t boundary | Test intent | Evidence script |
|---|---|---|---|---|---|---|---|
| EBIN-S10-001 | A | dev_allowed_now | D | Freeze ABI field/check-order behavior in admission tests | Do not change ABI schema/error taxonomy | Re-run deterministic manifest/gate probes | `tools/smoke_ebin_s10_001_admission_lock.sh` |
| EBIN-S10-002 | A | dev_allowed_now | D | Freeze resolver fixtures and output ordering | Do not alter selection policy for one-off fixtures | Replay same resolver input and diff canonical output | `tools/smoke_ebin_s10_002_resolver_determinism_lock.sh` |
| EBIN-S10-003 | A | dev_allowed_now | D | Verify fail->rollback/fallback->recovery telemetry | Do not allow non-deterministic partial-failure outcomes | Force activation failures and assert recovery telemetry fields | `tools/smoke_ebin_s10_003_rollback_fallback_lock.sh` |
| EBIN-S10-004 | A | dev_allowed_now | D | Verify GLUE/MMU/SHIFTER adapter surfaces and timing hooks | Do not redefine memory map/arbitration model | Register-window and timing checks | `tools/smoke_ebin_s10_004_glue_mmu_shifter_adapter_baseline.sh` |
| EBIN-S10-005 | A | dev_allowed_now | D | Verify MFP register/timer semantics and IRQ state behavior | Do not remap vector logic/ISR-IPR-IMR semantics | Timer/register and IRQ checks | `tools/smoke_ebin_s10_005_mfp_adapter_baseline.sh` |
| EBIN-S10-006 | A | dev_allowed_now | D | Verify ACIA framing and IKBD cadence path | Do not bypass ACIA timing with HLE injection | Serial framing/cadence positive+negative checks | `tools/smoke_ebin_s10_006_acia_ikbd_adapter_baseline.sh` |
| EBIN-S10-007 | A | dev_allowed_now | D | Verify DMA/FDC pacing and FSM terminal behavior | Do not collapse command phases into immediate transfer | DRQ/INTRQ pacing and FSM replay checks | `tools/smoke_ebin_s10_007_dma_fdc_adapter_baseline.sh` |
| EBIN-S10-008 | A | dev_allowed_now | D | Verify PSG register/audio continuity and GPIO sideband | Do not detach sideband effects | Audio register timeline and sideband checks | `tools/smoke_ebin_s10_008_psg_adapter_baseline.sh` |

## Aggregate runner

- `tools/smoke_ebin_s10_phase_a.sh` executes EBIN-S10-001..008 in order.

## Out-of-scope in this packet

- EBIN-S10-009..017 are `integration_blocked`/`release_blocked` and are not executable under current gate posture.

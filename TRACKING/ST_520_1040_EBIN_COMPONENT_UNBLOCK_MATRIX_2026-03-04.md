# ST 520/1040 EBIN Component Unblock Matrix (2026-03-04)

## Scope

Machine family: Atari ST 520/1040 baseline.

Runtime model: component EBINs staged on SD-card and dynamically loaded into PSRAM during machine profile bootstrap.

## Component matrix

| Component family | Planned EBIN ID | Current blocker | Dependency/gate | Acceptance criterion |
|---|---|---|---|---|
| CPU core | `st.cpu.m68k` | None (baseline validated) | ABI/signature/load safety gates | Deterministic load/unload + arbitration integration smoke pass |
| GLUE/MMU/SHIFTER | `st.chipset.glue_mmu_shifter` | None (adapter baseline validated) | Register/memory window contracts | `S10-004` smoke deterministic pass and evidence packet closure |
| MFP | `st.chipset.mfp` | None (adapter baseline validated) | Timer/IRQ contract conformance | `S10-005` smoke deterministic pass and evidence packet closure |
| ACIA + IKBD | `st.io.acia_ikbd` | None (adapter baseline validated) | Framing/parser contract conformance | `S10-006` smoke deterministic pass and evidence packet closure |
| DMA + FDC | `st.storage.dma_fdc` | None (adapter baseline validated) | DMA arbitration + FDC terminal FSM checks | `S10-007` smoke deterministic pass and evidence packet closure |
| PSG audio + GPIO | `st.audio_gpio.psg` | None (adapter baseline validated) | Audio/GPIO contract conformance | `S10-008` smoke deterministic pass and evidence packet closure |
| Interrupt wiring | `st.integration.interrupt_router` | None (integration validated) | Hierarchy/route/wiring integration checks | `S10-010` deterministic pass |
| Startup/reset profile | `st.integration.startup_profile` | None (integration validated) | Startup defaults + sequence/verification checks | `S10-011` deterministic pass |
| Suspend/restore bridge | `st.integration.suspend_restore` | None (integration validated) | Save/restore lifecycle + guards | `S10-012` deterministic pass |
| Conformance/release harness | `st.integration.release_harness` | None (integration validated) | Checklist/review-pack/signoff-bundle APIs | `S10-013..017` deterministic pass |

## Blocker status

- Component creation blockers are closed in this matrix.
- Runtime hardening/soak blocker (`B-003`) is also closed via soak evidence.
- No active blocker entries remain in current sprint blocker register.

## Traceability anchors

- `TRACKING/EBIN_S10_009_TO_017_CLOSURE_SUMMARY_2026-03-04.md`
- `TRACKING/ACCEPTANCE_LOG.md`
- `TRACKING/PROGRAM_STATUS_SNAPSHOT_2026-03-04.md`

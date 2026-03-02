# 8.1 First machine profile target

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 31

## 8.1 First machine profile target

Phase-1 machine profile:

- `atari_st_520_1040_baseline`

Includes:

- 68000-class CPU behavior contract
- ST chipset group behavior contract (GLUE/MMU/SHIFTER)
- GLUE/MMU/SHIFTER register + memory window model contract (register windows `register_window_v1`, memory windows `memory_window_v1`, and deterministic selector/session guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3A` and is the canonical source.
- GLUE/MMU/SHIFTER arbitration + timing integration checks (`CHIP-TIM-01..04`, deterministic chipset order, and timing monotonicity guards) are defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3A` and are the canonical source.
- MFP register + timer model contracts (`mfp_register_window_v1`, `mfp_timer_model_v1`, and deterministic session/selector guard failures) are defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3B` and are the canonical source.
- MFP interrupt emission behaviors + conformance checks (`MFP-IRQ-01..04`, deterministic interrupt vector mapping, and interrupt timing monotonicity guards) are defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3B` and are the canonical source.
- ACIA host channel bridge + framing rules contract (`acia_bridge_state_v1`, `acia_frame_v1`, checks `ACIA-FRM-01..04`, and deterministic bridge/framing guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3C` and is the canonical source.
- IKBD parser bridge + keyboard/mouse packet timing contract (`ikbd_bridge_state_v1`, `ikbd_packet_timing_v1`, checks `IKBD-PKT-01..04`, and deterministic bridge/timing guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3D` and is the canonical source.
- DMA request pacing model + arbitration hooks contract (`dma_pacing_state_v1`, `dma_arbitration_hook_v1`, checks `DMA-ARB-01..04`, and deterministic pacing/arbitration guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3E` and is the canonical source.
- FDC command/status FSM bridge + terminal-condition contract (`fdc_fsm_state_v1`, `fdc_terminal_event_v1`, checks `FDC-FSM-01..04`, and deterministic FSM/terminal guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3F` and is the canonical source.
- PSG register/audio behavior contract (`psg_register_window_v1`, `psg_audio_state_v1`, checks `PSG-AUD-01..04`, and deterministic register/audio guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3G` and is the canonical source.
- PSG GPIO behavior contract + validation checks (`psg_gpio_state_v1`, `psg_gpio_event_v1`, checks `PSG-GPIO-01..04`, and deterministic GPIO guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3H` and is the canonical source.
- Interrupt hierarchy map + vector routing contract (`interrupt_hierarchy_v1`, `interrupt_route_event_v1`, checks `INT-MAP-01..04`, and deterministic hierarchy/routing guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3I` and is the canonical source.
- Interrupt wiring integration-check contract (`interrupt_wiring_state_v1`, `interrupt_wiring_check_v1`, checks `INT-WIRE-01..04`, and deterministic wiring/linkage guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3J` and is the canonical source.
- Startup defaults + power-on register baseline contract (`startup_defaults_v1`, `power_on_register_baseline_v1`, checks `PWR-BASE-01..04`, and deterministic startup/baseline guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3K` and is the canonical source.
- Reset/startup sequence executor + verification checks contract (`startup_sequence_state_v1`, `startup_verification_event_v1`, checks `RST-SEQ-01..04`, and deterministic startup-sequence guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `11.3L` and is the canonical source.
- MFP timing and interrupt behavior
- ACIA + IKBD protocol behavior
- DMA + FDC + floppy media behavior
- PSG audio behavior

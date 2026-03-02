# Atari ST Cycle-Accurate Emulation Spec (v2)

This folder contains the cycle-accurate hardware specification split into cross-linked chapters for easier navigation and team review.

## Quick navigation

0. [Agent navigation graph (ultra-compact)](NAV_GRAPH.md)
0b. [Engine v2 implementation plan](../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md)
0c. [Engine v2 API specification](../EMU_ENGINE_V2_API_SPEC.md)
1. [Executive summary](01_executive_summary.md)
2. [System block diagram (textual)](02_system_block_diagram.md)
3. [Component-by-component specification table](03_component_spec_table.md)
4. [Full memory map and I/O map](04_memory_io_map.md)
5. [Timing chapter](05_timing.md)
6. [Storage/media chapter](06_storage_media.md)
7. [Input/peripherals chapter](07_input_peripherals.md)
8. [ROM/TOS chapter](08_rom_tos.md)
9. [Compatibility risk matrix](09_compatibility_risk_matrix.md)
10. [Validation plan](10_validation_plan.md)

## Agentic navigation manifest

Use this table as the primary machine-navigation source (stable IDs + explicit prev/next pointers).

| Doc ID | Title | Path | Prev | Next | Purpose |
|---|---|---|---|---|---|
| 01 | Executive summary | [01_executive_summary.md](01_executive_summary.md) | - | 02 | Defines measurable criteria for 100% compatibility and cycle accuracy |
| 02 | System block diagram | [02_system_block_diagram.md](02_system_block_diagram.md) | 01 | 03 | Defines chips, buses, clocks, DMA paths, and interrupt topology |
| 03 | Component specification table | [03_component_spec_table.md](03_component_spec_table.md) | 02 | 04 | Defines per-component behavior and cycle-accuracy requirements |
| 04 | Memory and I/O map | [04_memory_io_map.md](04_memory_io_map.md) | 03 | 05 | Defines address space, mirrors, undefined regions, and side effects |
| 05 | Timing chapter | [05_timing.md](05_timing.md) | 04 | 06 | Defines CPU/video/audio timing, jitter, and event ordering rules |
| 06 | Storage/media chapter | [06_storage_media.md](06_storage_media.md) | 05 | 07 | Defines floppy/FDC/DMA behavior, protections, and ACSI boot behavior |
| 07 | Input/peripherals chapter | [07_input_peripherals.md](07_input_peripherals.md) | 06 | 08 | Defines IKBD, ACIA, MIDI, serial, joystick, parallel, cartridge behavior |
| 08 | ROM/TOS chapter | [08_rom_tos.md](08_rom_tos.md) | 07 | 09 | Defines ROM/TOS dependencies, boot order, and region/version constraints |
| 09 | Compatibility risk matrix | [09_compatibility_risk_matrix.md](09_compatibility_risk_matrix.md) | 08 | 10 | Classifies cycle-perfect vs deterministic-high-level vs optional areas |
| 10 | Validation plan | [10_validation_plan.md](10_validation_plan.md) | 09 | A | Defines test categories, pass/fail criteria, and conformance checklist |
| A | Interrupt hierarchy appendix | [A_interrupt_hierarchy_vectors.md](A_interrupt_hierarchy_vectors.md) | 10 | B | Defines exact interrupt/vector ordering requirements |
| B | Startup/reset quirks appendix | [B_startup_reset_quirks.md](B_startup_reset_quirks.md) | A | C | Defines power-on defaults and undocumented software-relevant quirks |
| C | Model-delta appendix | [C_model_delta_summary.md](C_model_delta_summary.md) | B | D | Defines ST/Mega ST/STe/Mega STe model differences |
| D | Evidence closure appendix | [D_evidence_closure_requirements.md](D_evidence_closure_requirements.md) | C | - | Defines evidence requirements before claiming 100% |
| NAV | Agent navigation graph | [NAV_GRAPH.md](NAV_GRAPH.md) | - | 01 | Defines compact routing for automated doc traversal |

## Appendices

- [Appendix A: Interrupt hierarchy and vector behavior](A_interrupt_hierarchy_vectors.md)
- [Appendix B: Startup/reset/power-on assumptions and quirks](B_startup_reset_quirks.md)
- [Appendix C: Model-delta summary](C_model_delta_summary.md)
- [Appendix D: Evidence closure requirements](D_evidence_closure_requirements.md)

## Reference source

- [Monolithic original spec](ATARI_ST_CYCLE_ACCURATE_HARDWARE_SPEC.md)
- [Engine v2 implementation plan](../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md)
- [Engine v2 API specification](../EMU_ENGINE_V2_API_SPEC.md)

## Evidence legend

- **[V] Verified**: vendor docs/service docs or repeatable hardware tests
- **[I] Inferred**: reverse-engineered behavior requiring board-level validation
- **[R] Revision-specific**: depends on board/chip/TOS/region profile

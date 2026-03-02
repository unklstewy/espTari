# Agent Navigation Graph (Ultra-Compact)

**Purpose:** Fast routing map for agentic traversal across the emulation spec.

## Nodes

| ID | Path | Topic |
|---|---|---|
| 01 | [01_executive_summary.md](01_executive_summary.md) | Goals, compatibility definition, measurable success criteria |
| 02 | [02_system_block_diagram.md](02_system_block_diagram.md) | Chips, buses, clocks, interrupts, DMA paths |
| 03 | [03_component_spec_table.md](03_component_spec_table.md) | Per-component requirements and pitfalls |
| 04 | [04_memory_io_map.md](04_memory_io_map.md) | Address map, mirrors, undefined behavior |
| 05 | [05_timing.md](05_timing.md) | Cycle timing, contention, event ordering |
| 06 | [06_storage_media.md](06_storage_media.md) | Floppy/FDC/DMA/ACSI/media edge cases |
| 07 | [07_input_peripherals.md](07_input_peripherals.md) | IKBD, ACIA, MIDI, serial, joystick, parallel |
| 08 | [08_rom_tos.md](08_rom_tos.md) | ROM/TOS behavior and boot dependencies |
| 09 | [09_compatibility_risk_matrix.md](09_compatibility_risk_matrix.md) | Priority matrix for fidelity scope |
| 10 | [10_validation_plan.md](10_validation_plan.md) | Test strategy and conformance criteria |
| A | [A_interrupt_hierarchy_vectors.md](A_interrupt_hierarchy_vectors.md) | Interrupt and vector precision details |
| B | [B_startup_reset_quirks.md](B_startup_reset_quirks.md) | Power-on defaults and undocumented quirks |
| C | [C_model_delta_summary.md](C_model_delta_summary.md) | ST vs Mega ST vs STe vs Mega STe deltas |
| D | [D_evidence_closure_requirements.md](D_evidence_closure_requirements.md) | Evidence needed for 100% claim |

## Directed edges

- 01 -> 02 -> 03 -> 04 -> 05 -> 06 -> 07 -> 08 -> 09 -> 10 -> A -> B -> C -> D
- 05 -> A (timing rules feed interrupt hierarchy)
- 08 -> B (boot dependencies tied to startup defaults)
- 03 -> C (component model behavior varies by machine)
- 09 -> 10 (risk prioritization drives test plan)
- 10 -> D (validation output feeds evidence closure)

## Query-to-node routing

| Query intent | Start node |
|---|---|
| “What does 100% compatibility mean?” | 01 |
| “How are chips connected?” | 02 |
| “What must this chip emulate?” | 03 |
| “Where is this register or address range?” | 04 |
| “What is the exact event ordering or contention rule?” | 05 |
| “Why does this floppy/protected title fail?” | 06 |
| “How does IKBD/MIDI/ACIA timing work?” | 07 |
| “Why does this TOS version boot differently?” | 08 |
| “Can this be high-level instead of cycle-perfect?” | 09 |
| “How do we prove conformance?” | 10 |
| “Exact IRQ/vector corner case?” | A |
| “Power-on/reset quirk?” | B |
| “Model-specific difference?” | C |
| “What evidence is still missing?” | D |

## Minimal traversal algorithm

1. Classify user request into one query intent row.
2. Open mapped start node.
3. Follow linear edge chain unless a shortcut edge applies.
4. Stop when requirement is satisfied and cite final node(s).

<!-- NAV_META: doc=01; index=README.md; prev=-; next=02_system_block_diagram.md -->
[← Index](README.md) | [Next: System block diagram →](02_system_block_diagram.md)

# 1) Executive summary

**Purpose:** Define measurable success criteria for “100% compatibility + cycle-accurate behavior.”

## 1.1 Definition of “100% compatibility + cycle accuracy”

For this project, **100% compatibility** means any software that runs on original Atari ST-family hardware (in-scope models) must produce identical software-visible outcomes: same control flow, same interrupt sequencing, same media behavior, same video/audio temporal output, and same peripheral protocol behavior.

**Cycle accuracy** means all software-visible hardware interactions are reproduced at deterministic clock-step granularity so timing-sensitive software (demos, copy protection, trackers, MIDI sequencers, raster code) behaves identically.

## 1.2 Measurable acceptance targets

1. **Clock-step determinism**: chip state evolution is deterministic at master-clock-derived tick resolution.
2. **CPU-visible timing error budget**: 0 bus-slot mismatch for interrupt acknowledge, DMA contention, and exception entry ordering.
3. **Video phase accuracy**: register-write-to-pixel effect occurs on matching scanline and sub-line phase.
4. **Audio cadence accuracy**: PSG and STe DMA audio sample cadence drift = 0 over infinite deterministic replay.
5. **Storage protocol accuracy**: FDC/DMA command and DRQ/IRQ sequencing matches hardware traces for protected and non-protected media.
6. **Boot-state accuracy**: reset vectors, ROM overlay behavior, and early hardware defaults match model + TOS revision profile.

## 1.3 Required implementation philosophy for future engine

- Simulate **chips and buses**, not just APIs.
- Use **event ordering rules tied to clock domains**, not frame-based shortcuts.
- Maintain **model profiles** for ST, Mega ST, STe, Mega STe and selected chip/TOS revisions.
- Keep **undefined/open-bus** behavior deterministic and profile-selectable.

---

[← Index](README.md) | [Next: System block diagram →](02_system_block_diagram.md)

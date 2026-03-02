<!-- NAV_META: doc=06; index=README.md; prev=05_timing.md; next=07_input_peripherals.md -->
[← Index](README.md) | [← Previous: Timing chapter](05_timing.md) | [Next: Input/peripherals chapter →](07_input_peripherals.md)

# 6) Storage/media chapter

**Purpose:** Define floppy and ACSI behavior, including controller timing and protection-sensitive edge cases.

## 6.1 Floppy subsystem behavior

### 6.1.1 Required mechanical/logical model elements

- Motor spin-up/down timing windows.
- Head step rate and settle delay.
- Track/side position state.
- Index pulse rotational phase.
- Read/write gate and write splice timing.

### 6.1.2 Required controller behavior (FDC + DMA)

- Full command FSM and status flags.
- DRQ/INTRQ signal timing.
- Sector ID/gap/CRC handling.
- Multi-sector transfer pacing and terminal count behavior.
- Error/retry semantics visible to TOS and loaders.

### 6.1.3 Protection-sensitive edge cases

Must support deterministic representations for:

- Weak/fuzzy bits.
- Deliberate CRC errors.
- Overlong/short tracks.
- Illegal/duplicate sector IDs.
- Sync/address mark anomalies.
- Nonstandard rotational layouts.

## 6.2 ACSI/hard-disk behavior and boot interactions

- Implement command/status sequencing with realistic timeout windows.
- Respect boot probe ordering and device ID expectations.
- Emulate DMA transfer pacing and completion interrupt behavior.
- Preserve bus contention impact during active disk DMA.
- Ensure compatibility with early AHDI and boot-sector-driven loaders.

---

[← Index](README.md) | [← Previous: Timing chapter](05_timing.md) | [Next: Input/peripherals chapter →](07_input_peripherals.md)

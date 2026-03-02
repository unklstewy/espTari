<!-- NAV_META: doc=05; index=README.md; prev=04_memory_io_map.md; next=06_storage_media.md -->
[← Index](README.md) | [← Previous: Memory and I/O map](04_memory_io_map.md) | [Next: Storage/media chapter →](06_storage_media.md)

# 5) Timing chapter

**Purpose:** Define cycle-level timing, contention, latency/jitter bounds, and simultaneous-event ordering.

## 5.1 CPU cycle timing, wait states, bus contention

- Base cycle model must start from real 68000 bus cycle sequencing [V].
- Add external wait/hold effects from MMU arbitration, SHIFTER fetch, DMA/Blitter masters [V][R].
- Preserve prefetch queue visibility for self-modifying code, trap/interrupt entry, and cycle-exact loops.
- RMW instructions must lock bus sequence exactly as hardware does.
- Mega STe speed/cache mode transitions must update timing behavior immediately at documented boundaries [R].

## 5.2 Video timing: scanlines, blanking, sync, fetch windows

Required outputs:

1. Exact line and frame cadence for PAL/NTSC profiles [V][R].
2. Correct active display, border, and blanking windows [V].
3. SHIFTER fetch bursts at exact slot positions each line [V][I].
4. Mid-line register write effects (palette/mode/base changes) at correct phase [V][I].
5. STe-specific fine scroll/offset behavior integrated into fetch/visible timing [V][R].

## 5.3 Audio timing and mixing paths

- **PSG path**:
  - Tone/noise/envelope phase-accurate advancement.
  - Register write timing visible in waveform phase.
- **STe DMA path**:
  - Sample clock divisors exact.
  - Stereo channel and pointer wrap semantics exact.
  - End-of-buffer/loop events deterministic.
- **Mixer path**:
  - Microwire command shift/latch timing.
  - Deterministic combination of PSG + DMA output.

## 5.4 Interrupt latency and jitter requirements

- CPU IPL sampling at instruction boundaries per 68000 rules [V].
- MFP interrupt request, mask, and in-service transitions clock-accurate [V].
- VBL/HBL interrupt assertions aligned to line timing edges [V][R].
- Latency tolerance for compatibility target: effectively 0 bus-slot mismatch for timing-critical software.

## 5.5 Event ordering rules for simultaneous events

When two or more events become eligible in same master tick, resolve in deterministic order:

1. Reset/NMI/power-fail class events.
2. Bus fault-class events (BERR-adjacent model behavior).
3. Video timing edge transitions (line/frame boundaries).
4. DMA handshake edges (DRQ/terminal count completion).
5. MFP timer underflow and pending-bit updates.
6. ACIA byte completion edges.
7. CPU interrupt sampling/exception entry.

If board-level captures prove alternate ordering for a specific revision, override in that model profile [R].

---

[← Index](README.md) | [← Previous: Memory and I/O map](04_memory_io_map.md) | [Next: Storage/media chapter →](06_storage_media.md)

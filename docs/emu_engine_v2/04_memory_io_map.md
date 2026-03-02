<!-- NAV_META: doc=04; index=README.md; prev=03_component_spec_table.md; next=05_timing.md -->
[← Index](README.md) | [← Previous: Component table](03_component_spec_table.md) | [Next: Timing chapter →](05_timing.md)

# 4) Full memory map and I/O map

**Purpose:** Define complete address-space behavior, including mirrors, undefined areas, and register side effects.

## 4.1 Global memory map (24-bit physical)

> Note: exact mirror behavior differs by motherboard decode and TOS ROM size [R].

| Address range | Region | Behavior requirements |
|---|---|---|
| 0x000000-0x000007 | Reset vectors (initial SSP/PC fetch) | On reset, vector fetch must honor ROM overlay behavior [V][R] |
| 0x000000-(RAM_TOP-1) | ST-RAM | Shared between CPU/video/DMA/blitter; contention fully modeled [V] |
| RAM_TOP-implementation max | Unmapped RAM space | Bus error or deterministic open-bus profile [R] |
| 0xFA0000-0xFBFFFF | Cartridge ROM window (128 KB) | Read mapping + mirrors/profile behavior required [V][R] |
| 0xFC0000-0xFEFFFF | TOS ROM (common mapping window) | ROM size/revision may mirror/alias [R] |
| 0xFF0000-0xFFFFFF | High memory I/O + upper ROM decode interactions | I/O decode precedence and undefined-region behavior modeled [V][R] |

## 4.2 I/O map (core blocks)

| Address range (family-level) | Block | Required modeling |
|---|---|---|
| 0xFF8000-0xFF82FF | Video/MMU/SHIFTER control | Video base/counter, sync/mode, palette; STe extended controls where applicable |
| 0xFF8600-0xFF86FF | DMA + FDC/ACSI interface | DMA addr/count/control; command/status path timing and side effects |
| 0xFF8800-0xFF88FF | YM2149 PSG | Address latch + data semantics, GPIO side effects |
| 0xFF8900-0xFF89FF | STe DMA sound | Playback pointers, control, sample-rate divisors |
| 0xFF8920-0xFF892F | STe Microwire control | Serial command/latch behavior |
| 0xFF8A00-0xFF8AFF | Blitter registers | Full operation state and start/finish semantics |
| 0xFFFA00-0xFFFA3F | MFP 68901 | Full register map with true read/write side effects |
| 0xFFFC00-0xFFFC07 | ACIA pair (IKBD + MIDI) | Separate control/status/data channels |
| 0xFFFDxx (model dep.) | RTC / board-specific controls | Mega-class model profile required |
| Undecoded in 0xFFxxxx | Undefined I/O | Deterministic bus error/open-bus result by profile |

## 4.3 Mirrors, undefined regions, and side effects

Mandatory behavior classes:

1. **Decoded mirrors** [R]:
   - Partial decode in ASICs causes repeated register windows; emulate per profile.
2. **Write-only/read-as-last/open values** [R]:
   - Undefined reads may return last bus value, pull-up value, or device-specific patterns.
3. **Read side effects** [V]:
   - MFP/ACIA/FDC status accesses can clear flags or advance internal state.
4. **Byte-lane effects** [V][R]:
   - 8-bit peripheral registers on 16-bit bus may ignore wrong-lane writes.

---

[← Index](README.md) | [← Previous: Component table](03_component_spec_table.md) | [Next: Timing chapter →](05_timing.md)

<!-- NAV_META: doc=09; index=README.md; prev=08_rom_tos.md; next=10_validation_plan.md -->
[← Index](README.md) | [← Previous: ROM/TOS chapter](08_rom_tos.md) | [Next: Validation plan →](10_validation_plan.md)

# 9) Compatibility risk matrix

**Purpose:** Prioritize what must be cycle-perfect versus deterministic high-level or optional behavior.

| Category | Subsystems/features | Requirement level |
|---|---|---|
| Must be cycle-perfect | CPU bus + prefetch-visible behavior, GLUE/MMU/SHIFTER arbitration, MFP timer/IRQ sequencing, FDC+DMA handshake timing, Blitter arbitration, VBL/HBL edge timing, IKBD/ACIA byte timing | Non-negotiable for 100% claim |
| Can be high-level but deterministic | Analog filtering/monitor response nuances, non-critical audio coloration, RTC drift modeling, host file-backed media caching internals | Acceptable if externally deterministic and software-invisible |
| Optional for most software | Rare dongle edge cases, uncommon expansion board electrical quirks, niche cartridge sideband protocols | Implement as optional model extensions |

---

[← Index](README.md) | [← Previous: ROM/TOS chapter](08_rom_tos.md) | [Next: Validation plan →](10_validation_plan.md)

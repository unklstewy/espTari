<!-- NAV_META: doc=10; index=README.md; prev=09_compatibility_risk_matrix.md; next=A_interrupt_hierarchy_vectors.md -->
[← Index](README.md) | [← Previous: Compatibility risk matrix](09_compatibility_risk_matrix.md) | [Next: Appendix A →](A_interrupt_hierarchy_vectors.md)

# 10) Validation plan

**Purpose:** Define conformance tests, objective pass/fail signals, and completion checklist criteria.

## 10.1 Test categories

1. **CPU/exceptions**: 68000 trap/frame/prefetch timing test ROMs.
2. **Video/demo timing**: border, raster, split-palette, sync-sensitive demo tests.
3. **Audio**: PSG phase tests, STe DMA loop and rate tests, mixer control tests.
4. **Storage**: standard ST disks, malformed/protected images, ACSI boot/runtime tests.
5. **Input/peripherals**: IKBD command suite, mouse/joystick stress, MIDI throughput/jitter.
6. **TOS/GEM apps**: desktop, accessories, productivity apps across TOS variants.
7. **Model regression**: ST vs Mega ST vs STe vs Mega STe profile matrix.

## 10.2 Observable pass/fail criteria

- IRQ timeline traces match expected ordering and slot position.
- Video test patterns match scanline-phase effect points.
- FDC/DMA traces match command/DRQ/INTRQ sequences and completion timing.
- IKBD protocol transcript matches reference for command/reply ordering.
- MIDI stream tests show no timing-induced semantic errors.
- Boot behavior reproducible across cold/warm reset scenarios.

## 10.3 Suggested conformance checklist

- [ ] Reset/ROM overlay and vector behavior matches selected model+TOS profile.
- [ ] CPU exception timing and stack frame forms validated.
- [ ] MFP timer and interrupt nesting semantics validated.
- [ ] ACIA channels validated independently and under load.
- [ ] SHIFTER fetch windows and palette timing validated in all modes.
- [ ] DMA/FDC rotational and protection edge cases validated.
- [ ] Blitter bus arbitration validated (where present).
- [ ] STe DMA sound + Microwire behavior validated (STe/Mega STe).
- [ ] Undefined/open-bus behavior deterministic and profile-selectable.
- [ ] PAL/NTSC and region/TOS matrix passes representative software suites.

---

[← Index](README.md) | [← Previous: Compatibility risk matrix](09_compatibility_risk_matrix.md) | [Next: Appendix A →](A_interrupt_hierarchy_vectors.md)

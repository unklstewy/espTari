<!-- NAV_META: doc=08; index=README.md; prev=07_input_peripherals.md; next=09_compatibility_risk_matrix.md -->
[← Index](README.md) | [← Previous: Input/peripherals chapter](07_input_peripherals.md) | [Next: Compatibility risk matrix →](09_compatibility_risk_matrix.md)

# 8) ROM/TOS chapter

**Purpose:** Define ROM/TOS dependencies, boot-order requirements, and region/version compatibility constraints.

## 8.1 TOS/OS ROM interactions required by software

- Correct reset vector source and early ROM overlay mechanics [V][R].
- Accurate exception vector table usage from RAM after handover.
- Preserve TOS assumptions about DMA/FDC timing, MFP setup, IKBD initialization order.

## 8.2 Boot-sequence dependencies

Boot-time order must preserve:

1. CPU reset and initial vector fetch behavior.
2. Early memory sizing/tests dependent on contention timing.
3. Peripheral initialization order (MFP, ACIA, IKBD, storage).
4. Boot media probing order and timeout behavior.

## 8.3 Region/version compatibility considerations

- Support at minimum representative TOS sets across ST-era and STe-era branches.
- Distinguish PAL/NTSC timing profiles.
- Distinguish keyboard/locale assumptions affecting IKBD and TOS behavior.
- Expose per-profile ROM size/decode mirror differences [R].

---

[← Index](README.md) | [← Previous: Input/peripherals chapter](07_input_peripherals.md) | [Next: Compatibility risk matrix →](09_compatibility_risk_matrix.md)

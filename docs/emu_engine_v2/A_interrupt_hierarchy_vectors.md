<!-- NAV_META: doc=A; index=README.md; prev=10_validation_plan.md; next=B_startup_reset_quirks.md -->
[← Index](README.md) | [← Previous: Validation plan](10_validation_plan.md) | [Next: Appendix B →](B_startup_reset_quirks.md)

# Appendix A: Exact interrupt hierarchy and vector behavior requirements

**Purpose:** Pin down interrupt/vector ordering and acknowledge semantics for cycle-accurate exception behavior.

1. CPU interrupt priority evaluation must follow 68000 IPL rules.
2. MFP internal priority resolver and vector base behavior must match MC68901 behavior.
3. Simultaneous interrupt source assertions must follow deterministic chip-level event order (see [Timing chapter](05_timing.md)).
4. Interrupt acknowledge cycles must have correct bus timing and side effects.
5. Exception overlap cases (interrupt during trace, bus/access fault windows, stacked exceptions) must use real 68000 ordering.

---

[← Index](README.md) | [← Previous: Validation plan](10_validation_plan.md) | [Next: Appendix B →](B_startup_reset_quirks.md)

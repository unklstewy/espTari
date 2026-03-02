<!-- NAV_META: doc=B; index=README.md; prev=A_interrupt_hierarchy_vectors.md; next=C_model_delta_summary.md -->
[← Index](README.md) | [← Previous: Appendix A](A_interrupt_hierarchy_vectors.md) | [Next: Appendix C →](C_model_delta_summary.md)

# Appendix B: Startup/reset/power-on assumptions and undocumented quirks

**Purpose:** Define power-on defaults and undocumented startup quirks that software relies on.

Mandatory profile fields (per model/revision):

- Power-on register defaults for MFP, ACIA, PSG, DMA/FDC interface.
- Initial video mode and sync defaults.
- IKBD reset response timing and default mode.
- ROM overlay enable/disable transition point.
- Open-bus default pattern assumptions.
- Warm reset vs cold reset differences.

Undocumented but software-relevant quirks to preserve where verified [R]:

- Register write timing races during early VBL/HBL windows.
- DMA completion edge behavior in command boundary conditions.
- MFP pending/in-service interactions under rapid reassertion.
- Model/revision-specific mirror decode oddities.

---

[← Index](README.md) | [← Previous: Appendix A](A_interrupt_hierarchy_vectors.md) | [Next: Appendix C →](C_model_delta_summary.md)

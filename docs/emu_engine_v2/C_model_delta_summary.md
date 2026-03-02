<!-- NAV_META: doc=C; index=README.md; prev=B_startup_reset_quirks.md; next=D_evidence_closure_requirements.md -->
[← Index](README.md) | [← Previous: Appendix B](B_startup_reset_quirks.md) | [Next: Appendix D →](D_evidence_closure_requirements.md)

# Appendix C: Model-delta summary

**Purpose:** Summarize hardware and behavior differences between ST, Mega ST, STe, and Mega STe.

## C.1 520ST / 1040ST baseline

- Core: 68000 + GLUE/MMU/SHIFTER + MFP + ACIAx2 + IKBD + PSG + DMA/FDC.
- No STe DMA audio or Microwire features.
- Blitter may be absent/present depending on board/upgrades.

## C.2 Mega ST

- Baseline ST compatibility plus Mega-specific board integrations (RTC/model options).
- Blitter commonly present (profile-dependent by exact unit/revision).

## C.3 STe

- Enhanced video controls (fine scroll/extended palette behavior).
- DMA sound and Microwire-controlled mixer path.
- Additional registers and timing interactions in video/audio subsystems.

## C.4 Mega STe

- STe-class multimedia features plus speed/cache and board-control differences.
- Requires explicit model profile for CPU speed mode transitions and cache behavior.

---

[← Index](README.md) | [← Previous: Appendix B](B_startup_reset_quirks.md) | [Next: Appendix D →](D_evidence_closure_requirements.md)

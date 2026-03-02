# 4.1 EBIN concept

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 06

## 4.1 EBIN concept

An **EBIN** is a versioned, signed (recommended) binary module stored on SD-card and loaded by the v2 runtime.

EBIN module categories (Atari ST first profile):

- CPU module (68000 core contract)
- GLUE/MMU/SHIFTER module group
- MFP module
- ACIA/IKBD module group
- DMA/FDC module group
- PSG module
- Machine profile module (wiring + model defaults)
